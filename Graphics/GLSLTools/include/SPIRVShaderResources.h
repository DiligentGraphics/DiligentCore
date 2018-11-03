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
/// Declaration of Diligent::SPIRVShaderResources class

// SPIRVShaderResources class uses continuous chunk of memory to store all resources, as follows:
//
//   m_MemoryBuffer                                                                                                              m_TotalResources
//    |                                                                                                                             |
//    | Uniform Buffers | Storage Buffers | Storage Images | Sampled Images | Atomic Counters | Separate Samplers | Separate Images |  Immutable Samplers  |   Stage Inputs   |   Resource Names   |

#include <memory>
#include <vector>
#include <sstream>

#include "Shader.h"
#include "Sampler.h"
#include "RenderDevice.h"
#include "STDAllocator.h"
#include "HashUtils.h"
#include "RefCntAutoPtr.h"
#include "StringPool.h"

namespace spirv_cross
{
class Compiler;
struct Resource;
}

namespace Diligent
{

inline bool IsAllowedType(SHADER_VARIABLE_TYPE VarType, Uint32 AllowedTypeBits)noexcept
{
    return ((1 << VarType) & AllowedTypeBits) != 0;
}

inline Uint32 GetAllowedTypeBits(const SHADER_VARIABLE_TYPE* AllowedVarTypes, Uint32 NumAllowedTypes)noexcept
{
    if(AllowedVarTypes == nullptr)
        return 0xFFFFFFFF;

    Uint32 AllowedTypeBits = 0;
    for(Uint32 i=0; i < NumAllowedTypes; ++i)
        AllowedTypeBits |= 1 << AllowedVarTypes[i];
    return AllowedTypeBits;
}

// sizeof(SPIRVShaderResourceAttribs) == 24, msvc x64
struct SPIRVShaderResourceAttribs
{
    enum ResourceType : Uint8
    {
        UniformBuffer = 0,
        StorageBuffer,
        UniformTexelBuffer,
        StorageTexelBuffer,
        StorageImage,
        SampledImage,
        AtomicCounter,
        SeparateImage,
        SeparateSampler,
        NumResourceTypes
    };

    static constexpr const Uint32 ResourceTypeBits = 4;
    static constexpr const Uint32 VarTypeBits      = 4;
    static_assert(SHADER_VARIABLE_TYPE_NUM_TYPES < (1 << VarTypeBits),       "Not enough bits to represent SHADER_VARIABLE_TYPE");
    static_assert(ResourceType::NumResourceTypes < (1 << ResourceTypeBits),  "Not enough bits to represent ResourceType");

    static constexpr const Uint32 InvalidSepSmplrOrImgInd = static_cast<Uint32>(-1);

/* 0  */const char* const           Name;
/* 8  */const Uint16                ArraySize;
/*10.0*/const ResourceType          Type            : ResourceTypeBits;
/*10.4*/const SHADER_VARIABLE_TYPE  VarType         : VarTypeBits;
private:
      static constexpr const Uint8  InvalidImmutableSamplerInd = static_cast<Uint8>(-1);
/*11*/const Uint8                   ImmutableSamplerInd;

      // Defines mapping between separate samplers and seperate images when HLSL-style
      // combined texture samplers are in use (i.e. texture2D g_Tex + sampler g_Tex_sampler).
/*12*/      Uint32                  SepSmplrOrImgInd        = InvalidSepSmplrOrImgInd;
public:
      // Offset in SPIRV words (uint32_t) of binding & descriptor set decorations in SPIRV binary
/*16*/const uint32_t BindingDecorationOffset;
/*20*/const uint32_t DescriptorSetDecorationOffset;


    SPIRVShaderResourceAttribs(const spirv_cross::Compiler& Compiler, 
                               const spirv_cross::Resource& Res, 
                               const char*                  _Name,
                               ResourceType                 _Type, 
                               SHADER_VARIABLE_TYPE         _VarType,
                               Int32                        _ImmutableSamplerInd = -1,
                               Uint32                       _SamplerOrSepImgInd = InvalidSepSmplrOrImgInd)noexcept;

    bool IsValidSepSamplerAssigned() const
    {
        VERIFY_EXPR(Type == SeparateImage);
        return SepSmplrOrImgInd  != InvalidSepSmplrOrImgInd;
    }

