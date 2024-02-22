%生成工程后，为了使编译顺利，需要：1将字符集改为unicode， 2：删除命令行中的编译参数%

@echo off
rd /s /q cgo-vsproject
rem mkdir cgo-vsproject
rem cd cgo-vsproject
D:\programfiles\vs2022\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe . -G "Visual Studio 17 2022" -B cgo-vsproject
pause