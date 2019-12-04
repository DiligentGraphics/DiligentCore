REM Must be executed from build\DiligentCore folder

if NOT "%PLATFORM_NAME%"=="UWP" ( 
   UnitTests\%CONFIGURATION%\DiligentCoreTest.exe
)
