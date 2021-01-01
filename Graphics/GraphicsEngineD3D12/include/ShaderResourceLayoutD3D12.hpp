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
/// Declaration of Diligent::ShaderResourceLayoutD3D12 class

// http://diligentgraphics.com/diligent-engine/architecture/d3d12/shader-resource-layout/

// All resources are stored in a single continuous chunk of memory using the following layout:
//
//   m_ResourceBuffer
//      |                         |                         |
//      | D3D12Resource[0]  ...   | D3D12Resource[s]  ...   | D3D12Resource[s+m]  ...  | D3D12Resource[smd]  ...  | D3D12Resource[smd+s']  ...  | D3D12Resource[smd+s'+m']  ...  D3D12Resource[s+m+d+s'+m'+d'-1] ||
//      |                         |                         |                          |                          |                             |                                                                ||
//      |  SRV/CBV/UAV - STATIC   |  SRV/CBV/UAV - MUTABLE  |   SRV/CBV/UAV - DYNAMIC  |   Samplers - STATIC      |  Samplers - MUTABLE         |   Samplers - DYNAMIC                                           ||
//      |                         |                         |                          |
//
//      s == NumCbvSrvUav[SHADER_RESOURCE_VARIABLE_TYPE_STATIC]
//      m == NumCbvSrvUav[SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE]
//      d == NumCbvSrvUav[SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC]
//      smd = s+m+d
//
//      s' == NumSamplers[SHADER_RESOURCE_VARIABLE_TYPE_STATIC]
//      m' == NumSamplers[SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE]
//      d' == NumSamplers[SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC]
//
//
//
//    ___________________________                  ____________________________________________________________________________________________________________
//   |                           |   unique_ptr   |                  |                  |               |                    |                      |          |
//   | ShaderResourceLayoutD3D12 |--------------->| D3D12Resource[0] | D3D12Resource[1] |       ...     | D3D12Resource[smd] | D3D12Resource[smd+1] |   ...    |
//   |___________________________|                |__________________|__________________|_______________|____________________|______________________|__________|
//                A                                    A    |             A                                      A
//                |                                     \   |______________\____SamplerId________________________|
//                |                                      \                  \                                              
//                |                                      Ref                Ref
//                |                                        \                  \_____
//                |                                         \                       \             
//    ____________|_______________                   ________\_______________________\__________________________________________
//   |                            |                 |                            |                            |                 |
//   | ShaderVariableManagerD3D12 |---------------->| ShaderVariableD3D12Impl[0] | ShaderVariableD3D12Impl[1] |     ...         |
//   |____________________________|                 |____________________________|____________________________|_________________|
//
//
//
//
//
//  One ShaderResourceLayoutD3D12 instance can be referenced by multiple objects
//
//
//             ________________________           _<m_pShaderResourceLayouts>_          _____<m_pShaderVarMgrs>_____       ________________________________
//            |                        |         |                            |        |                            |     |                                |
//            | PipelineStateD3D12Impl |========>| ShaderResourceLayoutD3D12  |<-------| ShaderVariableManagerD3D12 |<====| ShaderResourceBindingD3D12Impl |
//            |________________________|         |____________________________|        |____________________________|     |________________________________|
//                                                                         A
//                                                                          \         
//                                                                           \          _____<m_pShaderVarMgrs>_____       ________________________________
//                                                                            \        |                            |     |                                |
//                                                                             '-------| ShaderVariableManagerD3D12 |<====| ShaderResourceBindingD3D12Impl |
//                                                                                     |____________________________|     |________________________________|
//
//
//
//   Resources in the resource cache are identified by the root index and offset in the descriptor table
//
//
//    ShaderResourceLayoutD3D12 is used as follows:
//    * Every pipeline state object (PipelineStateD3D12Impl) maintains shader resource layout for every active shader stage
//      ** These resource layouts are used as reference layouts for shader resource binding objects
//      ** All variable types are preserved
//      ** Root indices and descriptor table offsets are assigned during the initialization
//    * Every pipeline state object also contains shader resource layout that facilitates management of static shader resources
//      ** The resource layout defines artificial layout where root index matches the resource type (CBV/SRV/UAV/SAM)
//      ** Only static variables are referenced
//
//    * Every shader resource binding object (ShaderResourceBindingD3D12Impl) encompasses shader variable
//      manager (ShaderVariableManagerD3D12) for every active shader stage in the parent pipeline state that
//      handles mutable and dynamic resources

#include <array>

#include "ShaderBase.hpp"
#include "ShaderResourceCacheD3D12.hpp"
#include "ShaderD3D12Impl.hpp"
#include "StringPool.hpp"
#include "D3DCommonTypeConversions.hpp"

