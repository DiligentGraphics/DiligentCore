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
#include "TextureView.h"
#include "LockHelper.h"
#include "HashUtils.h"
#include "GLObjectWrapper.h"

namespace Diligent
{

class FBOCache
{
public:
    FBOCache();
    ~FBOCache();

    const GLObjectWrappers::GLFrameBufferObj& GetFBO( Uint32 NumRenderTargets, 
                                                       ITextureView *ppRenderTargets[], 
                                                       ITextureView *pDepthStencil,
                                                       class GLContextState &ContextState);
    void OnReleaseTexture(ITexture *pTexture);

private:
    // This structure is used as the key to find FBO
    struct FBOCacheKey
    {
        // Using pointers is not reliable!

        Uint32 NumRenderTargets;

        // Unique IDs of textures bound as render targets
        Diligent::UniqueIdentifier RTIds[MaxRenderTargets];
        TextureViewDesc RTVDescs[MaxRenderTargets];

        // Unique IDs of texture bound as depth stencil
        Diligent::UniqueIdentifier DSId;
        TextureViewDesc DSVDesc;

        mutable size_t Hash;

        bool operator == (const FBOCacheKey &Key)const;

        FBOCacheKey() :
            NumRenderTargets( 0 ),
            DSId(0),
            Hash(0)
        {
            for( int rt = 0; rt < MaxRenderTargets; ++rt )
                RTIds[rt] = 0;
        }
    };

    struct FBOCacheKeyHashFunc
    {
        std::size_t operator() ( const FBOCacheKey& Key )const;
    };


    friend class RenderDeviceGLImpl;
    ThreadingTools::LockFlag m_CacheLockFlag;
    std::unordered_map<FBOCacheKey, GLObjectWrappers::GLFrameBufferObj, FBOCacheKeyHashFunc> m_Cache;
    
    // Multimap that sets up correspondence between unique texture id and all
    // FBOs it is used in
    std::unordered_multimap<Diligent::UniqueIdentifier, FBOCacheKey> m_TexIdToKey;
};

}
