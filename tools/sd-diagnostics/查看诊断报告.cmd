@echo off
if exist "%LOCALAPPDATA%\WordcardDiagnostics" (
start "" "%LOCALAPPDATA%\WordcardDiagnostics"
) else (
echo No reports yet. Run the diagnostic first.
pause
)