namespace Diligent
{

/// Diligent::ShaderResourceLayoutD3D12 class
// sizeof(ShaderResourceLayoutD3D12) == 56 (MS compiler, x64)
class ShaderResourceLayoutD3D12 final
{
public:
    ShaderResourceLayoutD3D12(IObject& Owner, ID3D12Device* pd3d12Device) noexcept :
        m_Owner{Owner},
        m_pd3d12Device{pd3d12Device}
    {
#if defined(_MSC_VER) && defined(_WIN64)
        static_assert(sizeof(*this) == 56, "Unexpected sizeof(ShaderResourceLayoutD3D12)");
#endif
    }

    // Initializes reference layouts that address all types of resources (static, mutable, dynamic).
    // Root indices and descriptor table offsets are assigned during the initialization.
    void Initialize(PIPELINE_TYPE                        PipelineType,
                    const PipelineResourceLayoutDesc&    ResourceLayout,
                    const std::vector<ShaderD3D12Impl*>& Shaders,
                    IMemoryAllocator&                    LayoutDataAllocator,
                    class RootSignatureBuilder&          RootSgnBldr,
                    class LocalRootSignature*            pLocalRootSig);

    // Copies static resources from the source layout and initializes the
    // resource cache. Uses bind points from the source layout.
    void InitializeStaticReourceLayout(const ShaderResourceLayoutD3D12& SrcLayout,
                                       IMemoryAllocator&                LayoutDataAllocator,
                                       ShaderResourceCacheD3D12&        ResourceCache);

    // clang-format off
    ShaderResourceLayoutD3D12            (const ShaderResourceLayoutD3D12&) = delete;
    ShaderResourceLayoutD3D12            (ShaderResourceLayoutD3D12&&)      = delete;
    ShaderResourceLayoutD3D12& operator =(const ShaderResourceLayoutD3D12&) = delete;
    ShaderResourceLayoutD3D12& operator =(ShaderResourceLayoutD3D12&&)      = delete;
    // clang-format on

    ~ShaderResourceLayoutD3D12();

    // sizeof(D3D12Resource) == 32 (x64)
    struct D3D12Resource final
    {
        // clang-format off
        D3D12Resource           (const D3D12Resource&)  = delete;
        D3D12Resource           (      D3D12Resource&&) = delete;
        D3D12Resource& operator=(const D3D12Resource&)  = delete;
        D3D12Resource& operator=(      D3D12Resource&&) = delete;
        // clang-format on

        static constexpr Uint32 ResourceTypeBits = 3;
        static constexpr Uint32 VariableTypeBits = 2;
        static constexpr Uint32 RootIndexBits    = 32 - ResourceTypeBits - VariableTypeBits;

        static constexpr Uint32 InvalidRootIndex = (1U << RootIndexBits) - 1U;
        static constexpr Uint32 MaxRootIndex     = InvalidRootIndex - 1U;

        static constexpr Uint32 InvalidOffset = static_cast<Uint32>(-1);

        static_assert(SHADER_RESOURCE_VARIABLE_TYPE_NUM_TYPES < (1 << VariableTypeBits), "Not enough bits to represent SHADER_RESOURCE_VARIABLE_TYPE");
        static_assert(static_cast<int>(CachedResourceType::NumTypes) < (1 << ResourceTypeBits), "Not enough bits to represent CachedResourceType");

        /* 0  */ const ShaderResourceLayoutD3D12& ParentResLayout;
        /* 8  */ const D3DShaderResourceAttribs   Attribs; // Copy of the attributes, potentially with some changes to bindings
        /*24  */ const Uint32                     OffsetFromTableStart;
        /*28.0*/ const Uint32                     ResourceType : ResourceTypeBits; // | 0 1 2 |
        /*28.3*/ const Uint32                     VariableType : VariableTypeBits; //         | 3 4 |
        /*28.5*/ const Uint32                     RootIndex : RootIndexBits;       //               | 5 6 7 ... 15 |
        /*32  */                                                                   // End of data

        D3D12Resource(const ShaderResourceLayoutD3D12& _ParentLayout,
                      StringPool&                      _StringPool,
                      const D3DShaderResourceAttribs&  _Attribs,
                      Uint32                           _SamplerId,
                      SHADER_RESOURCE_VARIABLE_TYPE    _VariableType,
                      CachedResourceType               _ResType,
                      Uint32                           _BindPoint,
                      Uint32                           _RootIndex,
                      Uint32                           _OffsetFromTableStart) noexcept :
            // clang-format off
            ParentResLayout{_ParentLayout},
            Attribs 
            {
                _StringPool,
                _Attribs,
                _SamplerId,
                _BindPoint
            },
            ResourceType        {static_cast<Uint32>(_ResType)     },
            VariableType        {static_cast<Uint32>(_VariableType)},
            RootIndex           {static_cast<Uint32>(_RootIndex)   },
            OffsetFromTableStart{ _OffsetFromTableStart            }
        // clang-format on
        {
#if defined(_MSC_VER) && defined(_WIN64)
            static_assert(sizeof(*this) == 32, "Unexpected sizeof(D3D12Resource)");
#endif

            VERIFY(IsValidOffset(), "Offset must be valid");
            VERIFY(IsValidRootIndex(), "Root index must be valid");
            VERIFY(_RootIndex <= MaxRootIndex, "Root index (", _RootIndex, ") exceeds max allowed value (", MaxRootIndex, ")");
            VERIFY(static_cast<Uint32>(_ResType) < (1 << ResourceTypeBits), "Resource type is out of representable range");
            VERIFY(_VariableType < (1 << VariableTypeBits), "Variable type is out of representable range");
        }

