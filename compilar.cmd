@echo off
rem Compila o port do Def Jam (Xbox) neste PC: Visual Studio em E:\Program Files\Visual Studio (bibliotecas do Windows),
rem Clang portatil em E:\Tools\llvm, CMake/Ninja do Visual Studio. Registro em out\compilar.log.
setlocal
cd /d "%~dp0"
call "E:\Program Files\Visual Studio\VC\Auxiliary\Build\vcvars64.bat" >nul
set "VSCMAKE=E:\Program Files\Visual Studio\Common7\IDE\CommonExtensions\Microsoft\CMake"
set "PATH=E:\Tools\llvm\bin;%VSCMAKE%\CMake\bin;%VSCMAKE%\Ninja;%PATH%"
if not exist out mkdir out
echo [1/3] configurando
cmake --preset release > out\compilar.log 2>&1 || goto :fail
findstr /b /l /c:"[symbols]" config\game.toml >nul 2>nul && goto :game
echo [2/3] simbolos do XDK
cmake --build out\build\release --target xbe_symbols >> out\compilar.log 2>&1 || goto :fail
set /p SDK=<out\build\release\sdk_dir.txt
"out\build\release\xbe_symbols.exe" assets\default.xbe "%SDK%\xdk" --out config\game.toml >> out\compilar.log 2>&1 || goto :fail
:game
echo [3/3] codigo do jogo
cmake --build out\build\release >> out\compilar.log 2>&1 || goto :fail
echo pronto: out\build\release\DefJamFFNY.exe
exit /b 0
:fail
echo FALHOU - veja out\compilar.log
exit /b 1
