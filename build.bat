@echo off

setlocal enabledelayedexpansion

call "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars64.bat"

cmake -S . -B build -G Ninja -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang
IF ERRORLEVEL 1 EXIT /B %ERRORLEVEL%

cmake --build build
IF ERRORLEVEL 1 EXIT /B %ERRORLEVEL%

ctest --test-dir build --output-on-failure
IF ERRORLEVEL 1 EXIT /B %ERRORLEVEL%