        bool IsBound(Uint32                          ArrayIndex,
                     const ShaderResourceCacheD3D12& ResourceCache) const;

        void BindResource(IDeviceObject*            pObject,
                          Uint32                    ArrayIndex,
                          ShaderResourceCacheD3D12& ResourceCache) const;

        // clang-format off
        bool IsValidRootIndex() const { return RootIndex            != InvalidRootIndex; }
        bool IsValidOffset()    const { return OffsetFromTableStart != InvalidOffset;    }
        // clang-format on

        CachedResourceType            GetResType() const { return static_cast<CachedResourceType>(ResourceType); }
        SHADER_RESOURCE_VARIABLE_TYPE GetVariableType() const { return static_cast<SHADER_RESOURCE_VARIABLE_TYPE>(VariableType); }

    private:
        void CacheCB(IDeviceObject*                      pBuffer,
                     ShaderResourceCacheD3D12::Resource& DstRes,
                     Uint32                              ArrayInd,
                     D3D12_CPU_DESCRIPTOR_HANDLE         ShdrVisibleHeapCPUDescriptorHandle,
                     Uint32&                             BoundDynamicCBsCounter) const;

        template <typename TResourceViewType,
                  typename TViewTypeEnum,
                  typename TBindSamplerProcType>
        void CacheResourceView(IDeviceObject*                      pView,
                               ShaderResourceCacheD3D12::Resource& DstRes,
                               Uint32                              ArrayIndex,
                               D3D12_CPU_DESCRIPTOR_HANDLE         ShdrVisibleHeapCPUDescriptorHandle,
                               TViewTypeEnum                       dbgExpectedViewType,
                               TBindSamplerProcType                BindSamplerProc) const;

        void CacheSampler(IDeviceObject*                      pSampler,
                          ShaderResourceCacheD3D12::Resource& DstSam,
                          Uint32                              ArrayIndex,
                          D3D12_CPU_DESCRIPTOR_HANDLE         ShdrVisibleHeapCPUDescriptorHandle) const;

        void CacheAccelStruct(IDeviceObject*                      pTLAS,
                              ShaderResourceCacheD3D12::Resource& DstRes,
                              Uint32                              ArrayIndex,
                              D3D12_CPU_DESCRIPTOR_HANDLE         ShdrVisibleHeapCPUDescriptorHandle) const;
    };

    void CopyStaticResourceDesriptorHandles(const ShaderResourceCacheD3D12&  SrcCache,
                                            const ShaderResourceLayoutD3D12& DstLayout,
                                            ShaderResourceCacheD3D12&        DstCache) const;

#ifdef DILIGENT_DEVELOPMENT
    bool dvpVerifyBindings(const ShaderResourceCacheD3D12& ResourceCache) const;
#endif

    IObject& GetOwner()
    {
        return m_Owner;
    }

    Uint32 GetCbvSrvUavCount(SHADER_RESOURCE_VARIABLE_TYPE VarType) const
    {
        return m_CbvSrvUavOffsets[VarType + 1] - m_CbvSrvUavOffsets[VarType];
    }
    Uint32 GetSamplerCount(SHADER_RESOURCE_VARIABLE_TYPE VarType) const
    {
        return m_SamplersOffsets[VarType + 1] - m_SamplersOffsets[VarType];
    }
    Uint32 GetTotalResourceCount() const
    {
        return m_SamplersOffsets[SHADER_RESOURCE_VARIABLE_TYPE_NUM_TYPES];
    }

    const D3D12Resource& GetSrvCbvUav(SHADER_RESOURCE_VARIABLE_TYPE VarType, Uint32 r) const
    {
        VERIFY_EXPR(r < GetCbvSrvUavCount(VarType));
        return GetResource(GetSrvCbvUavOffset(VarType, r));
    }
    const D3D12Resource& GetSampler(SHADER_RESOURCE_VARIABLE_TYPE VarType, Uint32 s) const
    {
        VERIFY_EXPR(s < GetSamplerCount(VarType));
        return GetResource(GetSamplerOffset(VarType, s));
    }

