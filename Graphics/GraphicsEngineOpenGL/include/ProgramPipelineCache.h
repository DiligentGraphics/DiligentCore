/*     Copyright 2015 Egor Yusov
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

#include "RenderDevice.h"
#include "HashUtils.h"
#include "GLProgram.h"

namespace Diligent
{

class ProgramPipelineCache
{
public:
    ProgramPipelineCache( class RenderDeviceGLImpl *pRenderDeviceOpenGL );
    ~ProgramPipelineCache();

    struct CacheElementType
    {
        GLObjectWrappers::GLPipelineObj Pipeline;
        GLProgram Program;
        CacheElementType() :
            Pipeline( false ),
            Program( false )
        {}
        CacheElementType( CacheElementType &&Elem ) :
            Pipeline( std::move( Elem.Pipeline ) ),
            Program( std::move( Elem.Program ) )
        {}
    };

    CacheElementType &GetProgramPipeline( RefCntAutoPtr<IShader> *ppShaders, Uint32 NumShadersToSet );
    void OnDestroyShader(IShader *pShader);

private:
    friend class RenderDeviceGLImpl;
    ThreadingTools::LockFlag m_CacheLockFlag;

    // This structure is used as the key to find VAO
    struct PipelineCacheKey
    {
        IShader *pVS;
        IShader *pGS;
        IShader *pPS;
        IShader *pDS;
        IShader *pHS;
        IShader *pCS;

        bool operator == (const PipelineCacheKey &Key)const
        {
            return pVS == Key.pVS &&
                   pGS == Key.pGS &&
                   pPS == Key.pPS &&
                   pDS == Key.pDS &&
                   pHS == Key.pHS &&
                   pCS == Key.pCS;
        }
    };

    struct PipelineCacheHashFunc
    {
        std::size_t operator() ( const PipelineCacheKey& Key )const
        {
            std::size_t Seed = 0;
            if(Key.pVS)HashCombine(Seed, Key.pVS);
            if(Key.pGS)HashCombine(Seed, Key.pGS);
            if(Key.pPS)HashCombine(Seed, Key.pPS);
            if(Key.pDS)HashCombine(Seed, Key.pDS);
            if(Key.pHS)HashCombine(Seed, Key.pHS);
            if(Key.pCS)HashCombine(Seed, Key.pCS);
            return Seed;
        }
    };

    bool m_bIsProgramPipelineSupported;

    std::unordered_map<PipelineCacheKey, CacheElementType, PipelineCacheHashFunc> m_Cache;
    std::unordered_multimap<IShader*, PipelineCacheKey> m_ShaderToKey;
};

}
