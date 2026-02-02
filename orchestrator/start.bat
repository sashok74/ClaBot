@echo off
chcp 65001 >nul
cd /d "%~dp0"

echo ========================================
echo   ClaBot Orchestrator (Mock Server)
echo ========================================
echo.

:: Check if node_modules exists
if not exist "node_modules" (
    echo Installing dependencies...
    call npm install
    if errorlevel 1 (
        echo ERROR: npm install failed
        pause
        exit /b 1
    )
    echo.
)

:: Check if dist exists or source is newer
if not exist "dist\index.js" (
    echo Building TypeScript...
    call npm run build
    if errorlevel 1 (
        echo ERROR: Build failed
        pause
        exit /b 1
    )
    echo.
)

:: Start server
echo Starting server on http://localhost:3000
echo.
echo Endpoints:
echo   POST   /agent/create       - Create agent
echo   GET    /agent/:id/events   - SSE events
echo   POST   /agent/:id/query    - Send prompt
echo   GET    /agent/:id/status   - Get status
echo   POST   /agent/:id/interrupt- Interrupt
echo   DELETE /agent/:id          - Delete
echo   GET    /health             - Health check
echo.
echo Press Ctrl+C to stop
echo ========================================
echo.

node dist/index.js
