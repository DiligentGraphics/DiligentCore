if [ "$TRAVIS_OS_NAME" = "linux" ]; then
  echo No tests for linux yet
fi

if [ "$TRAVIS_OS_NAME" = "osx" ]; then 
  if [ "$IOS" = "false" ]; then
    $1/Tests/DiligentCoreTest/$CONFIG/DiligentCoreTest
  fi
fi
