@echo off

set RUN_API_CORE_TEST=false
set RUN_CORE_API_D3D11_TEST=false
set set RUN_D3D12_TEST=false

if "%PLATFORM_NAME%"=="Windows" (
    set RUN_API_CORE_TEST=true
    set RUN_CORE_API_D3D11_TEST=true
    set RUN_CORE_API_D3D12_TEST=true
)

if "%PLATFORM_NAME%"=="Windows8.1" (
    set RUN_API_CORE_TEST=true
    set RUN_CORE_API_D3D11_TEST=true
)

if "%RUN_API_CORE_TEST%"=="true" (
    "%1\UnitTests\DiligentCoreTest\%CONFIGURATION%\DiligentCoreTest.exe"
)

if "%RUN_CORE_API_D3D11_TEST%"=="true" (
    rem "%1\UnitTests\DiligentCoreAPITest\%CONFIGURATION%\DiligentCoreAPITest.exe" --mode=d3d11_sw
)

if "%RUN_CORE_API_D3D12_TEST%"=="true" (
    rem "%1\UnitTests\DiligentCoreAPITest\%CONFIGURATION%\DiligentCoreAPITest.exe" --mode=d3d12_sw
)
