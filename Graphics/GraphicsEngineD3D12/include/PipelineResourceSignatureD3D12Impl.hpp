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
/// Declaration of Diligent::PipelineResourceSignatureD3D12Impl class

#include <array>

#include "PipelineResourceSignatureBase.hpp"
#include "SRBMemoryAllocator.hpp"

namespace Diligent
{

class CommandContext;
class RenderDeviceD3D12Impl;
class DeviceContextD3D12Impl;
class ShaderResourceCacheD3D12;
class ShaderVariableManagerD3D12;

/// Implementation of the Diligent::PipelineResourceSignatureD3D12Impl class
class PipelineResourceSignatureD3D12Impl final : public PipelineResourceSignatureBase<IPipelineResourceSignature, RenderDeviceD3D12Impl>
{
    friend class RootSignatureD3D12;

public:
    using TPipelineResourceSignatureBase = PipelineResourceSignatureBase<IPipelineResourceSignature, RenderDeviceD3D12Impl>;

    PipelineResourceSignatureD3D12Impl(IReferenceCounters*                  pRefCounters,
                                       RenderDeviceD3D12Impl*               pDevice,
                                       const PipelineResourceSignatureDesc& Desc,
                                       bool                                 bIsDeviceInternal = false);
    ~PipelineResourceSignatureD3D12Impl();

    enum class CacheContentType
    {
        Signature = 0, // only static resources
        SRB       = 1  // in SRB
    };

    // sizeof(ResourceAttribs) == 16, x64
    struct ResourceAttribs
    {
    private:
        static constexpr Uint32 _BindPointBits       = 16;
        static constexpr Uint32 _SpaceBits           = 8;
        static constexpr Uint32 _SRBRootIndexBits    = 16;
        static constexpr Uint32 _SigRootIndexBits    = 3;
        static constexpr Uint32 _SamplerIndBits      = 16;
        static constexpr Uint32 _SamplerAssignedBits = 1;
        static constexpr Uint32 _SigOffsetBits       = 16;
        static constexpr Uint32 _RootViewBits        = 1;

        static_assert((1u << _BindPointBits) >= MAX_RESOURCES_IN_SIGNATURE, "Not enough bits to store bind point");
        static_assert((1u << _SamplerIndBits) >= MAX_RESOURCES_IN_SIGNATURE, "Not enough bits to store sampler resource index");

    public:
        static constexpr Uint32 InvalidSamplerInd   = (1u << _SamplerIndBits) - 1;
        static constexpr Uint32 InvalidSRBRootIndex = (1u << _SRBRootIndexBits) - 1;
        static constexpr Uint32 InvalidSigRootIndex = (1u << _SigRootIndexBits) - 1;
        static constexpr Uint32 InvalidBindPoint    = (1u << _BindPointBits) - 1;
        static constexpr Uint32 InvalidOffset       = ~0u;

        // clang-format off
        const Uint32  BindPoint               : _BindPointBits;       // shader register
        const Uint32  SRBRootIndex            : _SRBRootIndexBits;    // Root view/table index for SRB
        const Uint32  SamplerInd              : _SamplerIndBits;      // Index in m_Desc.Resources and m_pResourceAttribs
        const Uint32  SigRootIndex            : _SigRootIndexBits;    // Root table index for signature (static only)
        const Uint32  Space                   : _SpaceBits;           // shader register space
        const Uint32  ImtblSamplerAssigned    : _SamplerAssignedBits; // Immutable sampler flag
        const Uint32  RootView                : _RootViewBits;        // Is root view (for debugging)
        const Uint32  SigOffsetFromTableStart;                        // Offset in the root table for signature (static only)
        const Uint32  SRBOffsetFromTableStart;                        // Offset in the root table for SRB
        // clang-format on

