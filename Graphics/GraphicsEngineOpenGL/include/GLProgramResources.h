/*     Copyright 2015-2019 Egor Yusov
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

#include <vector>

#include "GLObjectWrapper.h"
#include "HashUtils.h"
#include "ShaderBase.h"
#include "SamplerGLImpl.h"
#include "HashUtils.h"
#include "ShaderResourceVariableBase.h"
#include "StringPool.h"

#ifdef _DEBUG
#   define VERIFY_RESOURCE_BINDINGS
#endif

namespace Diligent
{
    class GLProgramResources
    {
    public:
        GLProgramResources(){}
        ~GLProgramResources();
        GLProgramResources             (GLProgramResources&& Program)noexcept;

        GLProgramResources             (const GLProgramResources&)  = delete;
        GLProgramResources& operator = (const GLProgramResources&)  = delete;
        GLProgramResources& operator = (      GLProgramResources&&) = delete;

        void LoadUniforms(IObject&                      Owner,
                          class RenderDeviceGLImpl*     pDeviceGLImpl,
                          SHADER_TYPE                   ShaderStages,
                          GLuint                        GLProgram);


        void Clone(class RenderDeviceGLImpl*             pDeviceGLImpl, 
                   IObject&                              Owner,
                   const GLProgramResources&             SrcResources, 
                   const PipelineResourceLayoutDesc&     ResourceLayout,
                   const SHADER_RESOURCE_VARIABLE_TYPE*  AllowedVarTypes, 
                   Uint32                                NumAllowedTypes);

        struct GLProgramVariableBase : ShaderVariableBase
        {
/*  0 */    // ShaderVariableBase
/* 16 */    const Char*                             Name;
/* 24 */    const SHADER_RESOURCE_VARIABLE_TYPE     VariableType;
/* 25 */    const SHADER_RESOURCE_TYPE              ResourceType;
/* 26 */    const Uint16                            VariableIndex;
/* 28 */          Uint32                            ArraySize;
/* 32 */    RefCntAutoPtr<IDeviceObject>* const     pResources;
/* 40 */    //End of data

            GLProgramVariableBase(IObject&                        _Owner,
                                  const Char*                     _Name, 
                                  SHADER_RESOURCE_VARIABLE_TYPE   _VariableType,
                                  SHADER_RESOURCE_TYPE            _ResourceType,
                                  Uint16                          _VariableIndex,
                                  Uint32                          _ArraySize,
                                  RefCntAutoPtr<IDeviceObject>*   _pResources) :
                ShaderVariableBase(_Owner),
                Name              (_Name),
                VariableType      (_VariableType),
                ResourceType      (_ResourceType),
                VariableIndex     (_VariableIndex),
                ArraySize         (_ArraySize),
                pResources        (_pResources)
            {
                VERIFY_EXPR(_ArraySize >= 1);
            }

            bool IsCompatibleWith(const GLProgramVariableBase& Var)const
            {
                return VariableType == Var.VariableType && 
                       ResourceType == Var.ResourceType &&
                       ArraySize    == Var.ArraySize;
            }

            size_t GetHash()const
            {
                return ComputeHash(static_cast<Int32>(VariableType), static_cast<Int32>(ResourceType), ArraySize);
            }

            virtual void Set(IDeviceObject* pObject)override final
            {
                VERIFY(pResources != nullptr, "This variable has no resource cache attached");
                pResources[0] = pObject;
            }

            virtual void SetArray(IDeviceObject* const* ppObjects, Uint32 FirstElement, Uint32 NumElements)override final
            {
                VERIFY(pResources, "This variable has no resource cache attached");
                VERIFY(FirstElement + NumElements <= ArraySize, "Array indices are out of range");
                for (Uint32 i=0; i < NumElements; ++i)
                    pResources[FirstElement + i] = ppObjects[i];
            }

            virtual SHADER_RESOURCE_VARIABLE_TYPE GetType()const override final
            {
                return VariableType;
            }

            virtual Uint32 GetIndex()const override final
            {
                return VariableIndex;
            }

            virtual bool IsBound(Uint32 ArrayIndex) const override final
            {
                VERIFY(ArrayIndex < ArraySize, "Array index (", ArrayIndex, ") is out of range");
                return pResources[ArrayIndex] != nullptr;
            }

            virtual ShaderResourceDesc GetResourceDesc()const override final
            {
                ShaderResourceDesc ResourceDesc;
                ResourceDesc.Name      = Name;
                ResourceDesc.ArraySize = ArraySize;
                ResourceDesc.Type      = ResourceType;
                return ResourceDesc;
            }
        };

        struct UniformBufferInfo final : GLProgramVariableBase
        {
            UniformBufferInfo            (const UniformBufferInfo&)  = delete;
            UniformBufferInfo& operator= (const UniformBufferInfo&)  = delete;
            UniformBufferInfo            (      UniformBufferInfo&&) = default;
            UniformBufferInfo& operator= (      UniformBufferInfo&&) = default;

            UniformBufferInfo(IObject&                        _Owner,
                              const Char*                     _Name, 
                              SHADER_RESOURCE_VARIABLE_TYPE   _VariableType,
                              SHADER_RESOURCE_TYPE            _ResourceType,
                              Uint16                          _VariableIndex,
                              Uint32                          _ArraySize,
                              RefCntAutoPtr<IDeviceObject>*   _pResources,
                              GLuint                          _UBIndex) :
                GLProgramVariableBase(_Owner, _Name, _VariableType, _ResourceType, _VariableIndex, _ArraySize, _pResources),
                UBIndex(_UBIndex)
            {}

            bool IsCompatibleWith(const UniformBufferInfo& UBI)const
            {
                return UBIndex == UBI.UBIndex &&
                       GLProgramVariableBase::IsCompatibleWith(UBI);
            }

            size_t GetHash()const
            {
                return ComputeHash(UBIndex, GLProgramVariableBase::GetHash());
            }

            const GLuint UBIndex;
        };
        static_assert((sizeof(UniformBufferInfo) % sizeof(void*)) == 0, "sizeof(UniformBufferInfo) must be multiple of sizeof(void*)");


        struct SamplerInfo final : GLProgramVariableBase
        {
            SamplerInfo            (const SamplerInfo&)  = delete;
            SamplerInfo& operator= (const SamplerInfo&)  = delete;
            SamplerInfo            (      SamplerInfo&&) = default;
            SamplerInfo& operator= (      SamplerInfo&&) = default;

            SamplerInfo(IObject&                        _Owner,
                        const Char*                     _Name, 
                        SHADER_RESOURCE_VARIABLE_TYPE   _VariableType,
                        SHADER_RESOURCE_TYPE            _ResourceType,
                        Uint16                          _VariableIndex,
                        Uint32                          _ArraySize,
                        RefCntAutoPtr<IDeviceObject>*   _pResources,
                        GLint                           _Location,
                        GLenum                          _SamplerType,
                        class SamplerGLImpl*            _pStaticSampler) :
                GLProgramVariableBase(_Owner, _Name, _VariableType, _ResourceType, _VariableIndex, _ArraySize, _pResources),
                Location      (_Location),
                SamplerType   (_SamplerType),
                pStaticSampler(_pStaticSampler)
            {}

            bool IsCompatibleWith(const SamplerInfo& SI)const
            {
                return Location       == SI.Location &&
                       pStaticSampler == SI.pStaticSampler &&
                       GLProgramVariableBase::IsCompatibleWith(SI);
            }

            size_t GetHash()const
            {
                return ComputeHash(Location, SamplerType, GLProgramVariableBase::GetHash());
            }

            const GLint                        Location;
            const GLenum                       SamplerType;
            RefCntAutoPtr<class SamplerGLImpl> pStaticSampler;
        };
        static_assert((sizeof(SamplerInfo) % sizeof(void*)) == 0, "sizeof(SamplerInfo) must be multiple of sizeof(void*)");

                
        struct ImageInfo final : GLProgramVariableBase
        {
            ImageInfo            (const ImageInfo&)  = delete;
            ImageInfo& operator= (const ImageInfo&)  = delete;
            ImageInfo            (      ImageInfo&&) = default;
            ImageInfo& operator= (      ImageInfo&&) = default;

            ImageInfo(IObject&                        _Owner,
                      const Char*                     _Name, 
                      SHADER_RESOURCE_VARIABLE_TYPE   _VariableType,
                      SHADER_RESOURCE_TYPE            _ResourceType,
                      Uint16                          _VariableIndex,
                      Uint32                          _ArraySize,
                      RefCntAutoPtr<IDeviceObject>*   _pResources,
                      GLint                           _Location,
                      GLenum                          _ImageType) :
                GLProgramVariableBase(_Owner, _Name, _VariableType, _ResourceType, _VariableIndex, _ArraySize, _pResources),
                Location    (_Location),
                ImageType   (_ImageType)
            {}

            bool IsCompatibleWith(const ImageInfo& II)const
            {
                return Location  == II.Location &&
                       ImageType == II.ImageType &&
                       GLProgramVariableBase::IsCompatibleWith(II);
            }

            size_t GetHash()const
            {
                return ComputeHash(Location, ImageType, GLProgramVariableBase::GetHash());
            }

            const GLint  Location;
            const GLenum ImageType;
        };
        static_assert((sizeof(ImageInfo) % sizeof(void*)) == 0, "sizeof(ImageInfo) must be multiple of sizeof(void*)");


        struct StorageBlockInfo final : GLProgramVariableBase
        {
            StorageBlockInfo            (const StorageBlockInfo&)  = delete;
            StorageBlockInfo& operator= (const StorageBlockInfo&)  = delete;
            StorageBlockInfo            (      StorageBlockInfo&&) = default;
            StorageBlockInfo& operator= (      StorageBlockInfo&&) = default;

            StorageBlockInfo(IObject&                        _Owner,
                             const Char*                     _Name, 
                             SHADER_RESOURCE_VARIABLE_TYPE   _VariableType,
                             SHADER_RESOURCE_TYPE            _ResourceType,
                             Uint16                          _VariableIndex,
                             Uint32                          _ArraySize,
                             RefCntAutoPtr<IDeviceObject>*   _pResources,
                             GLint                           _SBIndex) :
                GLProgramVariableBase(_Owner, _Name, _VariableType, _ResourceType, _VariableIndex, _ArraySize, _pResources),
                SBIndex(_SBIndex)
            {}

            bool IsCompatibleWith(const StorageBlockInfo& SBI)const
            {
                return SBIndex == SBI.SBIndex &&
                       GLProgramVariableBase::IsCompatibleWith(SBI);
            }

            size_t GetHash()const
            {
                return ComputeHash(SBIndex, GLProgramVariableBase::GetHash());
            }

            const GLint SBIndex;
        };
        static_assert( (sizeof(StorageBlockInfo) % sizeof(void*)) == 0, "sizeof(StorageBlockInfo) must be multiple of sizeof(void*)");


        Uint32 GetNumUniformBuffers()const { return m_NumUniformBuffers; }
        Uint32 GetNumSamplers()      const { return m_NumSamplers;       }
        Uint32 GetNumImages()        const { return m_NumImages;         }
        Uint32 GetNumStorageBlocks() const { return m_NumStorageBlocks;  }

        UniformBufferInfo& GetUniformBuffer(Uint32 Index)
        {
            VERIFY(Index < m_NumUniformBuffers, "Uniform buffer index (", Index, ") is out of range");
            return m_UniformBuffers[Index];
        }

        SamplerInfo& GetSampler(Uint32 Index)
        {
            VERIFY(Index < m_NumSamplers, "Sampler index (", Index, ") is out of range");
            return m_Samplers[Index]; 
        }

        ImageInfo& GetImage(Uint32 Index)
        {
            VERIFY(Index < m_NumImages, "Image index (", Index, ") is out of range");
            return m_Images[Index];
        }

        StorageBlockInfo& GetStorageBlock(Uint32 Index)
        {
            VERIFY(Index < m_NumStorageBlocks, "Storage block index (", Index, ") is out of range");
            return m_StorageBlocks[Index];
        }


        const UniformBufferInfo& GetUniformBuffer(Uint32 Index)const
        {
            VERIFY(Index < m_NumUniformBuffers, "Uniform buffer index (", Index, ") is out of range");
            return m_UniformBuffers[Index];
        }

        const SamplerInfo& GetSampler(Uint32 Index)const
        {
            VERIFY(Index < m_NumSamplers, "Sampler index (", Index, ") is out of range");
            return m_Samplers[Index]; 
        }

        const ImageInfo& GetImage(Uint32 Index)const
        {
            VERIFY(Index < m_NumImages, "Image index (", Index, ") is out of range");
            return m_Images[Index];
        }

        const StorageBlockInfo& GetStorageBlock(Uint32 Index)const
        {
            VERIFY(Index < m_NumStorageBlocks, "Storage block index (", Index, ") is out of range");
            return m_StorageBlocks[Index];
        }


        Uint32 GetVariableCount()const
        {
            return m_NumUniformBuffers + m_NumSamplers + m_NumImages + m_NumStorageBlocks;
        }

        void BindResources(IResourceMapping* pResourceMapping, Uint32 Flags);

