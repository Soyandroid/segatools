:: dockerbuild.bat
:: Version: 1.2.0
@echo off
setlocal enabledelayedexpansion

:: Static Environment Variables
set BUILD_OUTPUT_PATH=.
rem set COPY_ALL_BUILD_DIRECTORIES=True
rem set NO_REMOVE_OLD_BUILD_DIRECTORIES=True
set IMAGE_NAME=djhackers/segatools:v004


:: Main Execution
if not defined NO_REMOVE_OLD_BUILD_DIRECTORIES (
    echo Deleting old build directories...

    for %%D in (_build32, _build64, _zip) do (
        set OUTPUT_DIR_TO_DELETE=%BUILD_OUTPUT_PATH%\%%D

        if exist "!OUTPUT_DIR_TO_DELETE!" (
            echo Deleting build directory '!OUTPUT_DIR_TO_DELETE!'...
            rmdir /S /Q "!OUTPUT_DIR_TO_DELETE!"
        ) else (
            echo Build directory '!OUTPUT_DIR_TO_DELETE!' has already been deleted.
        )
    )
)

echo Building Docker image '%IMAGE_NAME%' using Docker...
docker build . -t %IMAGE_NAME%
if ERRORLEVEL 1 (
    goto failure
)

echo Mounting Docker image '%IMAGE_NAME%'...
for /F "tokens=* USEBACKQ" %%F IN (`docker create %IMAGE_NAME%`) DO (set CONTAINER_ID=%%F)
echo Mounted Docker image '%IMAGE_NAME%' as container '%CONTAINER_ID%'.

echo Copying built files from Docker container '%CONTAINER_ID%'...
if defined COPY_ALL_BUILD_DIRECTORIES (
    set BUILD_DIRS_TO_COPY=_build32, _build64, _zip
) else (
    set BUILD_DIRS_TO_COPY=_zip
)
for %%D in (%BUILD_DIRS_TO_COPY%) do (
    echo Copying build directory '%%D' to '%BUILD_OUTPUT_PATH%\%%D'...
    docker cp %CONTAINER_ID%:%%D %BUILD_OUTPUT_PATH%
)

echo Removing Docker container '%CONTAINER_ID%'...
docker rm %CONTAINER_ID% > nul
goto success

:failure
echo segatools Docker build FAILED!
goto finish

:success
echo segatools Docker build completed successfully.
goto finish

:finish
pause
