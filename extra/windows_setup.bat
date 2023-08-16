@echo off
echo "Installing/Updating NanoCut.. When finished, this window will close and NanoCut will be started!"
mkdir %APPDATA%\NanoCut
copy /b/v/y glfw3.dll %APPDATA%\NanoCut
copy /b/v/y NanoCut.exe %APPDATA%\NanoCut
set SCRIPT="%TEMP%\%RANDOM%-%RANDOM%-%RANDOM%-%RANDOM%.vbs"
echo Set oWS = WScript.CreateObject("WScript.Shell") >> %SCRIPT%
echo sLinkFile = "%USERPROFILE%\Desktop\NanoCut.lnk" >> %SCRIPT%
echo Set oLink = oWS.CreateShortcut(sLinkFile) >> %SCRIPT%
echo oLink.TargetPath = "%APPDATA%\NanoCut\NanoCut.exe" >> %SCRIPT%
echo oLink.Save >> %SCRIPT%
cscript /nologo %SCRIPT%
del %SCRIPT%
start %USERPROFILE%\Desktop\NanoCut.lnk