#ifdef VERIFY_RESOURCE_BINDINGS
        void dbgVerifyResourceBindings()const;
#endif

        GLProgramVariableBase* GetVariable(const Char* Name);
        GLProgramVariableBase* GetVariable(Uint32 Index)
        {
            return const_cast<GLProgramVariableBase*>(const_cast<const GLProgramResources*>(this)->GetVariable(Index));
        }
        const GLProgramVariableBase* GetVariable(Uint32 Index)const;

        bool IsCompatibleWith(const GLProgramResources& Res)const;
        size_t GetHash()const;

        SHADER_TYPE GetShaderStages() const {return m_ShaderStages;}

        template<typename THandleUB,
                 typename THandleSampler,
                 typename THandleImg,
                 typename THandleSB>
        void ProcessConstResources(THandleUB       HandleUB,
                                   THandleSampler  HandleSampler,
                                   THandleImg      HandleImg,
                                   THandleSB       HandleSB)const
        {
            for (Uint32 ub=0; ub < m_NumUniformBuffers; ++ub)
                HandleUB(GetUniformBuffer(ub));

            for (Uint32 s=0; s < m_NumSamplers; ++s)
                HandleSampler(GetSampler(s));

            for (Uint32 img=0; img < m_NumImages; ++img)
                HandleImg(GetImage(img));

            for (Uint32 sb=0; sb < m_NumStorageBlocks; ++sb)
                HandleSB(GetStorageBlock(sb));
        }
    private:
        void AllocateResources(IObject&                        Owner,
                               std::vector<UniformBufferInfo>& UniformBlocks,
                               std::vector<SamplerInfo>&       Samplers,
                               std::vector<ImageInfo>&         Images,
                               std::vector<StorageBlockInfo>&  StorageBlocks,
                               bool                            InitializeResourceCache);

        template<typename THandleUB,
                 typename THandleSampler,
                 typename THandleImg,
                 typename THandleSB>
        void ProcessResources(THandleUB       HandleUB,
                              THandleSampler  HandleSampler,
                              THandleImg      HandleImg,
                              THandleSB       HandleSB)
        {
            for (Uint32 ub=0; ub < m_NumUniformBuffers; ++ub)
                HandleUB(GetUniformBuffer(ub));

            for (Uint32 s=0; s < m_NumSamplers; ++s)
                HandleSampler(GetSampler(s));

            for (Uint32 img=0; img < m_NumImages; ++img)
                HandleImg(GetImage(img));

            for (Uint32 sb=0; sb < m_NumStorageBlocks; ++sb)
                HandleSB(GetStorageBlock(sb));
        }

        // There could be more than one stage if using non-separable programs
        SHADER_TYPE         m_ShaderStages = SHADER_TYPE_UNKNOWN; 

        // Memory layout:
        // 
        //  |  Uniform buffers  |   Samplers  |   Images   |   Storage Blocks   |   Resource Cache   |   String Pool Data   |
        //

        UniformBufferInfo*              m_UniformBuffers = nullptr;
        SamplerInfo*                    m_Samplers       = nullptr;
        ImageInfo*                      m_Images         = nullptr;
        StorageBlockInfo*               m_StorageBlocks  = nullptr;
        RefCntAutoPtr<IDeviceObject>*   m_ResourceCache  = nullptr;

        StringPool          m_StringPool;

        Uint32              m_NumUniformBuffers = 0;
        Uint32              m_NumSamplers       = 0;
        Uint32              m_NumImages         = 0;
        Uint32              m_NumStorageBlocks  = 0;

        // When adding new member DO NOT FORGET TO UPDATE GLProgramResources( GLProgramResources&& ProgramResources )!!!
    };
}
