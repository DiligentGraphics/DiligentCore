/*
 *  Copyright 2019-2021 Diligent Graphics LLC
 *  Copyright 2015-2019 Egor Yusov
 *  
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *  
 *      http://www.apache.org/licenses/LICENSE-2.0
 *  
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
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
/// Declaration of Diligent::ShaderResourceLayoutVk class

// All resources are stored in a single continuous chunk of memory using the following layout:
//
//   m_ResourceBuffer
//      |
//     ||   VkResource[0]  ...  VkResource[s-1]   |   VkResource[s]  ...  VkResource[s+m-1]   |   VkResource[s+m]  ...  VkResource[s+m+d-1]   ||                      ||
//     ||                                         |                                           |                                               ||                      ||
//     ||            VARIABLE_TYPE_STATIC         |             VARIABLE_TYPE_MUTABLE         |               VARIABLE_TYPE_DYNAMIC           ||  Immutable Samplers  ||
//     ||                                         |                                           |                                               ||                      ||
//
//      s == m_NumResources[SHADER_RESOURCE_VARIABLE_TYPE_STATIC]
//      m == m_NumResources[SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE]
//      d == m_NumResources[SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC]
//
//
//   Every ShaderVariableVkImpl variable managed by ShaderVariableManagerVk keeps a reference to corresponding VkResource.
//
//    ___________________________                  ___________________________________________________________________________
//   |                           |   unique_ptr   |                   |                 |               |                     |
//   | ShaderResourceLayoutVk    |--------------->|   VkResource[0]   |  VkResource[1]  |       ...     | VkResource[s+m+d-1] |
//   |___________________________|                |___________________|_________________|_______________|_____________________|
//                                                                       A                       A
//                                                                      /                        |
//                                                                    Ref                       Ref
//                                                                    /                          |
//    __________________________                   __________________/___________________________|___________________________
//   |                          |                 |                           |                            |                 |
//   |  ShaderVariableManagerVk |---------------->|  ShaderVariableVkImpl[0]  |   ShaderVariableVkImpl[1]  |     ...         |
//   |__________________________|                 |___________________________|____________________________|_________________|
//
//
//
//
//   Resources in the resource cache are identified by the descriptor set index and the offset from the set start
//
//    ___________________________                  ___________________________________________________________________________
//   |                           |   unique_ptr   |                   |                 |               |                     |
//   | ShaderResourceLayoutVk    |--------------->|   VkResource[0]   |  VkResource[1]  |       ...     | VkResource[s+m+d-1] |
//   |___________________________|                |___________________|_________________|_______________|_____________________|
//                                                       |                                                            |
//                                                       |                                                            |
//                                                       | (DescriptorSet, CacheOffset)                              / (DescriptorSet, CacheOffset)
//                                                        \                                                         /
//    __________________________                   ________V_______________________________________________________V_______
//   |                          |                 |                                                                        |
//   |   ShaderResourceCacheVk  |---------------->|                                   Resources                            |
//   |__________________________|                 |________________________________________________________________________|
//
//
//
//    Every pipeline state object (PipelineStateVkImpl) keeps the following layouts:
//    * One layout object per shader stage to facilitate management of static shader resources
//      - Uses artificial layout where resource binding matches the resource type (SPIRVShaderResourceAttribs::ResourceType)
//    * One layout object per shader stage used by SRBs to manage all resource types:
//      - All variable types are preserved
//      - Bindings, descriptor sets and offsets are assigned during the initialization

#include <array>
#include <memory>
#include <unordered_map>

#include "PipelineState.h"
#include "ShaderBase.hpp"
#include "HashUtils.hpp"
#include "ShaderResourceCacheVk.hpp"
#include "VulkanUtilities/VulkanLogicalDevice.hpp"
#include "StringPool.hpp"

namespace Diligent
{

class ShaderVkImpl;

/// Diligent::ShaderResourceLayoutVk class
// sizeof(ShaderResourceLayoutVk)==40 (MS compiler, x64)
class ShaderResourceLayoutVk
{
public:
    struct ShaderStageInfo
    {
        ShaderStageInfo() {}
        ShaderStageInfo(const ShaderVkImpl* pShader);

        void   Append(const ShaderVkImpl* pShader);
        size_t Count() const;

        // Shader stage type. All shaders in the stage must have the same type.
        SHADER_TYPE Type = SHADER_TYPE_UNKNOWN;

        std::vector<const ShaderVkImpl*>   Shaders;
        std::vector<std::vector<uint32_t>> SPIRVs;
    };
    using TShaderStages = std::vector<ShaderStageInfo>;

    ShaderResourceLayoutVk(const VulkanUtilities::VulkanLogicalDevice& LogicalDevice) noexcept :
        m_LogicalDevice{LogicalDevice}
    {
#if defined(_MSC_VER) && defined(_WIN64)
        static_assert(sizeof(*this) == 40, "Unexpected sizeof(ShaderResourceLayoutVk).");
#endif
    }

    // clang-format off
    ShaderResourceLayoutVk              (const ShaderResourceLayoutVk&) = delete;
    ShaderResourceLayoutVk              (ShaderResourceLayoutVk&&)      = delete;
    ShaderResourceLayoutVk& operator =  (const ShaderResourceLayoutVk&) = delete;
    ShaderResourceLayoutVk& operator =  (ShaderResourceLayoutVk&&)      = delete;
    // clang-format on

    ~ShaderResourceLayoutVk();

    // This method is called by PipelineStateVkImpl class instance to initialize static
    // shader resource layout and the cache
    void InitializeStaticResourceLayout(const std::vector<const ShaderVkImpl*>& Shaders,
                                        IMemoryAllocator&                       LayoutDataAllocator,
                                        const PipelineResourceLayoutDesc&       ResourceLayoutDesc,
                                        ShaderResourceCacheVk&                  StaticResourceCache);

    // This method is called by PipelineStateVkImpl class instance to initialize resource
    // layouts for all shader stages in the pipeline.
    static void Initialize(IRenderDevice*                    pRenderDevice,
                           TShaderStages&                    ShaderStages,
                           ShaderResourceLayoutVk            Layouts[],
                           IMemoryAllocator&                 LayoutDataAllocator,
                           const PipelineResourceLayoutDesc& ResourceLayoutDesc,
                           class PipelineLayout&             PipelineLayout,
                           bool                              VerifyVariables,
                           bool                              VerifyImmutableSamplers);

    // sizeof(VkResource) == 32 (x64)
    struct VkResource
    {
        // clang-format off
        VkResource             (const VkResource&)  = delete;
        VkResource             (      VkResource&&) = delete;
        VkResource& operator = (const VkResource&)  = delete;
        VkResource& operator = (      VkResource&&) = delete;

        static constexpr const Uint32 CacheOffsetBits           = 21;
        static constexpr const Uint32 SamplerIndBits            = 8;
        static constexpr const Uint32 VariableTypeBits          = 2;
        static constexpr const Uint32 ImmutableSamplerFlagBits  = 1;
        static_assert(CacheOffsetBits + SamplerIndBits + VariableTypeBits + ImmutableSamplerFlagBits == 32, "Elements are expected to be packed into 32 bits");
        static_assert(SHADER_RESOURCE_VARIABLE_TYPE_NUM_TYPES <= (1<<VariableTypeBits), "Not enough bits to represent SHADER_RESOURCE_VARIABLE_TYPE");

        static constexpr const Uint32 InvalidSamplerInd  = (1 << SamplerIndBits)-1;

        static constexpr const Uint32 ResourceDimBits = 7;
        static constexpr const Uint32 IsMSFlagBits    = 8 - ResourceDimBits;
        static_assert(RESOURCE_DIM_NUM_DIMENSIONS <= (1 << ResourceDimBits), "Not enough bits to represent RESOURCE_DIMENSION");

        using ResourceType = SPIRVShaderResourceAttribs::ResourceType;

/* 0   */ const Uint16 Binding;
/* 2   */ const Uint16 DescriptorSet;

