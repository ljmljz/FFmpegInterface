// FFmpegTest.cpp : Defines the exported functions for the DLL application.
//


#include "stdafx.h"
#include "FFmpegInterface.h"

FFmpegState *state;

void packet_queue_init(PacketQueue *q) {
	memset(q, 0, sizeof(PacketQueue));
	q->mutex = SDL_CreateMutex();
	q->cond = SDL_CreateCond();
}

int packet_queue_put(PacketQueue *q, AVPacket *pkt) {
	AVPacketList *pkt1;

	pkt1 = (AVPacketList *)av_malloc(sizeof(AVPacketList));
	if (!pkt1) {
		return -1;
	}
	pkt1->pkt = *pkt;
	pkt1->next = NULL;

	SDL_LockMutex(q->mutex);

	if (!q->last_pkt) {
		q->first_pkt = pkt1;
	}
	else {
		q->last_pkt->next = pkt1;
	}

	q->last_pkt = pkt1;
	q->nb_packets++;
	q->size += pkt1->pkt.size;
	SDL_CondSignal(q->cond);
	SDL_UnlockMutex(q->mutex);
	return 0;
}

static int packet_queue_get(PacketQueue *q, AVPacket *pkt, int block) {
	AVPacketList *pkt1;
	int ret;

	SDL_LockMutex(q->mutex);

	for (;;) {
		if (state->quit) {
			ret = -1;
			break;
		}

		pkt1 = q->first_pkt;
		if (pkt1) {
			q->first_pkt = pkt1->next;
			if (!q->first_pkt) {
				q->last_pkt = NULL;
			}
			q->nb_packets--;
			q->size -= pkt1->pkt.size;
			*pkt = pkt1->pkt;

			av_free(pkt1);
			ret = 1;
			break;
		}
		else if (!block) {
			ret = 0;
			break;
		}
		else {
			SDL_CondWait(q->cond, q->mutex);
		}
	}

	SDL_UnlockMutex(q->mutex);

	return ret;
}

static void packet_queue_flush(PacketQueue *q) {
	AVPacketList *pkt, *pkt1;

	SDL_LockMutex(q->mutex);
	for (pkt = q->first_pkt; pkt != NULL; pkt = pkt1) {
		pkt1 = pkt->next;
		av_free_packet(&pkt->pkt);
		av_freep(&pkt);
	}
	q->last_pkt = NULL;
	q->first_pkt = NULL;
	q->nb_packets = 0;
	q->size = 0;
	SDL_UnlockMutex(q->mutex);
}

int DLL_EXPORT FFMPEG_API init() {

	// Allocate memory for the state.
	state = (FFmpegState *)av_mallocz(sizeof(FFmpegState));

	state->aFormatCtx = NULL;
	state->aCodecCtx = NULL;
	state->aFrame = NULL;

	state->audio_pkt_data = NULL;
	state->audio_buf = NULL;
	state->audio_buf1 = NULL;

	state->swr = NULL;

	state->quit = 0;
	state->status = FF_STATUS_IDLE;

	// Register all codec
	av_register_all();

	if (SDL_Init(SDL_INIT_AUDIO)) {
		state->error = ERROR_SDL_INIT;
		state->status = FF_STATUS_ERROR;

		return -1;
	}

	state->status = FF_STATUS_INITIALIZED;

	return 0;
}

int DLL_EXPORT FFMPEG_API prepare(char *uri) {
	if (!uri) {
		state->error = ERROR_OPEN_FILE;
		state->status = FF_STATUS_ERROR;
		return -1;
	}

	// Workaround for FFmpeg ticket #998
	// "must convert mms://... streams to mmsh://... for FFmpeg to work"
	char *restrict_to = strstr(uri, "mms://");
	if (restrict_to) {
		strncpy(restrict_to, "mmsh://", 6);
		puts(uri);
	}

	strncpy(state->filename, uri, sizeof(state->filename));

	state->prepare_thread = SDL_CreateThread(prepare_from_thread, state);
	//prepare_from_thread((void*)state);

	return 0;
}

