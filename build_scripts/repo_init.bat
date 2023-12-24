@echo off &setlocal
call repo_config.bat
set encryptedapikey="01000000d08c9ddf0115d1118c7a00c04fc297eb0100000014bbe3df1d20774e9c0e2092d32a32020000000002000000000010660000000100002000000034df983427e7bfa2d19a094287bf9a45caae971aca2204149c9b5ecfef4fe710000000000e8000000002000020000000464f0507a7a8fa2e5d955e91ebf2f7e83514cddfa9e0ffaeb88684f0650aeaf6c00000003b9fdb7042cc63f09829ae98e7d8f9bfa9a7c997665e7452af7463437d160b224f52d0088bd92408f8c109082d1d5352536ea1a52a6debbbba78ec54d20c72d883d9a9fc22dd02e0c56bbdbd1e63462981c2359bc338f46477f5ccb3bb716c4962e7efb63501217aea613607f1c41ed2f20ca5e06a7de67c78b31e38e1222fa9a7c84e4b71f0cc991509ad6a5243db8323d936250ce11ebbd7cdd9dd20a14a1532e6798ffc532a645bc6b98291c329cc99eeafebfae7d0023ac28d5f97d1d07740000000de62b51501ca3c93b06ed01a973bf81fb52d33e734cc3bdb2f5519aba65a7e45420dc4b0c3baa0a1cd52bf78c33001712c767af8406e3b0602a0cbbc38f95b30"

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