    bool IsValidSepImageAssigned() const
    {
        VERIFY_EXPR(Type == SeparateSampler);
        return SepSmplrOrImgInd != InvalidSepSmplrOrImgInd;
    }

    Uint32 GetAssignedSepSamplerInd() const
    {
        VERIFY_EXPR(Type == SeparateImage);
        return SepSmplrOrImgInd;
    }

    Uint32 GetAssignedSepImageInd() const
    {
        VERIFY_EXPR(Type == SeparateSampler);
        return SepSmplrOrImgInd;
    }

    void AssignSeparateSampler(Uint32 SemSamplerInd)
    {
        VERIFY_EXPR(Type == SeparateImage);
        SepSmplrOrImgInd = SemSamplerInd;
    }

    void AssignSeparateImage(Uint32 SepImageInd)
    {
        VERIFY_EXPR(Type == SeparateSampler);
        SepSmplrOrImgInd = SepImageInd;
    }

    bool IsImmutableSamplerAssigned() const
    {
        return ImmutableSamplerInd != InvalidImmutableSamplerInd;
    }

    Uint32 GetImmutableSamplerInd()const
    {
        VERIFY(Type == ResourceType::SampledImage || Type == ResourceType::SeparateSampler, "Only sampled images and separate samplers can be assigned immutable samplers");
        return ImmutableSamplerInd;
    }

    String GetPrintName(Uint32 ArrayInd)const
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

    bool IsCompatibleWith(const SPIRVShaderResourceAttribs& Attribs)const
    {
        return ArraySize        == Attribs.ArraySize        && 
               Type             == Attribs.Type             &&
               VarType          == Attribs.VarType          && 
               SepSmplrOrImgInd == Attribs.SepSmplrOrImgInd &&
               ( IsImmutableSamplerAssigned() &&  Attribs.IsImmutableSamplerAssigned() ||
                !IsImmutableSamplerAssigned() && !Attribs.IsImmutableSamplerAssigned());
    }
};
static_assert(sizeof(SPIRVShaderResourceAttribs) % sizeof(void*) == 0, "Size of SPIRVShaderResourceAttribs struct must be multiple of sizeof(void*)" );

struct SPIRVShaderStageInputAttribs
{
    SPIRVShaderStageInputAttribs(const char* _Semantic, uint32_t _LocationDecorationOffset) :
        Semantic                (_Semantic),
        LocationDecorationOffset(_LocationDecorationOffset)
    {}

    const char* const Semantic;
    const uint32_t LocationDecorationOffset;
};
static_assert(sizeof(SPIRVShaderStageInputAttribs) % sizeof(void*) == 0, "Size of SPIRVShaderStageInputAttribs struct must be multiple of sizeof(void*)" );

/// Diligent::SPIRVShaderResources class
class SPIRVShaderResources
{
public:
    SPIRVShaderResources(IMemoryAllocator&      Allocator,
                         IRenderDevice*         pRenderDevice,
                         std::vector<uint32_t>  spirv_binary,
                         const ShaderDesc&      shaderDesc,
                         const char*            CombinedSamplerSuffix,
                         bool                   LoadShaderStageInputs);

    SPIRVShaderResources             (const SPIRVShaderResources&)  = delete;
    SPIRVShaderResources             (      SPIRVShaderResources&&) = delete;
    SPIRVShaderResources& operator = (const SPIRVShaderResources&)  = delete;
    SPIRVShaderResources& operator = (      SPIRVShaderResources&&) = delete;
    
    ~SPIRVShaderResources();
    
    using SamplerPtrType = RefCntAutoPtr<ISampler>;

    Uint32 GetNumUBs      ()const noexcept{ return (m_StorageBufferOffset   - 0);                      }
    Uint32 GetNumSBs      ()const noexcept{ return (m_StorageImageOffset    - m_StorageBufferOffset);  }
    Uint32 GetNumImgs     ()const noexcept{ return (m_SampledImageOffset    - m_StorageImageOffset);   }
    Uint32 GetNumSmpldImgs()const noexcept{ return (m_AtomicCounterOffset   - m_SampledImageOffset);   }
    Uint32 GetNumACs      ()const noexcept{ return (m_SeparateSamplerOffset - m_AtomicCounterOffset);  }
    Uint32 GetNumSepSmplrs()const noexcept{ return (m_SeparateImageOffset   - m_SeparateSamplerOffset);}
    Uint32 GetNumSepImgs  ()const noexcept{ return (m_TotalResources        - m_SeparateImageOffset);  }
    Uint32 GetTotalResources()      const noexcept { return m_TotalResources; }
    Uint32 GetNumImmutableSamplers()const noexcept { return m_NumImmutableSamplers; }
    Uint32 GetNumShaderStageInputs()const noexcept { return m_NumShaderStageInputs; }

