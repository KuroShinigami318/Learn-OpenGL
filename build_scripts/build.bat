@echo off
set config=%1
if not "%1" == "Debug" (
	if not "%1" == "Release" (
		call :MakeError "PLease provide config build type: Debug | Release. For example: build.bat Debug"
	)
        set config=Release
)
if not exist "../libs" (
	call repo_init
)
if not exist "../build" (
	call gen_prj
)
echo Building with config %config%
pushd "../build"
cmake -DCMAKE_BUILD_TYPE=%config% ..
popd
cmake --build ../build --config "%config%"
pause
goto :EOF

:MakeError
if "%~1" == "" (
   goto :EOF
)
echo "%~1"
if "%~2" == "" (
   goto :EOF
)
%~2
goto :EOF