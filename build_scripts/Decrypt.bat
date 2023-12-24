@echo off &setlocal
rem https://www.dostips.com/forum/viewtopic.php?t=9420
set encrypted=%1
for /f "delims=" %%i in (
 'powershell -nop -ep bypass -c "[System.Net.NetworkCredential]::new('', (ConvertTo-SecureString -String '%encrypted%')).Password"'
) do set decrypted=%%i


endlocal & set %~2=%decrypted%