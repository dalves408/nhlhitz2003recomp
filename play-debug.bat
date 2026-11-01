@echo off
rem Use the latest Release build with the project's integrated xrdbg debugger.
call "%~dp0play.bat" --debug %*
exit /b %ERRORLEVEL%
