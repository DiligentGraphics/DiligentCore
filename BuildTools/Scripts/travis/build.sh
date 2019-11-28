mkdir build
cd build

if [ "$TRAVIS_OS_NAME" = "linux" ]; then 
  cmake .. -G "Unix Makefiles" -DDILIGENT_ENABLE_TESTS=TRUE -DCMAKE_BUILD_TYPE=${CONFIG}
  cmake --build .
fi
|
if [ "$TRAVIS_OS_NAME" = "osx" ]; then 
  if [ "$IOS" = "true" ]; then 
    cmake .. -DCMAKE_TOOLCHAIN_FILE=../DiligentCore/ios.toolchain.cmake -DIOS_PLATFORM=SIMULATOR64 -DDILIGENT_ENABLE_TESTS=TRUE -G "Xcode"
  else
    cmake .. -DDILIGENT_ENABLE_TESTS=TRUE -G "Xcode"
  fi
  xcodebuild -configuration ${CONFIG} | xcpretty
fi