/* 4.0 */ const Uint32 CacheOffset              : CacheOffsetBits; // Offset from the beginning of the cached descriptor set
/* 6.5 */ const Uint32 SamplerInd               : SamplerIndBits;  // When using combined texture samplers, index of the separate sampler 
                                                                   // assigned to separate image
/* 7.5 */ const Uint32 VariableType             : VariableTypeBits;  
/* 7.7 */ const Uint32 ImmutableSamplerAssigned : ImmutableSamplerFlagBits;

/* 8   */ const Uint16 ArraySize;

/* 10  */ const ResourceType                Type;
/* 11.0*/ const Uint8                       ResourceDim : ResourceDimBits;
/* 11.7*/ const Uint8                       IsMS        : IsMSFlagBits;

/* 16  */ const char* const                 Name;
/* 24  */ const ShaderResourceLayoutVk&     ParentResLayout;

#ifdef DILIGENT_DEVELOPMENT
/* 32  */ const Uint32                      BufferStaticSize;
/* 36  */ const Uint32                      BufferStride;
#endif
        // clang-format on

        VkResource(const ShaderResourceLayoutVk&     _ParentLayout,
                   const char*                       _Name,
                   const SPIRVShaderResourceAttribs& _Attribs,
                   SHADER_RESOURCE_VARIABLE_TYPE     _VariableType,
                   uint32_t                          _Binding,
                   uint32_t                          _DescriptorSet,
                   Uint32                            _CacheOffset,
                   Uint32                            _SamplerInd,
                   bool                              _ImmutableSamplerAssigned) noexcept :
            // clang-format off
            Binding                  {static_cast<decltype(Binding)>(_Binding)            },
            DescriptorSet            {static_cast<decltype(DescriptorSet)>(_DescriptorSet)},
            CacheOffset              {_CacheOffset },
            SamplerInd               {_SamplerInd  },
            VariableType             {_VariableType},
            ImmutableSamplerAssigned {_ImmutableSamplerAssigned ? 1U : 0U},
            ArraySize                {_Attribs.ArraySize  },
            Type                     {_Attribs.Type       },
            ResourceDim              {_Attribs.ResourceDim},
            IsMS                     {_Attribs.IsMS       },
