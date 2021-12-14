export module MyModule;

import std.core;


export void MyFunc();
export
void f() {
    std::cout << "123\n";
    MyFunc();
}

__declspec(dllexport) void exp1(){}
export __declspec(dllexport) void exp2(){}


export void ggg2(){}
export void ggg3(){}
export void ggg4(){}