    const SPIRVShaderResourceAttribs& GetUB      (Uint32 n)const noexcept{ return GetResAttribs(n, GetNumUBs(),         0                      ); }
    const SPIRVShaderResourceAttribs& GetSB      (Uint32 n)const noexcept{ return GetResAttribs(n, GetNumSBs(),         m_StorageBufferOffset  ); }
    const SPIRVShaderResourceAttribs& GetImg     (Uint32 n)const noexcept{ return GetResAttribs(n, GetNumImgs(),        m_StorageImageOffset   ); }
    const SPIRVShaderResourceAttribs& GetSmpldImg(Uint32 n)const noexcept{ return GetResAttribs(n, GetNumSmpldImgs(),   m_SampledImageOffset   ); }
    const SPIRVShaderResourceAttribs& GetAC      (Uint32 n)const noexcept{ return GetResAttribs(n, GetNumACs(),         m_AtomicCounterOffset  ); }
    const SPIRVShaderResourceAttribs& GetSepSmplr(Uint32 n)const noexcept{ return GetResAttribs(n, GetNumSepSmplrs(),   m_SeparateSamplerOffset); }
    const SPIRVShaderResourceAttribs& GetSepImg  (Uint32 n)const noexcept{ return GetResAttribs(n, GetNumSepImgs(),     m_SeparateImageOffset  ); }
    const SPIRVShaderResourceAttribs& GetResource(Uint32 n)const noexcept{ return GetResAttribs(n, GetTotalResources(), 0                      ); }
    
    ISampler* GetImmutableSampler(const SPIRVShaderResourceAttribs& ResAttribs)const noexcept
    { 
        if (!ResAttribs.IsImmutableSamplerAssigned())
            return nullptr;

        auto ImmutableSamplerInd = ResAttribs.GetImmutableSamplerInd();
        VERIFY(ImmutableSamplerInd < m_NumImmutableSamplers, "Static sampler index (", ImmutableSamplerInd, ") is out of range. Array size: ", m_NumImmutableSamplers);
        auto* ResourceMemoryEnd = reinterpret_cast<SPIRVShaderResourceAttribs*>(m_MemoryBuffer.get()) + m_TotalResources;
        return reinterpret_cast<SamplerPtrType*>(ResourceMemoryEnd)[ImmutableSamplerInd];
    }

    const SPIRVShaderStageInputAttribs& GetShaderStageInputAttribs(Uint32 n)const noexcept
    {
        VERIFY(n < m_NumShaderStageInputs, "Shader stage input index (", n, ") is out of range. Total input count: ", m_NumShaderStageInputs);
        auto* ResourceMemoryEnd = reinterpret_cast<const SPIRVShaderResourceAttribs*>(m_MemoryBuffer.get()) + m_TotalResources;
        auto* ImmutableSamplerMemoryEnd = reinterpret_cast<const SamplerPtrType*>(ResourceMemoryEnd) + m_NumImmutableSamplers;
        return reinterpret_cast<const SPIRVShaderStageInputAttribs*>(ImmutableSamplerMemoryEnd)[n];
    }

    struct ResourceCounters
    {
        Uint32 NumUBs       = 0;
        Uint32 NumSBs       = 0;
        Uint32 NumImgs      = 0; 
        Uint32 NumSmpldImgs = 0; 
        Uint32 NumACs       = 0;
        Uint32 NumSepSmplrs = 0;
        Uint32 NumSepImgs   = 0;
    };
    ResourceCounters CountResources(const SHADER_VARIABLE_TYPE* AllowedVarTypes, 
                                    Uint32                      NumAllowedTypes)const noexcept;

    SHADER_TYPE GetShaderType()const noexcept{return m_ShaderType;}

