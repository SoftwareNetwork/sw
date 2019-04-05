MY_A_API int a(int);
MY_B_API int b(int aa)
{
    return a(aa) * 2;
}
