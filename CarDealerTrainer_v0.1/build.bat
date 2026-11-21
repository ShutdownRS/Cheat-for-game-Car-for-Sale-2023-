@echo off
setlocal
set "VCVARS=C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat"
if not exist "%VCVARS%" (
  echo [!] vcvars64.bat not found. Edit VCVARS in this file to point at your Visual Studio.
  exit /b 1
)
call "%VCVARS%" >nul
cd /d "%~dp0"

echo [*] Building CarDealerCheat.dll ...
cl /nologo /LD /EHsc /O2 /MT /std:c++17 /Iinclude src\Cheat.cpp ^
   /Fe:bin\CarDealerCheat.dll /Fo:bin\cheat.obj ^
   /link /LIBPATH:lib libMinHook.x64.lib gdi32.lib user32.lib advapi32.lib
if errorlevel 1 exit /b 1

echo [*] Building Injector.exe ...
cl /nologo /EHsc /O2 /MT /std:c++17 src\Injector.cpp ^
   /Fe:bin\Injector.exe /Fo:bin\injector.obj ^
   /link kernel32.lib user32.lib
if errorlevel 1 exit /b 1

del /q bin\*.obj 2>nul
echo.
echo [+] Done. Files are in Cheat\bin
endlocal
