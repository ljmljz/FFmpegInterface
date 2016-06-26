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

            byte[] waveBuffer = new byte[8192];
            UInt16 actualSize = 0;
            StringBuilder sb = new StringBuilder();

            FFmpegInterface.init();
            FFmpegInterface.setWaveDataBuffer(waveBuffer, 8192, ref actualSize);
            int ret = FFmpegInterface.prepare(uri);
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