        ResourceAttribs(Uint32 _BindPoint,
                        Uint32 _Space,
                        Uint32 _SamplerInd,
                        Uint32 _SRBRootIndex,
                        Uint32 _SRBOffsetFromTableStart,
                        Uint32 _SigRootIndex,
                        Uint32 _SigOffsetFromTableStart,
                        bool   _ImtblSamplerAssigned,
                        bool   _IsRootView) noexcept :
            // clang-format off
            BindPoint              {_BindPoint                     },
            SRBRootIndex           {_SRBRootIndex                  },
            SamplerInd             {_SamplerInd                    },
            SigRootIndex           {_SigRootIndex                  },
            Space                  {_Space                         },
            ImtblSamplerAssigned   {_ImtblSamplerAssigned ? 1u : 0u},
            RootView               {_IsRootView ? 1u : 0u          },
            SigOffsetFromTableStart{_SigOffsetFromTableStart       },
            SRBOffsetFromTableStart{_SRBOffsetFromTableStart       }
        // clang-format on
        {
            VERIFY(BindPoint == _BindPoint, "Bind point (", _BindPoint, ") exceeds maximum representable value");
            VERIFY(SRBRootIndex == _SRBRootIndex, "SRB Root index (", _SRBRootIndex, ") exceeds maximum representable value");
            VERIFY(SigRootIndex == _SigRootIndex, "Signature Root index (", SigRootIndex, ") exceeds maximum representable value");
            VERIFY(SamplerInd == _SamplerInd, "Sampler index (", _SamplerInd, ") exceeds maximum representable value");
            VERIFY(Space == _Space, "Space (", Space, ") exceeds maximum representable value");
        }

        bool IsImmutableSamplerAssigned() const { return ImtblSamplerAssigned != 0; }
        bool IsCombinedWithSampler() const { return SamplerInd != InvalidSamplerInd; }
        bool IsRootView() const { return RootView != 0; }

        Uint32 RootIndex(CacheContentType Type) const { return Type == CacheContentType::SRB ? SRBRootIndex : SigRootIndex; }
        Uint32 OffsetFromTableStart(CacheContentType Type) const { return Type == CacheContentType::SRB ? SRBOffsetFromTableStart : SigOffsetFromTableStart; }
    };

    const ResourceAttribs& GetResourceAttribs(Uint32 ResIndex) const
    {
        VERIFY_EXPR(ResIndex < m_Desc.NumResources);
        return m_pResourceAttribs[ResIndex];
    }

    const PipelineResourceDesc& GetResourceDesc(Uint32 ResIndex) const
    {
        VERIFY_EXPR(ResIndex < m_Desc.NumResources);
        return m_Desc.Resources[ResIndex];
    }

    struct ImmutableSamplerAttribs
    {
    private:
        static constexpr Uint32 _ShaderRegisterBits    = 16;
        static constexpr Uint32 _RegisterSpaceBits     = 16;
        static constexpr Uint32 _InvalidShaderRegister = (1u << _ShaderRegisterBits) - 1;
        static constexpr Uint32 _InvalidRegisterSpace  = (1u << _RegisterSpaceBits) - 1;

    public:
        Uint32 ArraySize = 1;
        Uint32 ShaderRegister : _ShaderRegisterBits;
        Uint32 RegisterSpace : _RegisterSpaceBits;

        ImmutableSamplerAttribs() :
            ShaderRegister{_InvalidShaderRegister},
            RegisterSpace{_InvalidRegisterSpace}
        {}

        ImmutableSamplerAttribs(Uint32 _ArraySize,
                                Uint32 _ShaderRegister,
                                Uint32 _RegisterSpace) noexcept :
            // clang-format off
            ArraySize     {_ArraySize     },
            ShaderRegister{_ShaderRegister},
            RegisterSpace {_RegisterSpace }
        // clang-format on
        {
            VERIFY(ShaderRegister == _ShaderRegister, "Shader register (", _ShaderRegister, ") exceeds maximum representable value");
            VERIFY(RegisterSpace == _RegisterSpace, "Shader register space (", _RegisterSpace, ") exceeds maximum representable value");
        }

        bool IsAssigned() const { return ShaderRegister != _InvalidShaderRegister; }
    };

    const ImmutableSamplerAttribs& GetImmutableSamplerAttribs(Uint32 SampIndex) const
    {
        VERIFY_EXPR(SampIndex < m_Desc.NumImmutableSamplers);
        return m_ImmutableSamplers[SampIndex];
    }

    const ImmutableSamplerDesc& GetImmutableSamplerDesc(Uint32 SampIndex) const
    {
        VERIFY_EXPR(SampIndex < m_Desc.NumImmutableSamplers);
        return m_Desc.ImmutableSamplers[SampIndex];
    }

