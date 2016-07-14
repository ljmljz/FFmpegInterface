using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace FFmpegInterfaceTest
{
    class Program
    {
        static void Main(string[] args)
        {
            StringBuilder uri = new StringBuilder(255);
            FFmpegInterface.FFStatus status;
            uri.Append("..\\..\\..\\res\\yzcw.mp3");

            UInt16 bufferSize = 4608;

            byte[] waveBuffer = new byte[bufferSize];
            float[] leftFFTBuffer = new float[bufferSize];
            float[] rightFFTBuffer = new float[bufferSize];
            StringBuilder sb = new StringBuilder();
            int fftWindowSize = 0;

            FFmpegInterface.init();
            int ret = FFmpegInterface.setDataCaptureBuffer(waveBuffer, leftFFTBuffer, rightFFTBuffer, bufferSize);
            if(ret > 0)
            {
                fftWindowSize = ret;
            }

            FFmpegInterface.prepare(uri);
            //play();

            Console.WriteLine("Prepareing");
            for(;;)
            {
                status = FFmpegInterface.getStatus();
                switch(status)
                {
                    case FFmpegInterface.FFStatus.FF_STATUS_PREPARED:
                        FFmpegInterface.play();
                        Console.WriteLine("Playing");
                        break;
                    case FFmpegInterface.FFStatus.FF_STATUS_PLAYING:
                        
                        break;
                    case FFmpegInterface.FFStatus.FF_STATUS_ERROR:
                        break;
                    default:
                        break;
                }
            }
        }
    }
}
