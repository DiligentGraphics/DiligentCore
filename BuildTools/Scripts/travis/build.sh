mkdir build
cd build

if [ "$TRAVIS_OS_NAME" = "linux" ]; then 
  cmake .. -G "Unix Makefiles" $1 -DCMAKE_BUILD_TYPE=${CONFIG}
  cmake --build . && return ${PIPESTATUS[0]}
fi

if [ "$TRAVIS_OS_NAME" = "osx" ]; then 
  if [ "$IOS" = "true" ]; then 
    cmake .. -DCMAKE_TOOLCHAIN_FILE=../DiligentCore/ios.toolchain.cmake -DIOS_PLATFORM=SIMULATOR64 $1 -G "Xcode"
  else
    cmake .. $1 -G "Xcode"
  fi
  xcodebuild -configuration ${CONFIG} | xcpretty && return ${PIPESTATUS[0]}
fi

