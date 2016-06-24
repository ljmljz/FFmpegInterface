using System;
using System.Collections.Generic;
using System.Linq;
using System.Runtime.InteropServices;
using System.Text;
using System.Threading.Tasks;

namespace Test
{
    class Program
    {
        [DllImport("FFmpegInterface")]
        public extern static int init();

        [DllImport("FFmpegInterface")]
        public extern static int prepare(StringBuilder uri);

        [DllImport("FFmpegInterface")]
        public extern static void play();

        static void Main(string[] args)
        {
            StringBuilder uri = new StringBuilder(255);
            uri.Append("D:\\My Documents\\00.mp3");

            init();
            int ret = prepare(uri);
            play();

            Console.WriteLine("Playing");
            for(;;)
            {
                
            }
        }
    }
}
