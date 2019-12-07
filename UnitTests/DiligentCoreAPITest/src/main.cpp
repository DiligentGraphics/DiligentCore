/*     Copyright 2019 Diligent Graphics LLC
 *  
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 * 
 *     http://www.apache.org/licenses/LICENSE-2.0
 * 
 *  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 *  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT OF ANY PROPRIETARY RIGHTS.
 *
 *  In no event and under no legal theory, whether in tort (including negligence), 
 *  contract, or otherwise, unless required by applicable law (such as deliberate 
 *  and grossly negligent acts) or agreed to in writing, shall any Contributor be
 *  liable for any damages, including any direct, indirect, special, incidental, 
 *  or consequential damages of any character arising as a result of this License or 
 *  out of the use or inability to use the software (including but not limited to damages 
 *  for loss of goodwill, work stoppage, computer failure or malfunction, or any and 
 *  all other commercial damages or losses), even if such Contributor has been advised 
 *  of the possibility of such damages.
 */

#include "gtest/gtest.h"
#include "TestingEnvironment.h"

using namespace Diligent;

int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);

    DeviceType   deviceType  = DeviceType::Undefined;
    ADAPTER_TYPE AdapterType = ADAPTER_TYPE_UNKNOWN;
    for (int i = 1; i < argc; ++i)
    {
        const auto* arg = argv[i];
        if (strcmp(arg, "--mode=d3d11") == 0)
        {
            deviceType = DeviceType::D3D11;
        }
        else if (strcmp(arg, "--mode=d3d11_sw") == 0)
        {
            deviceType  = DeviceType::D3D11;
            AdapterType = ADAPTER_TYPE_SOFTWARE;
        }
        else if (strcmp(arg, "--mode=d3d12") == 0)
        {
            deviceType = DeviceType::D3D12;
        }
        else if (strcmp(arg, "--mode=d3d12_sw") == 0)
        {
            deviceType  = DeviceType::D3D12;
            AdapterType = ADAPTER_TYPE_SOFTWARE;
        }
        else if (strcmp(arg, "--mode=vk") == 0)
        {
            deviceType = DeviceType::Vulkan;
        }
        else if (strcmp(arg, "--mode=gl") == 0)
        {
            deviceType = DeviceType::OpenGL;
        }
    }

    Diligent::TestingEnvironment* pEnv = nullptr;
    try
    {
        pEnv = new Diligent::TestingEnvironment{deviceType, AdapterType};
    }
    catch (...)
    {
        return -1;
    }
    ::testing::AddGlobalTestEnvironment(pEnv);

    auto ret_val = RUN_ALL_TESTS();
    return ret_val;
}