int prepare_from_thread(void *userdata) {
	SDL_AudioSpec wanted_spec, spec;
	int64_t wanted_channel_layout = 0;
	int wanted_nb_channels;
	const int next_nb_channels[] = { 0, 0, 1 ,6, 2, 6, 4, 6 };

	FFmpegState *st = (FFmpegState*)userdata;

	// Open audio file
	if (avformat_open_input(&st->aFormatCtx, st->filename, NULL, NULL) != 0) {
		st->error = ERROR_OPEN_FILE;
		st->status = FF_STATUS_ERROR;
		return ERROR_OPEN_FILE;
	}

	// Retrieve stream information
	if (avformat_find_stream_info(st->aFormatCtx, NULL) < 0) {
		st->error = ERROR_FIND_STREAM_INFO;
		st->status = FF_STATUS_ERROR;
		return ERROR_FIND_STREAM_INFO;
	}

	// Find the first audio stream
	int i;
	st->audioStream = -1;
	for (i = 0; i < st->aFormatCtx->nb_streams; i++) {
		if (st->aFormatCtx->streams[i]->codec->codec_type == AVMEDIA_TYPE_AUDIO &&
			st->audioStream < 0) {
			st->audioStream = i;
		}
	}
	if (st->audioStream == -1) {
		st->error = ERORR_FIND_AUDIO_STREAM;
		st->status = FF_STATUS_ERROR;
		return ERORR_FIND_AUDIO_STREAM;
	}

	// Get a pointer to the codec context for the video stream
	st->aCodecCtx = st->aFormatCtx->streams[st->audioStream]->codec;

	// Find the decoder for the audio stream
	AVCodec *aCodec = avcodec_find_decoder(st->aCodecCtx->codec_id);
	if (!aCodec) {
		st->error = ERROR_UNSUPPORTED_CODEC;
		st->status = FF_STATUS_ERROR;
		return ERROR_UNSUPPORTED_CODEC;
	}

	if (avcodec_open2(st->aCodecCtx, aCodec, NULL) < 0) {
		st->error = ERROR_OPEN_CODEC;
		st->status = FF_STATUS_ERROR;
		return ERROR_OPEN_CODEC;
	}

	st->aFrame = av_frame_alloc();

	wanted_nb_channels = st->aCodecCtx->channels;
	if (!wanted_channel_layout || wanted_nb_channels != av_get_channel_layout_nb_channels(wanted_channel_layout)) {
		wanted_channel_layout = av_get_default_channel_layout(wanted_nb_channels);
		wanted_channel_layout &= ~AV_CH_LAYOUT_STEREO_DOWNMIX;
	}

	wanted_spec.channels = av_get_channel_layout_nb_channels(wanted_channel_layout);
	wanted_spec.freq = st->aCodecCtx->sample_rate;
	if (wanted_spec.freq <= 0 || wanted_spec.channels <= 0) {
		fprintf(stderr, "Invalid sample rate or channel count!\n");
		return -1;
	}
	wanted_spec.format = AUDIO_S16SYS;
	wanted_spec.silence = 0;
	wanted_spec.samples = SDL_AUDIO_BUFFER_SIZE;
	wanted_spec.callback = audio_callback;
	wanted_spec.userdata = st;

	while (SDL_OpenAudio(&wanted_spec, &spec) < 0) {
		fprintf(stderr, "SDL_OpenAudio (%d channels): %s\n", wanted_spec.channels, SDL_GetError());
		wanted_spec.channels = next_nb_channels[FFMIN(7, wanted_spec.channels)];
		if (!wanted_spec.channels) {
			fprintf(stderr, "No more channel combinations to tyu, audio open failed\n");
			return -1;
		}
		wanted_channel_layout = av_get_default_channel_layout(wanted_spec.channels);
	}

	if (spec.format != AUDIO_S16SYS) {
		fprintf(stderr, "SDL advised audio format %d is not supported!\n", spec.format);
		return -1;
	}
	if (spec.channels != wanted_spec.channels) {
		wanted_channel_layout = av_get_default_channel_layout(spec.channels);
		if (!wanted_channel_layout) {
			fprintf(stderr, "SDL advised channel count %d is not supported!\n", spec.channels);
			return -1;
		}
	}

	st->audio_src_fmt = st->audio_tgt_fmt = AV_SAMPLE_FMT_S16;
	st->audio_src_freq = st->audio_tgt_freq = spec.freq;
	st->audio_src_channel_layout = st->audio_tgt_channel_layout = wanted_channel_layout;
	st->audio_src_channels = st->audio_tgt_channels = spec.channels;

	// 设置格式转换
	if (st->swr) swr_free(&st->swr);
	st->swr = swr_alloc_set_opts(NULL,
		st->audio_tgt_channel_layout,
		st->audio_tgt_fmt,
		st->audio_tgt_freq,
		st->aCodecCtx->channel_layout,
		st->aCodecCtx->sample_fmt,
		st->aCodecCtx->sample_rate,
		0, NULL);
	if (!st->swr || swr_init(st->swr) < 0) {
		fprintf(stderr, "swr_init() failed\n");
		return -1;
	}

	st->aFormatCtx->streams[st->audioStream]->discard = AVDISCARD_DEFAULT;
	st->audio_st = st->aFormatCtx->streams[st->audioStream];
	st->audio_buf_size = 0;
	st->audio_buf_index = 0;
	memset(&st->audio_pkt, 0, sizeof(st->audio_pkt));
	packet_queue_init(&st->audioq);

	// Raise Prepared event
	//push_event(FF_EVENT_PREPARED, st);
	// 返回sample rate和channels
	//st->sample_rate = st->aCodecCtx->sample_rate;
	//st->channels = st->aCodecCtx->channels;

	state->decode_thread = SDL_CreateThread(decode_from_thread, state);

	st->status = FF_STATUS_PREPARED;
	//audio->event_callback(MEDIA_PREPARED, NONE, NONE);

	return 0;
}

