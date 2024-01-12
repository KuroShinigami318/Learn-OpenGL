@echo off &setlocal
call repo_config.bat
set encryptedapikey="01000000d08c9ddf0115d1118c7a00c04fc297eb010000004f46a413446fe345b03a80e4d94b727800000000020000000000106600000001000020000000fb1f2d4090c0ce009e6ef545cd9cc06975bce91be888e5c6ecba6ce09ec922a1000000000e80000000020000200000004f6d8398e87b4a839b9289546e9a0797126284d004f8dd337278bdb77b0f47d1c0000000401eaecdf836147af43829e1bd7f488629ab9a64b4d431930fcf8ffbf9edbb168fcf40b43c51dd7bdedf6b5410f70b47d1f3f81d3553504248baec250c040ad091f4df5efa45eaa827ced47b1a94953de5232b4f5e6f489b32ebc32558af072089cf41c7e79958c9dc810bfe951d7ceb9fb3c88d05eac16b75f4093b23d933dc6f61a089c3612f77e27b1882873636ca6a3bbc4377125f9a0443e8306a0249117c5283d58feb857487856da93fb7cd97d8476c555b0b9d2d852d3c10a3ae080a400000003dbf5d600351707e819b40a68f416ca937b8e873e6e0819b433545b787f5d85e810a73088961f519c919d9aca216188ce17b536732333cb2d7b7a07ee4d34e00"

call Decrypt %encryptedapikey% apikey

curl -L -H "Accept: application/vnd.github+json" -H "Authorization: Bearer %apikey%" -H "X-GitHub-Api-Version: %github_api_version%" --output "temp.zip" https://api.github.com/repos/KuroShinigami318/%repo%/zipball/%release_tag%
tar -xf temp.zip

rem find full folder name
rem https://stackoverflow.com/questions/27829749/finding-a-directory-name-or-a-folder-name-in-batch-file
for /f %%a in ('dir *%repo%* /B /A:D') do if exist %%a (
rem todo: should change this to repo that will always has include and/or src folder
    rename %%a %libname%
)
rem todo: also move the lib-repo into libs folder without using these tricky.
if not exist %libname% (
   echo "%libname% not found"
   goto :EOF
)
if exist "../libs/%libname%" rmdir "../libs/%libname%" /s /q
if not exist "../libs" (
   mkdir "../libs"
)
move %libname% "../libs"
del temp.zip &endlocal