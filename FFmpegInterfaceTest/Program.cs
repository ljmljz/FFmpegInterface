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

            FFmpegInterface.init();
            int ret = FFmpegInterface.prepare(uri);
            //play();

            Console.WriteLine("Prepareing");
            for(;;)
            {
                status = FFmpegInterface.get_status();
                if (status == FFmpegInterface.FFStatus.FF_STATUS_PREPARED)
                {
                    FFmpegInterface.play();
                    Console.WriteLine("Playing");
                }
            }
        }
    }
}
