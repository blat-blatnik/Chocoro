rem # Compile for the web using Emscripten.
rem # Assumes that the Emscripten SDK (emsdk) is on you PATH.
rem # https://github.com/raysan5/raylib/wiki/Working-for-Web-(HTML5)

@echo off
setlocal EnableExtensions EnableDelayedExpansion

rem # Collect all .c files for compilation into the 'input' variable.
for /f %%a in ('forfiles /s /m *.c /c "cmd /c echo @relpath"') do set input=!input! "%%~a"

rem emsdk update
rem emsdk install latest
call emsdk activate latest
call emcc %input% -o bin/web/index.html -Oz -flto -Wall -Wextra -L./lib -lraylib_web -lidbfs.js -sUSE_GLFW=3 -sTOTAL_MEMORY=33554432 -sENVIRONMENT='web,webview' -sEXPORTED_FUNCTIONS=_StartGameLoop,_SyncFinished,_main --shell-file etc/webshell.html --preload-file res

copy /y "etc\icon.ico" "bin\web\favicon.ico"

rem # Make sure .data is smaller than 32MB.
set /a size=0
for /f "usebackq" %%A in ('bin/web/index.data') do set /a size=%size%+%%~zA
set /a size=%size%/1048576
echo.
echo .data is %size% MB (limit is 32).
if %size% geq 32 (
	echo.
	echo WARNING: .data is larger than 32 MB!
)

pause
rem # Run with: $ emrun bin/web/index.html
