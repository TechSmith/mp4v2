@ECHO OFF

@REM Set paths that are independent of "bitness".
SET CMAKE_PATH=C:\Program Files\CMake 2.8\bin\cmake.exe
SET VS_PATH=C:\Program Files\Microsoft Visual Studio 10.0\Common7\IDE\devenv.com
SET SVN_PATH=C:\Program Files\TortoiseSVN\bin\svn.exe
SET TRUNK_PATH=%~1
SET TF_PATH=C:\Program Files\Microsoft Visual Studio 10.0\Common7\IDE\tf.exe
SET TFPT_PATH=C:\Program Files\Microsoft Team Foundation Server 2010 Power Tools\tfpt.exe
SET TFS_PATH=D:\Projects_TFS\Common\mp4v2\Development
SET DIR_SYMSTORE=C:\Program Files\Debugging Tools for Windows (x86)

SET MP4V2_INCPATH=%TRUNK_PATH%\include\mp4v2
SET MP4V2_SRCPATH=%TRUNK_PATH%\src
SET MP4V2_BINPATH=%TRUNK_PATH%\bin\win
SET MP4V2_LIBPATH=%TRUNK_PATH%\lib\win

@ECHO ON
@REM Update files from TFS, so that it thinks we have the latest code when we make our check-in later
"%TF_PATH%" get "%TFS_PATH%"

@REM Delete all the files in the TFS path so we can tell if a file was deleted in SVN
del "%TFS_PATH%" /S /F /Q
@ECHO OFF

@REM Do the 32-bit builds.
SET BITS=32
SET PLATFORM=Win%BITS%
SET BUILD_PATH=%TRUNK_PATH%\build%BITS%\
SET MP4V2_BUILD_BINPATH=%TRUNK_PATH%\build%BITS%\bin
SET MP4V2_BUILD_LIBPATH=%TRUNK_PATH%\build%BITS%\lib
SET SLN_PATH=%TRUNK_PATH%\vstudio10.0\

cd "%TRUNK_PATH%"
@ECHO ON

@REM Build the 32-bit MP4V2 solutions.
"%VS_PATH%" "%SLN_PATH%mp4v2.sln" /Rebuild "Debug|%PLATFORM%" /project "libmp4v2"
@IF %ERRORLEVEL% NEQ 0 GOTO :fail

"%VS_PATH%" "%SLN_PATH%mp4v2.sln" /Rebuild "Release|%PLATFORM%" /project "libmp4v2"
@IF %ERRORLEVEL% NEQ 0 GOTO :fail

@REM Copy all the 32-bit lib/bin files into the correct SVN directories for committing
@REM e.g. From $\mp4v2\build32\bin\Debug to $\mp4v2\bin\win\Debug\win32
xcopy /S /I /Y /R "%MP4V2_BUILD_BINPATH%\Debug"   "%MP4V2_BINPATH%\Debug\win%BITS%"
xcopy /S /I /Y /R "%MP4V2_BUILD_BINPATH%\Release" "%MP4V2_BINPATH%\Release\win%BITS%"

xcopy /S /I /Y /R "%MP4V2_BUILD_LIBPATH%\Debug"   "%MP4V2_LIBPATH%\Debug\win%BITS%"
xcopy /S /I /Y /R "%MP4V2_BUILD_LIBPATH%\Release" "%MP4V2_LIBPATH%\Release\win%BITS%"

@REM Copy all the 32-bit lib/bin files into the correct TFS directory for committing
xcopy /S /I /Y /R "%MP4V2_BUILD_BINPATH%" "%TFS_PATH%\bin\%PLATFORM%"
xcopy /S /I /Y /R "%MP4V2_BUILD_LIBPATH%" "%TFS_PATH%\lib\%PLATFORM%"


@REM Do the 64-bit builds.
SET BITS=64
SET PLATFORM=x%BITS%
SET BUILD_PATH=%TRUNK_PATH%\build%BITS%\
SET MP4V2_BUILD_BINPATH=%TRUNK_PATH%\build%BITS%\bin
SET MP4V2_BUILD_LIBPATH=%TRUNK_PATH%\build%BITS%\lib

cd "%TRUNK_PATH%"
@ECHO ON

@REM Build the 64-bit TREC solutions.
"%VS_PATH%" "%SLN_PATH%mp4v2.sln" /Rebuild "Debug|%PLATFORM%" /project "libmp4v2"
@IF %ERRORLEVEL% NEQ 0 GOTO :fail

"%VS_PATH%" "%SLN_PATH%mp4v2.sln" /Rebuild "Release|%PLATFORM%" /project "libmp4v2"
@IF %ERRORLEVEL% NEQ 0 GOTO :fail

@REM Copy all the 64-bit lib/bin files into the correct SVN directories for committing
@REM e.g. From $\mp4v2\build64\bin\Debug to $\mp4v2\bin\win\Debug\win64
xcopy /S /I /Y /R "%MP4V2_BUILD_BINPATH%\Debug"   "%MP4V2_BINPATH%\Debug\win%BITS%"
xcopy /S /I /Y /R "%MP4V2_BUILD_BINPATH%\Release" "%MP4V2_BINPATH%\Release\win%BITS%"

xcopy /S /I /Y /R "%MP4V2_BUILD_LIBPATH%\Debug"   "%MP4V2_LIBPATH%\Debug\win%BITS%"
xcopy /S /I /Y /R "%MP4V2_BUILD_LIBPATH%\Release" "%MP4V2_LIBPATH%\Release\win%BITS%"

@REM Copy all the 64-bit lib/bin files into the correct TFS directory for committing
xcopy /S /I /Y /R "%MP4V2_BUILD_BINPATH%" "%TFS_PATH%\bin\%PLATFORM%"
xcopy /S /I /Y /R "%MP4V2_BUILD_LIBPATH%" "%TFS_PATH%\lib\%PLATFORM%"


@REM Check in the binaries to SVN.
"%SVN_PATH%" commit "%TRUNK_PATH%" -m "Windows binaries committed by build server."
@IF %ERRORLEVEL% NEQ 0 GOTO :fail

@REM Copy all the source/include files into the TFS_PATH
xcopy /S /I /Y /R "%MP4V2_INCPATH%" "%TFS_PATH%\include\mp4v2"
xcopy /S /I /Y /R "%MP4V2_SRCPATH%" "%TFS_PATH%\src"

@REM Go online to pend edits including adds/deletes
"%TFPT_PATH%" online "%TFS_PATH%" /recursive /adds /deletes /diff /exclude:mac,*.idb,*.exp,*.ilk,*testrunner.* /noprompt
@IF %ERRORLEVEL% NEQ 0 GOTO :fail

@REM Check the Windows binaries into TFS
"%TF_PATH%" checkin "%TFS_PATH%" /noprompt /recursive /notes:"Code Reviewer"="none" /comment:"Automated checkin from Hudson project"
@IF %ERRORLEVEL% NEQ 0 GOTO :fail

@REM Store symbols on symbol server
"%DIR_SYMSTORE%\symstore.exe" add /r /f "%TFS_PATH%\*.*" /s \\techsmith.com\symbols /t "mp4v2"

GOTO :done

:fail
@ECHO "**Failed**"

:done