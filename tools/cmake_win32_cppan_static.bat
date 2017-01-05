@echo off
setlocal
IF NOT EXIST tools cd ..
cppan
cmake -H. -Bwin32_cppan_static -DUSE_CPPAN_BUILD=1
