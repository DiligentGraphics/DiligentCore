#!/bin/bash

## Set the shipped BIN
BIN="./clang-format_linux_10.0.0"
## ...or use the find command to avoid maintaining
#BIN=$(find . -name 'clang-format_linux*')

## Set to anything but 0 to force skip format validation
SKIP=0

if [ "$SKIP" -eq 0 ]; then
  ## Try to get the version
  eval "$BIN" --version 2> /dev/null  
  if [ $? -ne 0 ]; then
    ## Try to get a system installed clang-format
    BIN=$(which clang-format 2> /dev/null)
    if [ $? -ne 0 ]; then
      echo "WARNING: skipping format validation as no suitable executable was found"
      SKIP=1
    else
      echo "WARNING: could not load the integrated format validator binary, but a system one was found."
      echo "   Should it cause any issue due to versioning, you can skip validation by setting \"SKIP=1\""
      echo "   in: DiligentCore/BuildTools/FormatValidation/validate_format_linux.sh"
    fi
  fi
  
  if [ "$SKIP" -eq 0 ]; then
    python clang-format-validate.py --clang-format-executable "$BIN" \
    -r ../../Common ../../Graphics ../../Platforms ../../Primitives ../../Tests \
    --exclude ../../Graphics/HLSL2GLSLConverterLib/include/GLSLDefinitions.h \
    --exclude ../../Graphics/HLSL2GLSLConverterLib/include/GLSLDefinitions_inc.h \
    --exclude ../../Graphics/GraphicsEngineVulkan/shaders/GenerateMipsCS_inc.h \
    --exclude ../../Tests/DiligentCoreAPITest/assets/*
  fi
fi
