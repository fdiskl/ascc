@echo off
setlocal enabledelayedexpansion

set CC=gcc
set CFLAGS_COMMON=-Wall -Wextra
set LDFLAGS=

:: default debug build
if "%1"=="" set BUILD=debug
if not "%1"=="" set BUILD=%1

if "%BUILD%"=="debug" (
    set CFLAGS=%CFLAGS_COMMON% -g -O0
    set BINDIR=build\debug
) else if "%BUILD%"=="release" (
    set CFLAGS=%CFLAGS_COMMON% -O3 -DNDEBUG
    set BINDIR=build\release
) else (
    echo Unknown build type: %BUILD%
    exit /b 1
)

set OBJDIR=%BINDIR%\obj
set BIN=%BINDIR%\ascc.exe

:: get src
set SRC=
for %%f in (src\*.c) do (
    set SRC=!SRC! %%f
)

:: compile
set OBJ=
for %%f in (src\*.c) do (
    set fname=%%~nf
    set OBJ=!OBJ! %OBJDIR%\!fname!.o
)

if "%1"=="clean" (
    echo Cleaning build...
    rmdir /s /q build 2>nul
    exit /b 0
)

if "%1"=="count" (
    for /r src %%f in (*.c *.h) do (
        for /f %%l in ('find /c /v "" ^< "%%f"') do echo %%l "%%f"
    )
    exit /b 0
)

if "%1"=="gdb" (
    call %~f0 debug
    gdb %BIN%
    exit /b 0
)

echo Building in %BUILD% mode...

:: create dir
if not exist "%OBJDIR%" mkdir "%OBJDIR%"

:: compile each .c file
for %%f in (%SRC%) do (
    echo Building %%f
    %CC% %CFLAGS% -c %%f -o "%OBJDIR%\%%~nf.o"
    if errorlevel 1 exit /b 1
)

:: link
%CC% %OBJ% -o %BIN% %LDFLAGS%
if errorlevel 1 exit /b 1

echo done Output: %BIN%
