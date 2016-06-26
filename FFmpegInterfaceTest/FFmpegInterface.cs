using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Runtime.InteropServices;

namespace FFmpegInterfaceTest
{
    class FFmpegInterface
    {
        public enum FFStatus
        {
            FF_STATUS_IDLE = 0,
            FF_STATUS_INITIALIZED,
            FF_STATUS_PREPARING,
            FF_STATUS_PREPARED,
            FF_STATUS_PLAYING,
            FF_STATUS_PAUSED,
            FF_STATUS_STOPPED,
            FF_STATUS_COMPLETED,
            FF_STATUS_ERROR = 100
        };

        [DllImport("FFmpegInterface")]
        public extern static int init();

        [DllImport("FFmpegInterface")]
        public extern static int prepare(StringBuilder uri);

        [DllImport("FFmpegInterface")]
        public extern static void play();

        [DllImport("FFmpegInterface")]
        public extern static FFStatus getStatus();

        [DllImport("FFmpegInterface")]
        public extern static int setWaveDataBuffer(byte[] buffer, UInt16 max_size, ref UInt16 actual_size);
    }
}