static int decode_from_thread(void *userdata) {
	int ret = -1;
	AVPacket pkt1, *packet = &pkt1;
	FFmpegState *st = (FFmpegState*)userdata;

	while (1) {
		if (st->quit) break;
		if (st->audioq.size > MAX_AUDIOQ_SIZE) {
			SDL_Delay(10);
			continue;
		}
		ret = av_read_frame(st->aFormatCtx, packet);
		if (ret < 0) {
			if (ret == AVERROR_EOF || url_feof(st->aFormatCtx->pb)) {
				break;
			}
			if (st->aFormatCtx->pb && st->aFormatCtx->pb->error) {
				break;
			}
			continue;
		}

		if (packet->stream_index == st->audioStream) {
			packet_queue_put(&st->audioq, packet);
		}
		else {
			av_free_packet(packet);
		}
	}

	while (!st->quit) {
		SDL_Delay(100);
	}

	return 0;
}

int audio_decode_frame(FFmpegState *st) {
	int len1, len2, decoded_data_size;
	AVPacket *pkt = &st->audio_pkt;
	int got_frame = 0;
	int64_t dec_channel_layout;
	int wanted_nb_samples, resampled_data_size;

	for (;;) {
		while (st->audio_pkt_size > 0) {
			if (!st->aFrame) {
				if (!(st->aFrame = av_frame_alloc())) {
					return AVERROR(ENOMEM);
				}
			}
			//else
			//    avcodec_get_frame_defaults(st->aFrame);

			len1 = avcodec_decode_audio4(st->audio_st->codec, st->aFrame, &got_frame, pkt);
			if (len1 < 0) {
				// error, skip the frame
				st->audio_pkt_size = 0;
				break;
			}

			st->audio_pkt_data += len1;
			st->audio_pkt_size -= len1;

			if (!got_frame)
				continue;

			decoded_data_size = av_samples_get_buffer_size(NULL,
				st->aFrame->channels,
				st->aFrame->nb_samples,
				(AVSampleFormat)st->aFrame->format, 1);

			dec_channel_layout = (st->aFrame->channel_layout && st->aFrame->channels
				== av_get_channel_layout_nb_channels(st->aFrame->channel_layout))
				? st->aFrame->channel_layout
				: av_get_default_channel_layout(st->aFrame->channels);

			wanted_nb_samples = st->aFrame->nb_samples;

			//fprintf(stderr, "wanted_nb_samples = %d\n", wanted_nb_samples);
			if (st->swr) {
				// const uint8_t *in[] = { is->audio_frame->data[0] };
				const uint8_t **in = (const uint8_t **)st->aFrame->extended_data;
				uint8_t *out[] = { st->audio_buf2 };
				if (wanted_nb_samples != st->aFrame->nb_samples) {
					if (swr_set_compensation(st->swr, (wanted_nb_samples - st->aFrame->nb_samples)
						* st->audio_tgt_freq / st->aFrame->sample_rate,
						wanted_nb_samples * st->audio_tgt_freq / st->aFrame->sample_rate) < 0) {
						fprintf(stderr, "swr_set_compensation() failed\n");
						break;
					}
				}

				// SWR Convert
				len2 = swr_convert(st->swr, out,
					sizeof(st->audio_buf2)
					/ st->audio_tgt_channels
					/ av_get_bytes_per_sample(st->audio_tgt_fmt),
					in, st->aFrame->nb_samples);
				if (len2 < 0) {
					fprintf(stderr, "swr_convert() failed\n");
					break;
				}

				if (len2 == sizeof(st->audio_buf2) / st->audio_tgt_channels / av_get_bytes_per_sample(st->audio_tgt_fmt)) {
					fprintf(stderr, "warning: audio buffer is probably too small\n");
					swr_init(st->swr);
				}

				st->audio_buf = st->audio_buf2;
				resampled_data_size = len2 * st->audio_tgt_channels * av_get_bytes_per_sample(st->audio_tgt_fmt);
			}
			else {
				resampled_data_size = decoded_data_size;
				st->audio_buf = st->aFrame->data[0];
			}

			// Update the wave buffer
			if (st->wave) {
				update_wave_buffer(st->audio_buf, resampled_data_size);
			}

			// We have data, return it and come back for more later
			return resampled_data_size;
		}

		// Read packet from queue
		if (pkt->data) av_free_packet(pkt);
		memset(pkt, 0, sizeof(*pkt));
		if (st->quit) return -1;
		if (packet_queue_get(&st->audioq, pkt, 1) < 0) return -1;

		st->audio_pkt_data = pkt->data;
		st->audio_pkt_size = pkt->size;
	}
}

