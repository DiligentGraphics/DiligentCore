$DOTNET_URL = "https://dot.net/v1/dotnet-install.ps1"
$CMAKE_URL  = "https://github.com/Kitware/CMake/releases/download/v3.23.3/cmake-3.23.3-windows-x86_64.zip"

# Install .NET
Invoke-WebRequest $DOTNET_URL -OutFile "dotnet-install.ps1"
./dotnet-install.ps1 -Version 6.0.100 -InstallDir "C:\Program Files\dotnet"

# Install CMake
Invoke-WebRequest $CMAKE_URL -OutFile "cmake.zip"
Expand-Archive -Path cmake.zip -DestinationPath C:\projects\deps -Force

# Configuring environment
Move-Item -Path (Get-ChildItem C:\projects\deps\cmake-* | Select-Object -ExpandProperty FullName) -Destination C:\projects\deps\cmake -Force
$env:PATH = "C:\projects\deps\cmake\bin;C:\Python37-x64;$env:PATH"

# Check dependencies 
cmake --version
python --version

Write-Host "Available .NET SDK versions:"
dotnet --list-sdks
