set CMAKE_URL="https://github.com/Kitware/CMake/releases/download/v3.15.4/cmake-3.15.4-win64-x64.zip"
appveyor DownloadFile %CMAKE_URL% -FileName cmake.zip
7z x cmake.zip -oC:\projects\deps > nul
move C:\projects\deps\cmake-* C:\projects\deps\cmake
set PATH=C:\projects\deps\cmake\bin;C:\Python37;%PATH%
cmake --version
python --version