#ifdef DILIGENT_DEVELOPMENT
            BufferStaticSize         {_Attribs.BufferStaticSize},
            BufferStride             {_Attribs.BufferStride    },
#endif
            Name                     {_Name        },
            ParentResLayout          {_ParentLayout}
        // clang-format on
        {
#if defined(_MSC_VER) && defined(_WIN64) && !defined(DILIGENT_DEVELOPMENT)
            static_assert(sizeof(*this) == 32, "Unexpected sizeof(VkResource)");
#endif
            // clang-format off
            VERIFY(_CacheOffset   < (1 << CacheOffsetBits),                               "Cache offset (", _CacheOffset, ") exceeds max representable value ", (1 << CacheOffsetBits) );
            VERIFY(_SamplerInd    < (1 << SamplerIndBits),                                "Sampler index  (", _SamplerInd, ") exceeds max representable value ", (1 << SamplerIndBits) );
            VERIFY(_Binding       <= std::numeric_limits<decltype(Binding)>::max(),       "Binding (", _Binding, ") exceeds max representable value ", std::numeric_limits<decltype(Binding)>::max() );
            VERIFY(_DescriptorSet <= std::numeric_limits<decltype(DescriptorSet)>::max(), "Descriptor set (", _DescriptorSet, ") exceeds max representable value ", std::numeric_limits<decltype(DescriptorSet)>::max());
            VERIFY(_VariableType  < (1 << VariableTypeBits),                              "Variable type (", Uint32{_VariableType}, ") exceeds max representable value ", (1 << VariableTypeBits) );
            VERIFY(_Attribs.ResourceDim < (1 << ResourceDimBits),                         "Resource dimension (", Uint32{_Attribs.ResourceDim}, ") exceeds max representable value ", (1 << ResourceDimBits) );
            // clang-format on
        }


        // Checks if a resource is bound in ResourceCache at the given ArrayIndex
        bool IsBound(Uint32 ArrayIndex, const ShaderResourceCacheVk& ResourceCache) const;

        // Binds a resource pObject in the ResourceCache
        void BindResource(IDeviceObject* pObject, Uint32 ArrayIndex, ShaderResourceCacheVk& ResourceCache) const;

        // Updates resource descriptor in the descriptor set
        inline void UpdateDescriptorHandle(VkDescriptorSet                                     vkDescrSet,
                                           uint32_t                                            ArrayElement,
                                           const VkDescriptorImageInfo*                        pImageInfo,
                                           const VkDescriptorBufferInfo*                       pBufferInfo,
                                           const VkBufferView*                                 pTexelBufferView,
                                           const VkWriteDescriptorSetAccelerationStructureKHR* pAccelStructInfo = nullptr) const;

        bool IsImmutableSamplerAssigned() const
        {
            VERIFY(ImmutableSamplerAssigned == 0 ||
                       Type == SPIRVShaderResourceAttribs::ResourceType::SampledImage ||
                       Type == SPIRVShaderResourceAttribs::ResourceType::SeparateSampler,
                   "Immutable sampler can only be assigned to a sampled image or separate sampler");
            return ImmutableSamplerAssigned != 0;
        }

        SHADER_RESOURCE_VARIABLE_TYPE GetVariableType() const
        {
            return static_cast<SHADER_RESOURCE_VARIABLE_TYPE>(VariableType);
        }

        String GetPrintName(Uint32 ArrayInd) const
        {
            VERIFY_EXPR(ArrayInd < ArraySize);
            if (ArraySize > 1)
            {
                std::stringstream ss;
                ss << Name << '[' << ArrayInd << ']';
                return ss.str();
            }
            else
                return Name;
        }

        ShaderResourceDesc GetResourceDesc() const
        {
            return ShaderResourceDesc{Name, SPIRVShaderResourceAttribs::GetShaderResourceType(Type), ArraySize};
        }

        RESOURCE_DIMENSION GetResourceDimension() const
        {
            return static_cast<RESOURCE_DIMENSION>(ResourceDim);
        }

        bool IsMultisample() const
        {
            return IsMS != 0;
        }

        bool IsCompatibleWith(const VkResource& rhs) const
        {
            // clang-format off
            return Binding       == rhs.Binding       &&
                   DescriptorSet == rhs.DescriptorSet &&
                   ArraySize     == rhs.ArraySize     &&
                   Type          == rhs.Type;
            // clang-format on
        }

    private:
        void CacheUniformBuffer(IDeviceObject*                   pBuffer,
                                ShaderResourceCacheVk::Resource& DstRes,
                                VkDescriptorSet                  vkDescrSet,
                                Uint32                           ArrayInd,
                                Uint16&                          DynamicBuffersCounter) const;

        void CacheStorageBuffer(IDeviceObject*                   pBufferView,
                                ShaderResourceCacheVk::Resource& DstRes,
                                VkDescriptorSet                  vkDescrSet,
                                Uint32                           ArrayInd,
                                Uint16&                          DynamicBuffersCounter) const;

        void CacheTexelBuffer(IDeviceObject*                   pBufferView,
                              ShaderResourceCacheVk::Resource& DstRes,
                              VkDescriptorSet                  vkDescrSet,
                              Uint32                           ArrayInd,
                              Uint16&                          DynamicBuffersCounter) const;

        template <typename TCacheSampler>
        void CacheImage(IDeviceObject*                   pTexView,
                        ShaderResourceCacheVk::Resource& DstRes,
                        VkDescriptorSet                  vkDescrSet,
                        Uint32                           ArrayInd,
                        TCacheSampler                    CacheSampler) const;

        void CacheSeparateSampler(IDeviceObject*                   pSampler,
                                  ShaderResourceCacheVk::Resource& DstRes,
                                  VkDescriptorSet                  vkDescrSet,
                                  Uint32                           ArrayInd) const;

        void CacheInputAttachment(IDeviceObject*                   pTexView,
                                  ShaderResourceCacheVk::Resource& DstRes,
                                  VkDescriptorSet                  vkDescrSet,
                                  Uint32                           ArrayInd) const;

        void CacheAccelerationStructure(IDeviceObject*                   pTLAS,
                                        ShaderResourceCacheVk::Resource& DstRes,
                                        VkDescriptorSet                  vkDescrSet,
                                        Uint32                           ArrayInd) const;

        template <typename ObjectType, typename TPreUpdateObject>
        bool UpdateCachedResource(ShaderResourceCacheVk::Resource& DstRes,
                                  RefCntAutoPtr<ObjectType>&&      pObject,
                                  TPreUpdateObject                 PreUpdateObject) const;
    };

    // Copies static resources from SrcResourceCache defined by SrcLayout
    // to DstResourceCache defined by this layout
    void InitializeStaticResources(const ShaderResourceLayoutVk& SrcLayout,
                                   const ShaderResourceCacheVk&  SrcResourceCache,
                                   ShaderResourceCacheVk&        DstResourceCache) const;

