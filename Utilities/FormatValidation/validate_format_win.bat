python clang-format-validate.py --color never --clang-format-executable clang-format_10.0.0.exe ^
-r ../../Common ../../Graphics ^
--exclude ../../Graphics/HLSL2GLSLConverterLib/include/GLSLDefinitions.h ^
--exclude ../../Graphics/HLSL2GLSLConverterLib/include/GLSLDefinitions_inc.h ^
--exclude ../../Graphics/GraphicsEngineVulkan ^
--exclude ../../Graphics/GraphicsEngineMetal ^
--exclude ../../Graphics/GraphicsEngineOpenGL