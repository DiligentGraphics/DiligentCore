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

#pragma once

/// \file
/// Declaration of Diligent::RenderPassCache class

#include <unordered_map>
#include <mutex>
#include "GraphicsTypes.h"
#include "Constants.h"
#include "HashUtils.h"
#include "VulkanUtilities/VulkanObjectWrappers.h"

namespace Diligent
{

class RenderDeviceVkImpl;
class RenderPassCache
{
public:
    RenderPassCache(RenderDeviceVkImpl& DeviceVk)noexcept : 
        m_DeviceVkImpl(DeviceVk)
    {}

    RenderPassCache             (const RenderPassCache&) = delete;
    RenderPassCache             (RenderPassCache&&)      = delete;
    RenderPassCache& operator = (const RenderPassCache&) = delete;
    RenderPassCache& operator = (RenderPassCache&&)      = delete;

    ~RenderPassCache();

    // This structure is used as the key to find framebuffer
    struct RenderPassCacheKey
    {
        RenderPassCacheKey() : 
            NumRenderTargets{0},
            SampleCount     {0},
            DSVFormat       {TEX_FORMAT_UNKNOWN}
        {}

        RenderPassCacheKey(Uint32               _NumRenderTargets, 
                           Uint32               _SampleCount,
                           const TEXTURE_FORMAT _RTVFormats[],
                           TEXTURE_FORMAT       _DSVFormat) : 
            NumRenderTargets{static_cast<decltype(NumRenderTargets)>(_NumRenderTargets)},
            SampleCount     {static_cast<decltype(SampleCount)>     (_SampleCount)     },
            DSVFormat       {_DSVFormat                                                }
        {
            VERIFY_EXPR(_NumRenderTargets <= std::numeric_limits<decltype(NumRenderTargets)>::max());
            VERIFY_EXPR(_SampleCount <= std::numeric_limits<decltype(SampleCount)>::max());
            for(Uint32 rt=0; rt < NumRenderTargets; ++rt)
                RTVFormats[rt] = _RTVFormats[rt];
        }
        // Default memeber initialization is intentionally omitted
        Uint8           NumRenderTargets;
        Uint8           SampleCount;
        TEXTURE_FORMAT  DSVFormat;
        TEXTURE_FORMAT  RTVFormats[MaxRenderTargets];

        bool operator == (const RenderPassCacheKey &rhs)const
        {
            if (GetHash()        != rhs.GetHash()        ||
                NumRenderTargets != rhs.NumRenderTargets ||
                SampleCount      != rhs.SampleCount      ||
                DSVFormat        != rhs.DSVFormat)
            {
                return false;
            }

            for (Uint32 rt = 0; rt < NumRenderTargets; ++rt)
                if (RTVFormats[rt] != rhs.RTVFormats[rt])
                    return false;

            return true;
        }

        size_t GetHash()const
        {
            if(Hash == 0)
            {
                Hash = ComputeHash(NumRenderTargets, SampleCount, DSVFormat);
                for(Uint32 rt = 0; rt < NumRenderTargets; ++rt)
                    HashCombine(Hash, RTVFormats[rt]);
            }
            return Hash;
        }

    private:
        mutable size_t Hash = 0;
    };

    VkRenderPass GetRenderPass(const RenderPassCacheKey& Key);

private:

    struct RenderPassCacheKeyHash
    {
        std::size_t operator() (const RenderPassCacheKey& Key)const
        {
            return Key.GetHash();
        }
    };
    
    RenderDeviceVkImpl& m_DeviceVkImpl;
    std::mutex m_Mutex;
    std::unordered_map<RenderPassCacheKey, VulkanUtilities::RenderPassWrapper, RenderPassCacheKeyHash> m_Cache;
};

}