#ifdef DILIGENT_DEVELOPMENT
    bool        dvpVerifyBindings(const ShaderResourceCacheVk& ResourceCache) const;
    static void dvpVerifyResourceLayoutDesc(const TShaderStages&              ShaderStages,
                                            const PipelineResourceLayoutDesc& ResourceLayoutDesc,
                                            bool                              VerifyVariables,
                                            bool                              VerifyImmutableSamplers);
#endif

    Uint32 GetResourceCount(SHADER_RESOURCE_VARIABLE_TYPE VarType) const
    {
        return m_NumResources[VarType];
    }

    // Initializes resource slots in the ResourceCache
    void InitializeResourceMemoryInCache(ShaderResourceCacheVk& ResourceCache) const;

    // Writes dynamic resource descriptors from ResourceCache to vkDynamicDescriptorSet
    void CommitDynamicResources(const ShaderResourceCacheVk& ResourceCache,
                                VkDescriptorSet              vkDynamicDescriptorSet) const;

    const Char* GetShaderName() const
    {
        return GetStringPoolData();
    }

    SHADER_TYPE GetShaderType() const { return m_ShaderType; }

    const VkResource& GetResource(SHADER_RESOURCE_VARIABLE_TYPE VarType, Uint32 r) const
    {
        VERIFY_EXPR(r < m_NumResources[VarType]);
        auto* Resources = reinterpret_cast<const VkResource*>(m_ResourceBuffer.get());
        return Resources[GetResourceOffset(VarType, r)];
    }

    bool IsUsingSeparateSamplers() const { return m_IsUsingSeparateSamplers; }

    bool IsCompatibleWith(const ShaderResourceLayoutVk& ResLayout) const;