    Uint32 GetTotalRootCount() const
    {
        return m_RootParams.GetNumRootTables() + m_RootParams.GetNumRootViews();
    }

    Uint32 GetBaseRegisterSpace() const
    {
        return m_Desc.BindingIndex * MAX_SPACES_PER_SIGNATURE;
    }

    virtual void DILIGENT_CALL_TYPE CreateShaderResourceBinding(IShaderResourceBinding** ppShaderResourceBinding,
                                                                bool                     InitStaticResources) override final;

    virtual IShaderResourceVariable* DILIGENT_CALL_TYPE GetStaticVariableByName(SHADER_TYPE ShaderType, const Char* Name) override final;

    virtual IShaderResourceVariable* DILIGENT_CALL_TYPE GetStaticVariableByIndex(SHADER_TYPE ShaderType, Uint32 Index) override final;

    virtual Uint32 DILIGENT_CALL_TYPE GetStaticVariableCount(SHADER_TYPE ShaderType) const override final;

    virtual void DILIGENT_CALL_TYPE BindStaticResources(Uint32            ShaderFlags,
                                                        IResourceMapping* pResourceMapping,
                                                        Uint32            Flags) override final;

    virtual bool DILIGENT_CALL_TYPE IsCompatibleWith(const IPipelineResourceSignature* pPRS) const override final
    {
        VERIFY_EXPR(pPRS != nullptr);
        return IsCompatibleWith(*ValidatedCast<const PipelineResourceSignatureD3D12Impl>(pPRS));
    }

    bool IsCompatibleWith(const PipelineResourceSignatureD3D12Impl& Other) const;

    bool IsIncompatibleWith(const PipelineResourceSignatureD3D12Impl& Other) const
    {
        return GetHash() != Other.GetHash();
    }

    SRBMemoryAllocator& GetSRBMemoryAllocator()
    {
        return m_SRBMemAllocator;
    }

    void InitSRBResourceCache(ShaderResourceCacheD3D12& ResourceCache,
                              IMemoryAllocator&         CacheMemAllocator,
                              const char*               DbgPipelineName) const;

    void InitializeStaticSRBResources(ShaderResourceCacheD3D12& ResourceCache) const;

    // Binds object pObj to resource with index ResIndex in m_Desc.Resources and
    // array index ArrayIndex.
    void BindResource(IDeviceObject*            pObj,
                      Uint32                    ArrayIndex,
                      Uint32                    ResIndex,
                      ShaderResourceCacheD3D12& ResourceCache) const;

    bool IsBound(Uint32                    ArrayIndex,
                 Uint32                    ResIndex,
                 ShaderResourceCacheD3D12& ResourceCache) const;

    void TransitionResources(ShaderResourceCacheD3D12& ResourceCache, CommandContext& Ctx, bool PerformResourceTransitions, bool ValidateStates) const;

    void CommitRootTables(ShaderResourceCacheD3D12& ResourceCache,
                          CommandContext&           Ctx,
                          DeviceContextD3D12Impl*   pDeviceCtx,
                          Uint32                    DeviceCtxId,
                          bool                      IsCompute,
                          Uint32                    FirstRootIndex);

    void CommitRootViews(ShaderResourceCacheD3D12& ResourceCache,
                         CommandContext&           Ctx,
                         DeviceContextD3D12Impl*   pDeviceCtx,
                         Uint32                    DeviceCtxId,
                         bool                      IsCompute,
                         Uint32                    FirstRootIndex);

private:
    enum ROOT_TYPE : Uint8
    {
        ROOT_TYPE_STATIC  = 0,
        ROOT_TYPE_DYNAMIC = 1,
        ROOT_TYPE_COUNT
    };
    static ROOT_TYPE GetRootType(SHADER_RESOURCE_VARIABLE_TYPE VarType);


    class RootParameter
    {
    public:
        RootParameter(D3D12_ROOT_PARAMETER_TYPE ParameterType,
                      Uint32                    RootIndex,
                      UINT                      Register,
                      UINT                      RegisterSpace,
                      D3D12_SHADER_VISIBILITY   Visibility,
                      ROOT_TYPE                 RootType) noexcept;

