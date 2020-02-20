mkdir build
cd build

if [ "$TRAVIS_OS_NAME" = "linux" ]; then 
  cmake .. -G "Unix Makefiles" $1 -DCMAKE_BUILD_TYPE=${CONFIG} &&
  cmake --build .
  # We must return now because otherwise the following if... command will reset the error code
  return
fi

if [ "$TRAVIS_OS_NAME" = "osx" ]; then 
  if [ "$IOS" = "true" ]; then 
    cmake .. -DCMAKE_TOOLCHAIN_FILE=../DiligentCore/ios.toolchain.cmake -DIOS_PLATFORM=SIMULATOR64 $1 -G "Xcode" || return
    local BUILD_SETTINGS="CODE_SIGN_IDENTITY=\"\" CODE_SIGNING_REQUIRED=NO"
  else
    cmake .. $1 -G "Xcode" || return
    local BUILD_SETTINGS=""
  fi
  xcodebuild -configuration ${CONFIG} ${BUILD_SETTINGS} | xcpretty && return ${PIPESTATUS[0]}
fi

