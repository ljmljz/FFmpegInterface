#pragma once
#pragma warning(disable:4996)

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>
#include <libavutil/avstring.h>
#include <libavutil/pixfmt.h>
#include <libavutil/log.h>

#include <SDL.h>
#include <SDL_thread.h>
#ifdef __cplusplus
};
#endif // __cplusplus

#pragma comment(lib, "avcodec.lib")  
#pragma comment(lib, "avformat.lib")  
#pragma comment(lib, "avdevice.lib")  
#pragma comment(lib, "avfilter.lib")  
#pragma comment(lib, "avutil.lib")  
#pragma comment(lib, "postproc.lib")  
#pragma comment(lib, "swresample.lib")  
#pragma comment(lib, "swscale.lib")

#pragma comment(lib, "SDL.lib")

#ifdef _WIN32
#define DLL_EXPORT	__declspec(dllexport)
#define FFMPEG_API	__stdcall
#else
#define DLL_EXPORT
#define FFMPEG_API	
#endif // _WIN32


#define SDL_AUDIO_BUFFER_SIZE 1024
#define MAX_AUDIOQ_SIZE (1 * 1024 * 1024)
#define FF_ALLOC_EVENT   (SDL_USEREVENT)
#define FF_REFRESH_EVENT (SDL_USEREVENT + 1)
#define FF_QUIT_EVENT (SDL_USEREVENT + 2)

#define AVCODEC_MAX_AUDIO_FRAME_SIZE 192000

typedef enum {
	ERROR_NONE = 0x0000,

	ERROR_SDL_INIT,

	ERROR_OPEN_FILE,
	ERROR_FIND_STREAM_INFO,
	ERORR_FIND_AUDIO_STREAM,
	ERROR_UNSUPPORTED_CODEC,
	ERROR_OPEN_CODEC,

	ERROR_INVALID_SAMPLE_RATE_CHANNEL,
	ERROR_SDL_OPEN_AUDIO,
} ErrorType;

typedef enum {
	FF_EVENT_NOP = SDL_USEREVENT, // interface test message
	FF_EVENT_PREPARED,
	FF_EVENT_PLAYBACK_COMPLETE,
	FF_EVENT_BUFFERING_UPDATE,
	FF_EVENT_SEEK_COMPLETE,
	FF_EVENT_STARTED,
	FF_EVENT_PAUSED,
	FF_EVENT_ERROR,
} EventType;

typedef enum {
	FF_STATUS_IDLE = 0,
	FF_STATUS_INITIALIZED,
	FF_STATUS_PREPARING,
	FF_STATUS_PREPARED,
	FF_STATUS_PLAYING,
	FF_STATUS_PAUSED,
	FF_STATUS_STOPPED,
	FF_STATUS_COMPLETED,
	FF_STATUS_ERROR = 100
} StatusType;

typedef struct PacketQueue {
	AVPacketList *first_pkt, *last_pkt;
	int nb_packets;
	int size;
	SDL_mutex *mutex;
	SDL_cond *cond;
} PacketQueue;

typedef struct FFmpegState {
	char            filename[1024];
	AVFormatContext *aFormatCtx;
	AVCodecContext *aCodecCtx;
	int             audioStream;
	AVStream        *audio_st;
	AVFrame         *aFrame;
	PacketQueue     audioq;
	unsigned int    audio_buf_size;
	unsigned int    audio_buf_index;
	AVPacket        audio_pkt;
	uint8_t         *audio_pkt_data;
	int             audio_pkt_size;
	uint8_t         *audio_buf;
	uint8_t         *audio_buf1;
	DECLARE_ALIGNED(16, uint8_t, audio_buf2)[AVCODEC_MAX_AUDIO_FRAME_SIZE * 4];
	enum AVSampleFormat  audio_src_fmt;
	enum AVSampleFormat  audio_tgt_fmt;
	int             audio_src_channels;
	int             audio_tgt_channels;
	int64_t         audio_src_channel_layout;
	int64_t         audio_tgt_channel_layout;
	int             audio_src_freq;
	int             audio_tgt_freq;
	struct SwrContext *swr;
	SDL_Thread      *decode_thread;
	SDL_Thread      *prepare_thread;
	SDL_Thread		*event_thread;
	int				quit;
	int				status;
	int				error;
} FFmpegState;


#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

	void packet_queue_init(PacketQueue *q);
	int packet_queue_put(PacketQueue *q, AVPacket *pkt);
	static int packet_queue_get(PacketQueue *q, AVPacket *pkt, int block);
	static void packet_queue_flush(PacketQueue *q);

	int DLL_EXPORT FFMPEG_API init();
	void DLL_EXPORT FFMPEG_API release();
	int DLL_EXPORT FFMPEG_API prepare(char *uri);
	void DLL_EXPORT FFMPEG_API play();
	int DLL_EXPORT FFMPEG_API get_status();

	int audio_decode_frame(FFmpegState *st);
	int prepare_from_thread(void *userdata);
	static int decode_from_thread(void *userdata);

	void audio_callback(void *userdata, Uint8 *stream, int len);

#ifdef __cplusplus
};
#endif // __cplusplus

int poll_event(void *userdata);