        RootParameter(D3D12_ROOT_PARAMETER_TYPE ParameterType,
                      Uint32                    RootIndex,
                      UINT                      Register,
                      UINT                      RegisterSpace,
                      UINT                      NumDwords,
                      D3D12_SHADER_VISIBILITY   Visibility,
                      ROOT_TYPE                 RootType) noexcept;

        RootParameter(D3D12_ROOT_PARAMETER_TYPE ParameterType,
                      Uint32                    RootIndex,
                      UINT                      NumRanges,
                      D3D12_DESCRIPTOR_RANGE*   pRanges,
                      D3D12_SHADER_VISIBILITY   Visibility,
                      ROOT_TYPE                 RootType) noexcept;

        RootParameter(const RootParameter& RP) noexcept;

        RootParameter(const RootParameter&    RP,
                      UINT                    NumRanges,
                      D3D12_DESCRIPTOR_RANGE* pRanges) noexcept;

        RootParameter& operator=(const RootParameter&) = delete;
        RootParameter& operator=(RootParameter&&) = delete;

        void SetDescriptorRange(UINT                        RangeIndex,
                                D3D12_DESCRIPTOR_RANGE_TYPE Type,
                                UINT                        Register,
                                UINT                        RegisterSpace,
                                UINT                        Count,
                                UINT                        OffsetFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND);

        ROOT_TYPE GetRootType() const { return m_RootType; }

        Uint32 GetDescriptorTableSize() const;

        D3D12_SHADER_VISIBILITY   GetShaderVisibility() const { return m_RootParam.ShaderVisibility; }
        D3D12_ROOT_PARAMETER_TYPE GetParameterType() const { return m_RootParam.ParameterType; }

        Uint32 GetLocalRootIndex() const { return m_RootIndex; }

        operator const D3D12_ROOT_PARAMETER&() const { return m_RootParam; }

        bool operator==(const RootParameter& rhs) const;
        bool operator!=(const RootParameter& rhs) const { return !(*this == rhs); }

        size_t GetHash() const;

    private:
        ROOT_TYPE            m_RootType            = static_cast<ROOT_TYPE>(-1);
        D3D12_ROOT_PARAMETER m_RootParam           = {};
        Uint32               m_DescriptorTableSize = 0;
        Uint32               m_RootIndex           = static_cast<Uint32>(-1);
    };


    class RootParamsManager
    {
    public:
        RootParamsManager(IMemoryAllocator& MemAllocator);

        // clang-format off
        RootParamsManager           (const RootParamsManager&) = delete;
        RootParamsManager& operator=(const RootParamsManager&) = delete;
        RootParamsManager           (RootParamsManager&&)      = delete;
        RootParamsManager& operator=(RootParamsManager&&)      = delete;
        // clang-format on

        Uint32 GetNumRootTables() const { return m_NumRootTables; }
        Uint32 GetNumRootViews() const { return m_NumRootViews; }

        const RootParameter& GetRootTable(Uint32 TableInd) const
        {
            VERIFY_EXPR(TableInd < m_NumRootTables);
            return m_pRootTables[TableInd];
        }

        RootParameter& GetRootTable(Uint32 TableInd)
        {
            VERIFY_EXPR(TableInd < m_NumRootTables);
            return m_pRootTables[TableInd];
        }

        const RootParameter& GetRootView(Uint32 ViewInd) const
        {
            VERIFY_EXPR(ViewInd < m_NumRootViews);
            return m_pRootViews[ViewInd];
        }

        RootParameter& GetRootView(Uint32 ViewInd)
        {
            VERIFY_EXPR(ViewInd < m_NumRootViews);
            return m_pRootViews[ViewInd];
        }

        void AddRootView(D3D12_ROOT_PARAMETER_TYPE ParameterType,
                         Uint32                    RootIndex,
                         UINT                      Register,
                         UINT                      RegisterSpace,
                         D3D12_SHADER_VISIBILITY   Visibility,
                         ROOT_TYPE                 RootType);

        void AddRootTable(Uint32                  RootIndex,
                          D3D12_SHADER_VISIBILITY Visibility,
                          ROOT_TYPE               RootType,
                          Uint32                  NumRangesInNewTable = 1);