private:
    Uint32 GetResourceOffset(SHADER_RESOURCE_VARIABLE_TYPE VarType, Uint32 r) const
    {
        VERIFY_EXPR(r < m_NumResources[VarType]);
        static_assert(SHADER_RESOURCE_VARIABLE_TYPE_STATIC == 0, "SHADER_RESOURCE_VARIABLE_TYPE_STATIC == 0 expected");
        r += (VarType > SHADER_RESOURCE_VARIABLE_TYPE_STATIC) ? m_NumResources[SHADER_RESOURCE_VARIABLE_TYPE_STATIC] : 0;
        static_assert(SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE == 1, "SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE == 1 expected");
        r += (VarType > SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE) ? m_NumResources[SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE] : 0;
        return r;
    }
    VkResource& GetResource(SHADER_RESOURCE_VARIABLE_TYPE VarType, Uint32 r)
    {
        VERIFY_EXPR(r < m_NumResources[VarType]);
        auto* Resources = reinterpret_cast<VkResource*>(m_ResourceBuffer.get());
        return Resources[GetResourceOffset(VarType, r)];
    }

    const VkResource& GetResource(Uint32 r) const
    {
        VERIFY_EXPR(r < GetTotalResourceCount());
        const auto* Resources = reinterpret_cast<const VkResource*>(m_ResourceBuffer.get());
        return Resources[r];
    }

    Uint32 GetTotalResourceCount() const
    {
        return m_NumResources[SHADER_RESOURCE_VARIABLE_TYPE_NUM_TYPES];
    }

    const char* GetStringPoolData() const
    {
        const auto* ResourceDataEnd = reinterpret_cast<const VkResource*>(m_ResourceBuffer.get()) + GetTotalResourceCount();
        const auto* SamplerDataEnd  = reinterpret_cast<const ImmutableSamplerPtrType*>(ResourceDataEnd) + m_NumImmutableSamplers;
        return reinterpret_cast<const char*>(SamplerDataEnd);
    }

    static constexpr const Uint32 InvalidResourceIndex = ~0u;

    // Maps resource name to its index in m_ResourceBuffer
    using ResourceNameToIndex_t = std::unordered_map<HashMapStringKey, Uint32, HashMapStringKey::Hasher>;
    StringPool AllocateMemory(const std::vector<const ShaderVkImpl*>& Shaders,
                              IMemoryAllocator&                       Allocator,
                              const PipelineResourceLayoutDesc&       ResourceLayoutDesc,
                              const SHADER_RESOURCE_VARIABLE_TYPE*    AllowedVarTypes,
                              Uint32                                  NumAllowedTypes,
                              ResourceNameToIndex_t&                  UniqueNames,
                              bool                                    AllocateImmutableSamplers);

    using ImmutableSamplerPtrType = RefCntAutoPtr<ISampler>;
    ImmutableSamplerPtrType& GetImmutableSampler(Uint32 n) noexcept
    {
        VERIFY(n < m_NumImmutableSamplers, "Immutable sampler index (", n, ") is out of range. Total immutable sampler count: ", m_NumImmutableSamplers);
        auto* ResourceMemoryEnd = reinterpret_cast<VkResource*>(m_ResourceBuffer.get()) + GetTotalResourceCount();
        return reinterpret_cast<ImmutableSamplerPtrType*>(ResourceMemoryEnd)[n];
    }

    // clang-format off
/* 0 */ const VulkanUtilities::VulkanLogicalDevice&    m_LogicalDevice;
/* 8 */ std::unique_ptr<void, STDDeleterRawMem<void>>  m_ResourceBuffer;

/*24 */ std::array<Uint16, SHADER_RESOURCE_VARIABLE_TYPE_NUM_TYPES+1>  m_NumResources = {};

/*32 */ Uint16      m_NumImmutableSamplers    = 0;
/*34 */ bool        m_IsUsingSeparateSamplers = false;
/*36 */ SHADER_TYPE m_ShaderType              = SHADER_TYPE_UNKNOWN;

/*40 */  // End of class
    // clang-format on
};

} // namespace Diligent
