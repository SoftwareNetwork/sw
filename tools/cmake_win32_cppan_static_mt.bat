@echo off
setlocal
IF NOT EXIST tools cd ..
cppan
cmake -H. -Bwin32_cppan_static_mt -DUSE_CPPAN_BUILD=1 -DMSVC_STATIC_RUNTIME=1
