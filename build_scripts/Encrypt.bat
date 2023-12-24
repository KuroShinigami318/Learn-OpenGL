@echo off &setlocal
rem https://www.dostips.com/forum/viewtopic.php?t=9420
set password=%1

setlocal EnableDelayedExpansion
for /f %%i in (
 'powershell -nop -ep bypass -c "ConvertFrom-SecureString -SecureString (ConvertTo-SecureString '!password:'=''!' -AsPlainText -Force)"'
) do endlocal &set encrypted=%%i

echo %encrypted%
pause