    // Process only resources listed in AllowedVarTypes
    template<typename THandleUB,
             typename THandleSB,
             typename THandleImg,
             typename THandleSmplImg,
             typename THandleAC,
             typename THandleSepSmpl,
             typename THandleSepImg>
    void ProcessResources(const SHADER_VARIABLE_TYPE*   AllowedVarTypes, 
                          Uint32                        NumAllowedTypes,
                          THandleUB                     HandleUB,
                          THandleSB                     HandleSB,
                          THandleImg                    HandleImg,
                          THandleSmplImg                HandleSmplImg,
                          THandleAC                     HandleAC,
                          THandleSepSmpl                HandleSepSmpl,
                          THandleSepImg                 HandleSepImg)const
    {
        Uint32 AllowedTypeBits = GetAllowedTypeBits(AllowedVarTypes, NumAllowedTypes);

        for(Uint32 n=0; n < GetNumUBs(); ++n)
        {
            const auto& UB = GetUB(n);
            if( IsAllowedType(UB.VarType, AllowedTypeBits) )
                HandleUB(UB, n);
        }

        for (Uint32 n = 0; n < GetNumSBs(); ++n)
        {
            const auto& SB = GetSB(n);
            if (IsAllowedType(SB.VarType, AllowedTypeBits))
                HandleSB(SB, n);
        }

        for (Uint32 n = 0; n < GetNumImgs(); ++n)
        {
            const auto& Img = GetImg(n);
            if (IsAllowedType(Img.VarType, AllowedTypeBits))
                HandleImg(Img, n);
        }

        for (Uint32 n = 0; n < GetNumSmpldImgs(); ++n)
        {
            const auto& SmplImg = GetSmpldImg(n);
            if (IsAllowedType(SmplImg.VarType, AllowedTypeBits))
                HandleSmplImg(SmplImg, n);
        }

        for (Uint32 n = 0; n < GetNumACs(); ++n)
        {
            const auto& AC = GetAC(n);
            if (IsAllowedType(AC.VarType, AllowedTypeBits))
                HandleAC(AC, n);
        }

        for (Uint32 n = 0; n < GetNumSepSmplrs(); ++n)
        {
            const auto& SepSmpl = GetSepSmplr(n);
            if (IsAllowedType(SepSmpl.VarType, AllowedTypeBits))
                HandleSepSmpl(SepSmpl, n);
        }

        for (Uint32 n = 0; n < GetNumSepImgs(); ++n)
        {
            const auto& SepImg = GetSepImg(n);
            if (IsAllowedType(SepImg.VarType, AllowedTypeBits))
                HandleSepImg(SepImg, n);
        }
    }

    template<typename THandler>
    void ProcessResources(const SHADER_VARIABLE_TYPE* AllowedVarTypes, 
                          Uint32                      NumAllowedTypes,
                          THandler                    Handler)const
    {
        Uint32 AllowedTypeBits = GetAllowedTypeBits(AllowedVarTypes, NumAllowedTypes);

        for(Uint32 n=0; n < GetTotalResources(); ++n)
        {
            const auto& Res = GetResource(n);
            if( IsAllowedType(Res.VarType, AllowedTypeBits) )
                Handler(Res, n);
        }
    }

    std::string DumpResources();

    bool IsCompatibleWith(const SPIRVShaderResources& Resources)const;
    
    //size_t GetHash()const;

    const char* GetCombinedSamplerSuffix() const { return m_CombinedSamplerSuffix; } 
    bool        IsUsingCombinedSamplers()  const { return m_CombinedSamplerSuffix != nullptr; }


private:
    void Initialize(IMemoryAllocator&       Allocator, 
                    const ResourceCounters& Counters,
                    Uint32                  NumImmutableSamplers,
                    Uint32                  NumShaderStageInputs,
                    size_t                  ResourceNamesPoolSize);

    SPIRVShaderResourceAttribs& GetResAttribs(Uint32 n, Uint32 NumResources, Uint32 Offset)noexcept
    {
        VERIFY(n < NumResources, "Resource index (", n, ") is out of range. Total resource count: ", NumResources);
        VERIFY_EXPR(Offset + n < m_TotalResources);
        return reinterpret_cast<SPIRVShaderResourceAttribs*>(m_MemoryBuffer.get())[Offset + n];
    }

