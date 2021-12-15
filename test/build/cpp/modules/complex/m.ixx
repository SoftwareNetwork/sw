export module MyModule;

import std.core;


export void MyFunc();
export
void f() {
    std::cout << "123\n";
    MyFunc();
}

MY_API void exp1(){}
export MY_API void exp2(){}


export void ggg2(){}
export void ggg3(){}
export void ggg4(){}