        void AddDescriptorRanges(Uint32 RootTableInd, Uint32 NumExtraRanges = 1);

        template <class TOperation>
        void ProcessRootTables(TOperation) const;

        bool operator==(const RootParamsManager& RootParams) const;

    private:
        size_t GetRequiredMemorySize(Uint32 NumExtraRootTables,
                                     Uint32 NumExtraRootViews,
                                     Uint32 NumExtraDescriptorRanges) const;

        D3D12_DESCRIPTOR_RANGE* Extend(Uint32 NumExtraRootTables,
                                       Uint32 NumExtraRootViews,
                                       Uint32 NumExtraDescriptorRanges,
                                       Uint32 RootTableToAddRanges = static_cast<Uint32>(-1));

        IMemoryAllocator&                                         m_MemAllocator;
        std::unique_ptr<void, STDDeleter<void, IMemoryAllocator>> m_pMemory;
        Uint32                                                    m_NumRootTables         = 0;
        Uint32                                                    m_NumRootViews          = 0;
        Uint32                                                    m_TotalDescriptorRanges = 0;
        RootParameter*                                            m_pRootTables           = nullptr;
        RootParameter*                                            m_pRootViews            = nullptr;
    };

    using CacheOffsetsType = std::array<Uint32, 2>;

    void CreateLayout();

    // Allocates root signature slot for the given resource.
    // For graphics and compute pipelines, BindPoint is the same as the original bind point.
    // For ray-tracing pipeline, BindPoint will be overriden. Bind points are then
    // remapped by PSO constructor.
    void AllocateResourceSlot(SHADER_TYPE                   ShaderStages,
                              SHADER_RESOURCE_VARIABLE_TYPE VariableType,
                              D3D12_DESCRIPTOR_RANGE_TYPE   RangeType,
                              Uint32                        ArraySize,
                              bool                          IsRootView,
                              Uint32                        BindPoint,
                              Uint32                        Space,
                              Uint32&                       RootIndex,
                              Uint32&                       OffsetFromTableStart);

    size_t CalculateHash() const;

    void Destruct();

    std::vector<Uint32, STDAllocatorRawMem<Uint32>> GetCacheTableSizes() const;

    Uint32 FindAssignedSampler(const PipelineResourceDesc& SepImg) const;

private:
    static constexpr Uint8  InvalidRootTableIndex    = static_cast<Uint8>(-1);
    static constexpr Uint32 MAX_SPACES_PER_SIGNATURE = 128;

    ResourceAttribs* m_pResourceAttribs = nullptr; // [m_Desc.NumResources]

    std::array<Int8, MAX_SHADERS_IN_PIPELINE> m_StaticVarIndex = {-1, -1, -1, -1, -1, -1};
    static_assert(MAX_SHADERS_IN_PIPELINE == 6, "Please update the initializer list above");

    // The array below contains array index of a CBV/SRV/UAV root table
    // in m_RootParams (NOT the Root Index!), for every variable type
    // (static, mutable, dynamic) and every shader type,
    // or -1, if the table is not yet assigned to the combination
    std::array<Uint8, ROOT_TYPE_COUNT* MAX_SHADERS_IN_PIPELINE> m_SrvCbvUavRootTablesMap = {};
    // This array contains the same data for Sampler root table
    std::array<Uint8, ROOT_TYPE_COUNT* MAX_SHADERS_IN_PIPELINE> m_SamplerRootTablesMap = {};

    std::array<Uint32, ROOT_TYPE_COUNT> m_TotalSrvCbvUavSlots = {};
    std::array<Uint32, ROOT_TYPE_COUNT> m_TotalSamplerSlots   = {};
    std::array<Uint32, ROOT_TYPE_COUNT> m_TotalRootViews      = {};

    Uint32 m_NumSpaces = 0;

    ShaderResourceCacheD3D12*   m_pStaticResCache = nullptr;
    ShaderVariableManagerD3D12* m_StaticVarsMgrs  = nullptr; // [m_NumShaderStages]

    ImmutableSamplerAttribs* m_ImmutableSamplers = nullptr; // [m_Desc.NumImmutableSamplers]

    RootParamsManager m_RootParams;

    SRBMemoryAllocator m_SRBMemAllocator;
};


} // namespace Diligent