    const SPIRVShaderResourceAttribs& GetResAttribs(Uint32 n, Uint32 NumResources, Uint32 Offset)const noexcept
    {
        VERIFY(n < NumResources, "Resource index (", n, ") is out of range. Total resource count: ", NumResources);
        VERIFY_EXPR(Offset + n < m_TotalResources);
        return reinterpret_cast<SPIRVShaderResourceAttribs*>(m_MemoryBuffer.get())[Offset + n];
    }

    SPIRVShaderResourceAttribs& GetUB      (Uint32 n)noexcept{ return GetResAttribs(n, GetNumUBs(),         0                      ); }
    SPIRVShaderResourceAttribs& GetSB      (Uint32 n)noexcept{ return GetResAttribs(n, GetNumSBs(),         m_StorageBufferOffset  ); }
    SPIRVShaderResourceAttribs& GetImg     (Uint32 n)noexcept{ return GetResAttribs(n, GetNumImgs(),        m_StorageImageOffset   ); }
    SPIRVShaderResourceAttribs& GetSmpldImg(Uint32 n)noexcept{ return GetResAttribs(n, GetNumSmpldImgs(),   m_SampledImageOffset   ); }
    SPIRVShaderResourceAttribs& GetAC      (Uint32 n)noexcept{ return GetResAttribs(n, GetNumACs(),         m_AtomicCounterOffset  ); }
    SPIRVShaderResourceAttribs& GetSepSmplr(Uint32 n)noexcept{ return GetResAttribs(n, GetNumSepSmplrs(),   m_SeparateSamplerOffset); }
    SPIRVShaderResourceAttribs& GetSepImg  (Uint32 n)noexcept{ return GetResAttribs(n, GetNumSepImgs(),     m_SeparateImageOffset  ); }
    SPIRVShaderResourceAttribs& GetResource(Uint32 n)noexcept{ return GetResAttribs(n, GetTotalResources(), 0                      ); }

    SamplerPtrType& GetImmutableSampler(Uint32 n)noexcept
    {
        VERIFY(n < m_NumImmutableSamplers, "Immutable sampler index (", n, ") is out of range. Total immutable sampler count: ", m_NumImmutableSamplers);
        auto* ResourceMemoryEnd = reinterpret_cast<SPIRVShaderResourceAttribs*>(m_MemoryBuffer.get()) + m_TotalResources;
        return reinterpret_cast<SamplerPtrType*>(ResourceMemoryEnd)[n];
    }

    SPIRVShaderStageInputAttribs& GetShaderStageInputAttribs(Uint32 n)noexcept
    {
        return const_cast<SPIRVShaderStageInputAttribs&>(const_cast<const SPIRVShaderResources*>(this)->GetShaderStageInputAttribs(n));
    }

    // Memory buffer that holds all resources as continuous chunk of memory:
    // |  UBs  |  SBs  |  StrgImgs  |  SmplImgs  |  ACs  |  SepSamplers  |  SepImgs  | Immutable Samplers | Stage Inputs | Resource Names |
    std::unique_ptr< void, STDDeleterRawMem<void> > m_MemoryBuffer;
    StringPool m_ResourceNames;

    const char* m_CombinedSamplerSuffix = nullptr;

    using OffsetType = Uint16;
    OffsetType m_StorageBufferOffset   = 0;
    OffsetType m_StorageImageOffset    = 0;
    OffsetType m_SampledImageOffset    = 0;
    OffsetType m_AtomicCounterOffset   = 0;
    OffsetType m_SeparateSamplerOffset = 0;
    OffsetType m_SeparateImageOffset   = 0;
    OffsetType m_TotalResources        = 0;
    OffsetType m_NumImmutableSamplers  = 0;
    OffsetType m_NumShaderStageInputs  = 0;

    SHADER_TYPE m_ShaderType = SHADER_TYPE_UNKNOWN;
};

}

namespace std
{
#if 0
    template<>
    struct hash<Diligent::D3DShaderResourceAttribs>
    {
        size_t operator()(const Diligent::D3DShaderResourceAttribs &Attribs) const
        {
            return Attribs.GetHash();
        }
    };

    template<>
    struct hash<Diligent::ShaderResources>
    {
        size_t operator()(const Diligent::ShaderResources &Res) const
        {
            return Res.GetHash();
        }
    };
#endif
}
