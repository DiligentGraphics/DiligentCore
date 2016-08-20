/*     Copyright 2015-2016 Egor Yusov
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

#include "GraphicsTypes.h"
#include "Buffer.h"
#include "InputLayout.h"
#include "LockHelper.h"
#include "HashUtils.h"
#include "DeviceContextBase.h"
#include "BaseInterfacesGL.h"

namespace Diligent
{

class VAOCache
{
public:
    VAOCache();
    ~VAOCache();

    const GLObjectWrappers::GLVertexArrayObj& GetVAO( IPipelineState *pPSO,
                                                       IBuffer *pIndexBuffer,
                                                       VertexStreamInfo VertexStreams[],
                                                       Uint32 NumVertexStreams,
                                                       class GLContextState &GLContextState);
    void OnDestroyBuffer(IBuffer *pBuffer);
    void OnDestroyPSO(IPipelineState *pPSO);

private:
    // This structure is used as the key to find VAO
    struct VAOCacheKey
    {
        // Note that the the pointers are used for ordering only
        // They are not used to access the objects
        IPipelineState* pPSO;
        IBuffer* pIndexBuffer;
        struct StreamAttribs
        {
            IBuffer* pBuffer;
            Uint32 Stride;
            Uint32 Offset;
        }Streams[MaxBufferSlots];
        
        bool operator == (const VAOCacheKey &Key)const
        {
            return (pPSO == Key.pPSO) &&
                   (pIndexBuffer ==  Key.pIndexBuffer) &&
                   (memcmp(Streams, Key.Streams, sizeof(Streams)) == 0);
        }
    };

    struct VAOCacheKeyHashFunc
    {
        std::size_t operator() ( const VAOCacheKey& Key )const
        {
            std::size_t Seed = 0;
            HashCombine(Seed, Key.pPSO);
            if(Key.pIndexBuffer)
                HashCombine(Seed, Key.pIndexBuffer);
            for(int iStream = 0; iStream < _countof(Key.Streams); ++iStream )
            {
                auto &CurrStream = Key.Streams[iStream];
                if( CurrStream.pBuffer )
                {
                    HashCombine(Seed, CurrStream.pBuffer);
                    HashCombine(Seed, CurrStream.Offset);
                    HashCombine(Seed, CurrStream.Stride);
                }
            }
            return Seed;
        }
    };


    friend class RenderDeviceGLImpl;
    ThreadingTools::LockFlag m_CacheLockFlag;
    std::unordered_map<VAOCacheKey, GLObjectWrappers::GLVertexArrayObj, VAOCacheKeyHashFunc> m_Cache;
    std::unordered_multimap<IPipelineState*, VAOCacheKey> m_PSOToKey;
    std::unordered_multimap<IBuffer*, VAOCacheKey> m_BuffToKey;
};

}
