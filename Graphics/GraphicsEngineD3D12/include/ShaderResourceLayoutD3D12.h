/*     Copyright 2015-2018 Egor Yusov
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
/// Declaration of Diligent::ShaderResourceLayoutD3D12 class

// http://diligentgraphics.com/diligent-engine/architecture/d3d12/shader-resource-layout/

// All resources are stored in a single continuous chunk of memory using the following layout:
//
//   m_ResourceBuffer                                                                                                                             m_Samplers  
//      |                                                                                                                                            |     
//      |   SRV_CBV_UAV[0]  ...  SRV_CBV_UAV[s-1]   |   SRV_CBV_UAV[s]  ...  SRV_CBV_UAV[s+m-1]   |   SRV_CBV_UAV[s+m]  ...  SRV_CBV_UAV[s+m+d-1]   ||   Sampler[0]  ...  Sampler[s'-1]   |   Sampler[s']  ...  Sampler[s'+m'-1]   |   Sampler[s'+m']  ...  Sampler[s'+m'+d'-1]    ||
//      |                                           |                                             |                                                 ||                                    |                                        |                                               ||
//      |        SHADER_VARIABLE_TYPE_STATIC        |          SHADER_VARIABLE_TYPE_MUTABLE       |            SHADER_VARIABLE_TYPE_DYNAMIC         ||    SHADER_VARIABLE_TYPE_STATIC     |       SHADER_VARIABLE_TYPE_MUTABLE     |          SHADER_VARIABLE_TYPE_DYNAMIC         ||
//      |                                           |                                             |                                                 ||             
//
//      s == m_NumCbvSrvUav[SHADER_VARIABLE_TYPE_STATIC]
//      m == m_NumCbvSrvUav[SHADER_VARIABLE_TYPE_MUTABLE]
//      d == m_NumCbvSrvUav[SHADER_VARIABLE_TYPE_DYNAMIC]
//
//      s' == m_NumSamplers[SHADER_VARIABLE_TYPE_STATIC]
//      m' == m_NumSamplers[SHADER_VARIABLE_TYPE_MUTABLE]
//      d' == m_NumSamplers[SHADER_VARIABLE_TYPE_DYNAMIC]
//
//
//   Memory buffer is allocated through the allocator provided by the pipeline state. If allocation granularity > 1, fixed block
//   memory allocator is used. This ensures that all resources from different shader resource bindings reside in
//   continuous memory. If allocation granularity == 1, raw allocator is used.
//
//
//   Every SRV_CBV_UAV and Sampler structure holds a reference to D3DShaderResourceAttribs structure from ShaderResources.
//   ShaderResourceLayoutD3D12 holds shared pointer to ShaderResourcesD3D12 instance. Note that ShaderResources::SamplerId 
//   references a sampler in ShaderResources, while SRV_CBV_UAV::SamplerId references a sampler in ShaderResourceLayoutD3D12, 
//   and the two are not the same
//
//
//                                                      ________________SamplerId____________________
//                                                     |                                             |
//    _________________                  ______________|_____________________________________________V________
//   |                 |  unique_ptr    |        |           |           |           |           |            |
//   | ShaderResources |--------------->|   CBs  |  TexSRVs  |  TexUAVs  |  BufSRVs  |  BufUAVs  |  Samplers  |
//   |_________________|                |________|___________|___________|___________|___________|____________|
//            A                                         A                              A                   A  
//            |                                          \                            /                     \
//            |shared_ptr                                Ref                        Ref                     Ref
//    ________|__________________                  ________\________________________/_________________________\_________________________________________
//   |                           |   unique_ptr   |                   |                 |               |                  |                 |          |
//   | ShaderResourceLayoutD3D12 |--------------->|   SRV_CBV_UAV[0]  |  SRV_CBV_UAV[1] |       ...     |    Sampler[0]    |    Sampler[1]   |   ...    |
//   |___________________________|                |___________________|_________________|_______________|__________________|_________________|__________|
//            |                                          |  |                                                    A    |     /        
//            | Raw ptr                                  |  |___________________SamplerId________________________|    |    /
//            |                                          |                                                           /    /
//            |                                           \                                                         /    /
//    ________V_________________                   ________V_______________________________________________________V____V__                     
//   |                          |                 |                                                                        |                   
//   | ShaderResourceCacheD3D12 |---------------->|                                   Resources                            | 
//   |__________________________|                 |________________________________________________________________________|                   
//
//   http://diligentgraphics.com/diligent-engine/architecture/d3d12/shader-resource-layout#Figure2
//   Resources in the resource cache are identified by the root index and offset in the descriptor table
//
//                                
//    ShaderResourceLayoutD3D12 is used as follows:
//    * Every pipeline state object (PipelineStateD3D12Impl) maintains shader resource layout for every active shader stage
//      ** These resource layouts are not bound to a resource cache and are used as reference layouts for shader resource binding objects
//      ** All variable types are preserved
//      ** Root indices and descriptor table offsets are assigned during the initialization
//      ** Resource cache is not assigned
//    * Every shader object (ShaderD3D12Impl) contains shader resource layout that facilitates management of static shader resources
//      ** The resource layout defines artificial layout and is bound to a resource cache that actually holds references to resources
//      ** Resource cache is assigned and initialized
//    * Every shader resource binding object (ShaderResourceBindingD3D12Impl) encompasses shader resource layout for every active shader 
//      stage in the parent pipeline state
//      ** Resource layouts are initialized by clonning reference layouts from the pipeline state object and are bound to the resource 
//         cache that holds references to resources set by the application
//      ** All shader variable types are clonned
//      ** Resource cache is assigned, but not initialized; Initialization is performed by the root signature
//

// Set this define to 1 to use unordered_map to store shader variables. 
// Note that sizeof(m_VariableHash)==128 (release mode, MS compiler, x64).
#define USE_VARIABLE_HASH_MAP 0

#include <unordered_map>
#include <array>

#include "ShaderD3DBase.h"
#include "ShaderBase.h"
#include "HashUtils.h"
#include "ShaderResourcesD3D12.h"
#include "ShaderResourceCacheD3D12.h"
#include "ShaderVariableD3DBase.h"

#ifdef _DEBUG
#   define VERIFY_SHADER_BINDINGS
#endif

namespace Diligent
{

/// Diligent::ShaderResourceLayoutD3D12 class
// sizeof(ShaderResourceLayoutD3D12)==80 (MS compiler, x64)
class ShaderResourceLayoutD3D12
{
public:
    ShaderResourceLayoutD3D12(IObject& Owner, IMemoryAllocator& ResourceLayoutDataAllocator);

    // This constructor is used by ShaderResourceBindingD3D12Impl to clone layout from the reference layout in PipelineStateD3D12Impl. 
    // Root indices and descriptor table offsets must be correct. Resource cache is assigned, but not initialized.
    ShaderResourceLayoutD3D12(IObject&                         Owner, 
                              const ShaderResourceLayoutD3D12& SrcLayout, 
                              IMemoryAllocator&                ResourceLayoutDataAllocator,
                              const SHADER_VARIABLE_TYPE*      AllowedVarTypes, 
                              Uint32                           NumAllowedTypes, 
                              ShaderResourceCacheD3D12&        ResourceCache);

    ShaderResourceLayoutD3D12            (const ShaderResourceLayoutD3D12&) = delete;
    ShaderResourceLayoutD3D12            (ShaderResourceLayoutD3D12&&)      = delete;
    ShaderResourceLayoutD3D12& operator =(const ShaderResourceLayoutD3D12&) = delete;
    ShaderResourceLayoutD3D12& operator =(ShaderResourceLayoutD3D12&&)      = delete;
    
    ~ShaderResourceLayoutD3D12();

    //  The method is called by
    //  - ShaderD3D12Impl class instance to initialize static resource layout and initialize shader resource cache
    //    to hold static resources
    //  - PipelineStateD3D12Impl class instance to reference all types of resources (static, mutable, dynamic). 
    //    Root indices and descriptor table offsets are assigned during the initialization; 
    //    no shader resource cache is provided
    void Initialize(ID3D12Device*                                      pd3d12Device,
                    const std::shared_ptr<const ShaderResourcesD3D12>& pSrcResources, 
                    IMemoryAllocator&                                  LayoutDataAllocator,
                    const SHADER_VARIABLE_TYPE*                        VarTypes, 
                    Uint32                                             NumAllowedTypes, 
                    ShaderResourceCacheD3D12*                          pResourceCache,
                    class RootSignature*                               pRootSig);

    // sizeof(SRV_CBV_UAV) == 32 (x64)
    struct SRV_CBV_UAV : ShaderVariableD3DBase<ShaderResourceLayoutD3D12>
    {
        SRV_CBV_UAV             (const SRV_CBV_UAV&) = delete;
        SRV_CBV_UAV             (SRV_CBV_UAV&&)      = delete;
        SRV_CBV_UAV& operator = (const SRV_CBV_UAV&) = delete;
        SRV_CBV_UAV& operator = (SRV_CBV_UAV&&)      = delete;

        static constexpr Uint32 ResTypeBits = 3;
        static constexpr Uint32 RootIndBits = 16-ResTypeBits;
        static constexpr Uint32 RootIndMask = (1 << RootIndBits)-1;
        static constexpr Uint32 ResTypeMask = (1 << ResTypeBits)-1;

        static constexpr Uint16 InvalidRootIndex = RootIndMask;
        static constexpr Uint16 MaxRootIndex = RootIndMask-1;

        static constexpr Uint32 InvalidSamplerId = 0xFFFF;
        static constexpr Uint32 MaxSamplerId = InvalidSamplerId-1;
        static constexpr Uint32 InvalidOffset = static_cast<Uint32>(-1);

        static_assert( static_cast<int>(CachedResourceType::NumTypes) <= ResTypeMask, "3 bits is not enough to store CachedResourceType");
        
        const Uint32 OffsetFromTableStart;

        // Special copy constructor. Note that sampler ID refers to the ID of the sampler
        // within THIS layout, and may not be the same as in original layout
        SRV_CBV_UAV(ShaderResourceLayoutD3D12&  ParentLayout, 
                    const SRV_CBV_UAV&          rhs, 
                    Uint32                      SamId)noexcept :
            ShaderVariableD3DBase<ShaderResourceLayoutD3D12>(ParentLayout, rhs.Attribs),
            ResType_RootIndex   (rhs.ResType_RootIndex),
            SamplerId           (static_cast<Uint16>(SamId)),
            OffsetFromTableStart(rhs.OffsetFromTableStart)
        {
            VERIFY(SamId == InvalidSamplerId || SamId <= MaxSamplerId, "Sampler id exceeds max allowed value (", MaxSamplerId, ")" );
            VERIFY(rhs.m_ParentResLayout.m_pResources == m_ParentResLayout.m_pResources, "Incosistent resource references");
            VERIFY(IsValidOffset(), "Offset must be valid" );
            VERIFY(IsValidRootIndex(), "Root index must be valid" );
        }

        SRV_CBV_UAV(ShaderResourceLayoutD3D12&      ParentLayout, 
                    const D3DShaderResourceAttribs& _Attribs, 
                    CachedResourceType              ResType, 
                    Uint32                          RootIndex, 
                    Uint32                          _OffsetFromTableStart, 
                    Uint32                          _SamplerId)noexcept :
            ShaderVariableD3DBase<ShaderResourceLayoutD3D12>(ParentLayout, _Attribs),
            ResType_RootIndex   ( (static_cast<Uint16>(ResType) << RootIndBits) | (RootIndex & RootIndMask)),
            SamplerId           ( static_cast<Uint16>(_SamplerId) ),
            OffsetFromTableStart( _OffsetFromTableStart )
        {
            VERIFY(RootIndex == InvalidRootIndex || RootIndex <= MaxRootIndex, "Root index exceeds max allowed value (", MaxRootIndex, ")" );
            VERIFY(IsValidOffset(), "Offset must be valid" );
            VERIFY(SamplerId == InvalidSamplerId || SamplerId <= MaxSamplerId, "Sampler id exceeds max allowed value (", MaxSamplerId, ")" );
        }

        bool IsBound(Uint32 ArrayIndex)const;

        // Non-virtual function
        void BindResource(IDeviceObject *pObject, Uint32 ArrayIndex, const ShaderResourceLayoutD3D12 *dbgResLayout);
        virtual void Set(IDeviceObject *pObject)override final{ BindResource(pObject, 0, nullptr); }

        virtual void SetArray(IDeviceObject* const* ppObjects, Uint32 FirstElement, Uint32 NumElements)override final
        {
            for(Uint32 Elem = 0; Elem < NumElements; ++Elem)
                BindResource(ppObjects[Elem], FirstElement+Elem, nullptr);
        }

        bool IsValidSampler()  const { return GetSamplerId()       != InvalidSamplerId; }
        bool IsValidRootIndex()const { return GetRootIndex()       != InvalidRootIndex; }
        bool IsValidOffset()   const { return OffsetFromTableStart != InvalidOffset;    }

        CachedResourceType GetResType()const
        {
            return static_cast<CachedResourceType>( (ResType_RootIndex >> RootIndBits) & ResTypeMask );
        }
        Uint32 GetRootIndex()const
        {
            return ResType_RootIndex & RootIndMask;
        }

        Uint32 GetSamplerId()const
        {
            return SamplerId;
        }

    private:
        const Uint16 ResType_RootIndex; //  bit
                                        // | 0 1 ....  12 | 13  14  15  |
                                        // |              |             |
                                        // |  Root index  |   ResType   |
        const Uint16 SamplerId;

        void CacheCB(IDeviceObject*                      pBuffer, 
                     ShaderResourceCacheD3D12::Resource& DstRes, 
                     Uint32                              ArrayInd, 
                     D3D12_CPU_DESCRIPTOR_HANDLE         ShdrVisibleHeapCPUDescriptorHandle);

        template<typename TResourceViewType, 
                 typename TViewTypeEnum,
                 typename TBindSamplerProcType>
        void CacheResourceView(IDeviceObject*                      pView, 
                               ShaderResourceCacheD3D12::Resource& DstRes, 
                               Uint32                              ArrayIndex,
                               D3D12_CPU_DESCRIPTOR_HANDLE         ShdrVisibleHeapCPUDescriptorHandle, 
                               TViewTypeEnum                       dbgExpectedViewType, 
                               TBindSamplerProcType                BindSamplerProc);
    };

    // sizeof(Sampler) == 24 (x64)
    struct Sampler
    {
        Sampler             (const Sampler&) = delete;
        Sampler             (Sampler&&)      = delete;
        Sampler& operator = (const Sampler&) = delete;
        Sampler& operator = (Sampler&&)      = delete;

        const D3DShaderResourceAttribs& Attribs;
        ShaderResourceLayoutD3D12&      m_ParentResLayout;

        static constexpr Uint32 InvalidRootIndex = static_cast<Uint32>(-1);
        static constexpr Uint32 InvalidOffset    = static_cast<Uint32>(-1);

        const Uint32 RootIndex;
        const Uint32 OffsetFromTableStart;

        Sampler(ShaderResourceLayoutD3D12 &ParentLayout, const Sampler& Sam)noexcept :
            Attribs             (Sam.Attribs),
            m_ParentResLayout   (ParentLayout),
            RootIndex           (Sam.RootIndex),
            OffsetFromTableStart(Sam.OffsetFromTableStart)
        {
            VERIFY(Sam.m_ParentResLayout.m_pResources == m_ParentResLayout.m_pResources, "Incosistent resource references");
            VERIFY(IsValidRootIndex(), "Root index must be valid" );
            VERIFY(IsValidOffset(), "Offset must be valid" );
        }

        Sampler(ShaderResourceLayoutD3D12&       ParentResLayout, 
                const D3DShaderResourceAttribs&  _Attribs, 
                Uint32                           _RootIndex,
                Uint32                           _OffsetFromTableStart)noexcept :
            RootIndex           (_RootIndex),
            OffsetFromTableStart(_OffsetFromTableStart),
            Attribs             (_Attribs),
            m_ParentResLayout   (ParentResLayout)
        {
            VERIFY(IsValidRootIndex(), "Root index must be valid" );
            VERIFY(IsValidOffset(), "Offset must be valid" );
        }

        bool IsValidRootIndex()const { return RootIndex            != InvalidRootIndex; }
        bool IsValidOffset()   const { return OffsetFromTableStart != InvalidOffset;    }

        void CacheSampler(class ITextureViewD3D12 *pTexViewD3D12, Uint32 ArrayIndex, D3D12_CPU_DESCRIPTOR_HANDLE ShdrVisibleHeapCPUDescriptorHandle);
    };


    void CopyStaticResourceDesriptorHandles(const ShaderResourceLayoutD3D12 &SrcLayout);

    // dbgResourceCache is only used for sanity check and as a remainder that the resource cache must be alive
    // while Layout is alive
    void BindResources( IResourceMapping* pResourceMapping, Uint32 Flags, const ShaderResourceCacheD3D12 *dbgResourceCache );

    IShaderVariable* GetShaderVariable( const Char* Name );

#ifdef VERIFY_SHADER_BINDINGS
    void dbgVerifyBindings()const;
#endif

    IObject& GetOwner(){return m_Owner;}

private:
    void InitVariablesHashMap();

    Sampler &GetAssignedSampler(const SRV_CBV_UAV &TexSrv);

    const Char* GetShaderName()const;

    // There is no need to use shared ptr as referenced resource cache is either part of the
    // parent ShaderD3D12Impl object or ShaderResourceBindingD3D12Impl object
    ShaderResourceCacheD3D12* m_pResourceCache;

    std::unique_ptr<void, STDDeleterRawMem<void> > m_ResourceBuffer;
    Sampler* m_Samplers = nullptr;
    std::array<Uint16, SHADER_VARIABLE_TYPE_NUM_TYPES + 1> m_CbvSrvUavOffsets = {};
    std::array<Uint16, SHADER_VARIABLE_TYPE_NUM_TYPES + 1> m_SamplersOffsets  = {};
    
    Uint32 GetCbvSrvUavCount(SHADER_VARIABLE_TYPE VarType)const
    {
        return m_CbvSrvUavOffsets[VarType + 1] - m_CbvSrvUavOffsets[VarType];
    }
    Uint32 GetSamplerCount(SHADER_VARIABLE_TYPE VarType)const
    {
        return m_SamplersOffsets[VarType + 1] - m_SamplersOffsets[VarType];
    }
    Uint32 GetTotalSrvCbvUavCount()const
    {
        return m_CbvSrvUavOffsets[SHADER_VARIABLE_TYPE_NUM_TYPES];
    }
    Uint32 GetTotalSamplerCount()const
    {
        return m_SamplersOffsets[SHADER_VARIABLE_TYPE_NUM_TYPES];
    }

    Uint32 GetSrvCbvUavOffset(SHADER_VARIABLE_TYPE VarType, Uint32 r)const
    {
        Uint32 Offset = m_CbvSrvUavOffsets[VarType] + r;
        VERIFY_EXPR( Offset < m_CbvSrvUavOffsets[VarType+1] );
        return Offset;
    }
    SRV_CBV_UAV& GetSrvCbvUav(SHADER_VARIABLE_TYPE VarType, Uint32 r)
    {
        VERIFY_EXPR( r < GetCbvSrvUavCount(VarType) );
        auto* CbvSrvUav = reinterpret_cast<SRV_CBV_UAV*>(m_ResourceBuffer.get());
        return CbvSrvUav[ GetSrvCbvUavOffset(VarType,r) ];
    }
    const SRV_CBV_UAV& GetSrvCbvUav(SHADER_VARIABLE_TYPE VarType, Uint32 r)const
    {
        VERIFY_EXPR( r < GetCbvSrvUavCount(VarType) );
        auto* CbvSrvUav = reinterpret_cast<SRV_CBV_UAV*>(m_ResourceBuffer.get());
        return CbvSrvUav[GetSrvCbvUavOffset(VarType,r)];
    }
    SRV_CBV_UAV& GetSrvCbvUav(Uint32 r)
    {
        VERIFY_EXPR( r < GetTotalSrvCbvUavCount() );
        auto* CbvSrvUav = reinterpret_cast<SRV_CBV_UAV*>(m_ResourceBuffer.get());
        return CbvSrvUav[r];
    }

    Uint32 GetSamplerOffset(SHADER_VARIABLE_TYPE VarType, Uint32 s)const
    {
        auto Offset = m_SamplersOffsets[VarType] + s;
        VERIFY_EXPR( Offset < m_SamplersOffsets[VarType+1] );
        return Offset;
    }
    Sampler& GetSampler(SHADER_VARIABLE_TYPE VarType, Uint32 s)
    {
        VERIFY_EXPR( s < GetSamplerCount(VarType) );
        return m_Samplers[GetSamplerOffset(VarType,s)];
    }
    const Sampler& GetSampler(SHADER_VARIABLE_TYPE VarType, Uint32 s)const
    {
        VERIFY_EXPR( s < GetSamplerCount(VarType) );
        return m_Samplers[GetSamplerOffset(VarType,s)];
    }

    void AllocateMemory(IMemoryAllocator&                                         Allocator,
                        const std::array<Uint32, SHADER_VARIABLE_TYPE_NUM_TYPES>& CbvSrvUavCount,
                        const std::array<Uint32, SHADER_VARIABLE_TYPE_NUM_TYPES>& SamplerCount);

#if USE_VARIABLE_HASH_MAP
    // Hash map to look up shader variables by name.
    // Note that sizeof(m_VariableHash)==128 (release mode, MS compiler, x64).
    typedef std::pair<HashMapStringKey, IShaderVariable*> VariableHashElemType;
    std::unordered_map<HashMapStringKey, IShaderVariable*, std::hash<HashMapStringKey>, std::equal_to<HashMapStringKey>, STDAllocatorRawMem<VariableHashElemType> > m_VariableHash;
#endif

    CComPtr<ID3D12Device> m_pd3d12Device;
    IObject &m_Owner;
    // We must use shared_ptr to reference ShaderResources instance, because
    // there may be multiple objects referencing the same set of resources
    std::shared_ptr<const ShaderResourcesD3D12> m_pResources;
};

}
