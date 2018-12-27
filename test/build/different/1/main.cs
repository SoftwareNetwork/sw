// A Hello World! program in C#.
using System;

namespace HelloWorld
{
    class Hello
    {
        static void Main()
        {
            One one = new One();
            Two two = new Two();

            Console.WriteLine("Hello World! {0}", one.f() + two.f());
        }
    }
}
