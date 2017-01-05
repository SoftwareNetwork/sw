@echo off
setlocal
IF NOT EXIST tools cd ..
cppan
cmake -H. -Bwin32_cppan -DUSE_CPPAN_BUILD=1 -DCPPAN_BUILD_SHARED_LIBS=1
