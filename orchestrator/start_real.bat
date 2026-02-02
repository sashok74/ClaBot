@echo off
set USE_REAL_AGENT=true
set CLAUDE_CODE_GIT_BASH_PATH=C:\Tools\Git\usr\bin\bash.exe
cd /d "%~dp0"
node dist/index.js