void audio_callback(void *userdata, Uint8 *stream, int len) {
	FFmpegState *st = (FFmpegState *)userdata;
	int len1, audio_data_size;

	while (len > 0) {
		if (st->audio_buf_index >= st->audio_buf_size) {
			audio_data_size = audio_decode_frame(st);

			if (audio_data_size < 0) {
				/* silence */
				st->audio_buf_size = 1024;
				memset(st->audio_buf, 0, st->audio_buf_size);
			}
			else {
				st->audio_buf_size = audio_data_size;
			}
			st->audio_buf_index = 0;
		}

		len1 = st->audio_buf_size - st->audio_buf_index;
		if (len1 > len) {
			len1 = len;
		}

		memcpy(stream, (uint8_t *)st->audio_buf + st->audio_buf_index, len1);
		len -= len1;
		stream += len1;
		st->audio_buf_index += len1;
	}
}

void DLL_EXPORT FFMPEG_API play() {
	SDL_PauseAudio(0);
	state->status = FF_STATUS_PLAYING;
}

int DLL_EXPORT FFMPEG_API getStatus()
{
	return state->status;
}

void DLL_EXPORT FFMPEG_API release()
{
	state->quit = 1;
	SDL_Quit();

	free(state->wave->tmp_buffer);
	free(state->wave);

	av_free(state);
}

int DLL_EXPORT FFMPEG_API setWaveDataBuffer(uint8_t *wave, unsigned int max_size, unsigned int *actual_size)
{
	if (!wave || max_size <= 0 || !actual_size) {
		return -1;
	}

	state->wave = (WaveBuffer*)malloc(sizeof(WaveBuffer));
	state->wave->out_buffer = wave;
	state->wave->tmp_buffer = (uint8_t*)malloc(max_size);
	state->wave->max_size = max_size;
	state->wave->out_size = actual_size;
	state->wave->index = 0;

	state->wave->mutex = SDL_CreateMutex();

	return 0;
}

void update_wave_buffer(uint8_t *buffer, unsigned int size) {
	SDL_LockMutex(state->wave->mutex);

	if (state->wave->index + size > state->wave->max_size) {
		memset(state->wave->out_buffer, 0, state->wave->max_size);
		memcpy(state->wave->out_buffer, state->wave->tmp_buffer, state->wave->index);
		*state->wave->out_size = state->wave->index;

		memset(state->wave->tmp_buffer, 0, state->wave->max_size);
		memcpy(state->wave->tmp_buffer, buffer, size);
		state->wave->index = size;
	}
	else {
		memcpy((uint8_t*)state->wave->tmp_buffer + state->wave->index, buffer, size);
		state->wave->index += size;
	}

	SDL_UnlockMutex(state->wave->mutex);
}