    const D3D12Resource& GetResource(Uint32 r) const
    {
        VERIFY_EXPR(r < GetTotalResourceCount());
        const auto* Resources = reinterpret_cast<const D3D12Resource*>(m_ResourceBuffer.get());
        return Resources[r];
    }

    const bool IsUsingSeparateSamplers() const { return m_IsUsingSeparateSamplers; }

    SHADER_TYPE GetShaderType() const { return m_ShaderType; }

    bool IsCompatibleWith(const ShaderResourceLayoutD3D12& ResLayout) const;

private:
    const D3D12Resource& GetAssignedSampler(const D3D12Resource& TexSrv) const;
    D3D12Resource&       GetAssignedSampler(const D3D12Resource& TexSrv);

    Uint32 FindSamplerByName(const char* SamplerName) const;

    const Char* GetShaderName() const
    {
        return GetStringPoolData();
    }


    Uint32 GetTotalSrvCbvUavCount() const
    {
        VERIFY_EXPR(m_CbvSrvUavOffsets[0] == 0);
        return m_CbvSrvUavOffsets[SHADER_RESOURCE_VARIABLE_TYPE_NUM_TYPES];
    }
    Uint32 GetTotalSamplerCount() const
    {
        return m_SamplersOffsets[SHADER_RESOURCE_VARIABLE_TYPE_NUM_TYPES] - m_SamplersOffsets[0];
    }

    D3D12Resource& GetResource(Uint32 r)
    {
        VERIFY_EXPR(r < GetTotalResourceCount());
        auto* Resources = reinterpret_cast<D3D12Resource*>(m_ResourceBuffer.get());
        return Resources[r];
    }

    Uint32 GetSrvCbvUavOffset(SHADER_RESOURCE_VARIABLE_TYPE VarType, Uint32 r) const
    {
        Uint32 Offset = m_CbvSrvUavOffsets[VarType] + r;
        VERIFY_EXPR(Offset < m_CbvSrvUavOffsets[VarType + 1]);
        return Offset;
    }
    D3D12Resource& GetSrvCbvUav(SHADER_RESOURCE_VARIABLE_TYPE VarType, Uint32 r)
    {
        VERIFY_EXPR(r < GetCbvSrvUavCount(VarType));
        return GetResource(GetSrvCbvUavOffset(VarType, r));
    }
    const D3D12Resource& GetSrvCbvUav(Uint32 r) const
    {
        VERIFY_EXPR(r < GetTotalSrvCbvUavCount());
        return GetResource(m_CbvSrvUavOffsets[0] + r);
    }


    Uint32 GetSamplerOffset(SHADER_RESOURCE_VARIABLE_TYPE VarType, Uint32 s) const
    {
        auto Offset = m_SamplersOffsets[VarType] + s;
        VERIFY_EXPR(Offset < m_SamplersOffsets[VarType + 1]);
        return Offset;
    }
    D3D12Resource& GetSampler(SHADER_RESOURCE_VARIABLE_TYPE VarType, Uint32 s)
    {
        VERIFY_EXPR(s < GetSamplerCount(VarType));
        return GetResource(GetSamplerOffset(VarType, s));
    }
    const D3D12Resource& GetSampler(Uint32 s) const
    {
        VERIFY_EXPR(s < GetTotalSamplerCount());
        return GetResource(m_SamplersOffsets[0] + s);
    }

    const char* GetStringPoolData() const
    {
        const auto* Resources = reinterpret_cast<const D3D12Resource*>(m_ResourceBuffer.get());
        return reinterpret_cast<const char*>(Resources + GetTotalResourceCount());
    }

    StringPool AllocateMemory(IMemoryAllocator&                                                  Allocator,
                              const std::array<Uint32, SHADER_RESOURCE_VARIABLE_TYPE_NUM_TYPES>& CbvSrvUavCount,
                              const std::array<Uint32, SHADER_RESOURCE_VARIABLE_TYPE_NUM_TYPES>& SamplerCount,
                              size_t                                                             StringPoolSize);
    // clang-format off

/*  0 */ std::unique_ptr<void, STDDeleterRawMem<void> >                  m_ResourceBuffer;
/* 16 */ std::array<Uint16, SHADER_RESOURCE_VARIABLE_TYPE_NUM_TYPES + 1> m_CbvSrvUavOffsets = {};
/* 24 */ std::array<Uint16, SHADER_RESOURCE_VARIABLE_TYPE_NUM_TYPES + 1> m_SamplersOffsets  = {};

/* 32 */ IObject&              m_Owner;
/* 40 */ CComPtr<ID3D12Device> m_pd3d12Device;
/* 48 */ SHADER_TYPE           m_ShaderType              = SHADER_TYPE_UNKNOWN;
/*    */ bool                  m_IsUsingSeparateSamplers = false;
/* 56 */ // End of data

    // clang-format on
};

} // namespace Diligent
