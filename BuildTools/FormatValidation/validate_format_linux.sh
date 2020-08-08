#!/bin/bash

BIN=$(find . -name 'clang-format_linux_*')

## Try to launch the bin
eval "$BIN --version >/dev/null 2> /dev/null"
if [ $? -ne 0 ]; then
  ## BIN failed to run, try to get a system installed clang-format
  SYS_BIN=$(which clang-format 2> /dev/null)
  if [ $? -ne 0 ]; then
    echo "WARNING: skipping format validation as no suitable executable was found"
    BIN=""
  else
    BIN_VERSION=$(echo $BIN | grep -Eo '[0-9]+\.[0-9]+\.[0-9]+')
    SYS_BIN_VERSION=$(eval "$SYS_BIN --version" | grep -Eo '[0-9]+\.[0-9]+\.[0-9]+')
    if [ "$BIN_VERSION" != "$SYS_BIN_VERSION" ]; then
      echo "WARNING: could not load the provided clang-format for validation."
      echo "   clang-format exists in the system path however its version is $SYS_BIN_VERSION instead of $BIN_VERSION"
      echo "   Should the validation fail, you can try skipping it by setting the cmake option:"
      echo "   DILIGENT_SKIP_FORMAT_VALIDATION"
    fi
    BIN="$SYS_BIN"
  fi
fi

if [ ! -z "$BIN" ]; then
  python clang-format-validate.py --clang-format-executable "$BIN" \
  -r ../../Common ../../Graphics ../../Platforms ../../Primitives ../../Tests \
  --exclude ../../Graphics/HLSL2GLSLConverterLib/include/GLSLDefinitions.h \
  --exclude ../../Graphics/HLSL2GLSLConverterLib/include/GLSLDefinitions_inc.h \
  --exclude ../../Graphics/GraphicsEngineVulkan/shaders/GenerateMipsCS_inc.h \
  --exclude ../../Tests/DiligentCoreAPITest/assets/*
fi
