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
//   m_MemoryBuffer                                                                                                              m_BufferEndOffset
//    |                                                                                                                             |
//    | Uniform Buffers | Storage Buffers | Storage Images | Sampled Images | Atomic Counters | Separate Images | Separate Samplers |

#include <memory>
#include <vector>

#include "Shader.h"
#include "STDAllocator.h"
#include "HashUtils.h"

namespace Diligent
{

inline bool IsAllowedType(SHADER_VARIABLE_TYPE VarType, Uint32 AllowedTypeBits)noexcept
{
    return ((1 << VarType) & AllowedTypeBits) != 0;
}

inline Uint32 GetAllowedTypeBits(const SHADER_VARIABLE_TYPE *AllowedVarTypes, Uint32 NumAllowedTypes)noexcept
{
    if(AllowedVarTypes == nullptr)
        return 0xFFFFFFFF;

    Uint32 AllowedTypeBits = 0;
    for(Uint32 i=0; i < NumAllowedTypes; ++i)
        AllowedTypeBits |= 1 << AllowedVarTypes[i];
    return AllowedTypeBits;
}


struct SPIRVShaderResourceAttribs
{
    enum ResourceType : Uint8
    {
        UniformBuffer,
        StorageBuffer,
        StorageImage,
        SampledImage,
        AtomicCounter,
        SeparateImage,
        SeparateSampler
    };

    const String Name;
    const Uint8 Binding;
    const Uint8 Location;
    const Uint16 ArraySize;
    const ResourceType Type;
    const SHADER_VARIABLE_TYPE VarType : 8;

    SPIRVShaderResourceAttribs(String _Name,
                               Uint8 _Binding,
                               Uint8 _Location,
                               Uint16 _ArraySize,
                               ResourceType _Type,
                               SHADER_VARIABLE_TYPE _VarType) :
        Name(std::move(_Name)),
        Binding(_Binding),
        Location(_Location),
        ArraySize(_ArraySize),
        Type(_Type),
        VarType(_VarType)
    {}

    String GetPrintName(Uint32 ArrayInd)const
    {
        VERIFY_EXPR(ArrayInd < ArraySize);
        if (ArraySize > 1)
            return Name+ '[' + std::to_string(ArrayInd) + ']';
        else
            return Name;
    }

    bool IsCompatibleWith(const SPIRVShaderResourceAttribs& Attibs)const
    {
        return Binding   == Attibs.Binding &&
               Location  == Attibs.Location &&
               ArraySize == Attibs.ArraySize && 
               Type      == Attibs.Type;
    }
};


/// Diligent::SPIRVShaderResources class
class SPIRVShaderResources
{
public:
    SPIRVShaderResources(IMemoryAllocator &Allocator, SHADER_TYPE ShaderType, std::vector<uint32_t> spirv_binary);

    // Copies specified types of resources from another ShaderResources objects
    // Only resources listed in AllowedVarTypes are copied
    SPIRVShaderResources(IMemoryAllocator &Allocator,
                         const SPIRVShaderResources& SrcResources,
                         const SHADER_VARIABLE_TYPE *AllowedVarTypes, 
                         Uint32 NumAllowedTypes);

    SPIRVShaderResources             (const SPIRVShaderResources&) = delete;
    SPIRVShaderResources             (SPIRVShaderResources&&)      = delete;
    SPIRVShaderResources& operator = (const SPIRVShaderResources&) = delete;
    SPIRVShaderResources& operator = (SPIRVShaderResources&&)      = delete;
    
    ~SPIRVShaderResources();
    
    Uint32 GetNumUBs     ()const noexcept{ return (m_StorageBufferOffset   - 0);                      }
    Uint32 GetNumSBs     ()const noexcept{ return (m_StorageImageOffset    - m_StorageBufferOffset);  }
    Uint32 GetNumImgs    ()const noexcept{ return (m_SampledImageOffset    - m_StorageImageOffset);   }
    Uint32 GetNumSmplImgs()const noexcept{ return (m_AtomicCounterOffset   - m_SampledImageOffset);   }
    Uint32 GetNumACs     ()const noexcept{ return (m_SeparateImageOffset   - m_AtomicCounterOffset);  }
    Uint32 GetNumSepImgs ()const noexcept{ return (m_SeparateSamplerOffset - m_SeparateImageOffset);  }
    Uint32 GetNumSepSmpls()const noexcept{ return (m_BufferEndOffset       - m_SeparateSamplerOffset);}

    const SPIRVShaderResourceAttribs& GetUB      (Uint32 n)const noexcept{ return GetResAttribs(n, GetNumUBs(),      0                      ); }
    const SPIRVShaderResourceAttribs& GetSB      (Uint32 n)const noexcept{ return GetResAttribs(n, GetNumSBs(),      m_StorageBufferOffset  ); }
    const SPIRVShaderResourceAttribs& GetImg     (Uint32 n)const noexcept{ return GetResAttribs(n, GetNumImgs(),     m_StorageImageOffset   ); }
    const SPIRVShaderResourceAttribs& GetSmplImg (Uint32 n)const noexcept{ return GetResAttribs(n, GetNumSmplImgs(), m_SampledImageOffset   ); }
    const SPIRVShaderResourceAttribs& GetAC      (Uint32 n)const noexcept{ return GetResAttribs(n, GetNumACs(),      m_AtomicCounterOffset  ); }
    const SPIRVShaderResourceAttribs& GetSepImg  (Uint32 n)const noexcept{ return GetResAttribs(n, GetNumSepImgs(),  m_SeparateImageOffset  ); }
    const SPIRVShaderResourceAttribs& GetSepSmpl (Uint32 n)const noexcept{ return GetResAttribs(n, GetNumSepSmpls(), m_SeparateSamplerOffset); }

    void CountResources(const SHADER_VARIABLE_TYPE *AllowedVarTypes, 
                        Uint32 NumAllowedTypes, 
                        Uint32& NumUBs, 
                        Uint32& NumSBs, 
                        Uint32& NumImgs, 
                        Uint32& NumSmplImgs, 
                        Uint32& NumACs, 
                        Uint32& NumSepImgs,
                        Uint32& NumSepSmpls)const noexcept;

    SHADER_TYPE GetShaderType()const noexcept{return m_ShaderType;}

    // Process only resources listed in AllowedVarTypes
    template<typename THandleUB,
             typename THandleSB,
             typename THandleImg,
             typename THandleSmplImg,
             typename THandleAC,
             typename THandleSepImg,
             typename THandleSepSmpl>
    void ProcessResources(const SHADER_VARIABLE_TYPE *AllowedVarTypes, 
                          Uint32 NumAllowedTypes,
                          THandleUB HandleUB,
                          THandleSB HandleSB,
                          THandleImg HandleImg,
                          THandleSmplImg HandleSmplImg,
                          THandleAC HandleAC,
                          THandleSepImg HandleSepImg,
                          THandleSepSmpl HandleSepSmpl)const
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

        for (Uint32 n = 0; n < GetNumSmplImgs(); ++n)
        {
            const auto& SmplImg = GetSmplImg(n);
            if (IsAllowedType(SmplImg.VarType, AllowedTypeBits))
                HandleSmplImg(SmplImg, n);
        }

        for (Uint32 n = 0; n < GetNumACs(); ++n)
        {
            const auto& AC = GetAC(n);
            if (IsAllowedType(AC.VarType, AllowedTypeBits))
                HandleAC(AC, n);
        }

        for (Uint32 n = 0; n < GetNumSepImgs(); ++n)
        {
            const auto& SepImg = GetSepImg(n);
            if (IsAllowedType(SepImg.VarType, AllowedTypeBits))
                HandleSepImg(SepImg, n);
        }

        for (Uint32 n = 0; n < GetNumSepSmpls(); ++n)
        {
            const auto& SepSmpl = GetSepSmpl(n);
            if (IsAllowedType(SepSmpl.VarType, AllowedTypeBits))
                HandleSepSmpl(SepSmpl, n);
        }
    }

    //bool IsCompatibleWith(const ShaderResources &Resources)const;
    
    //size_t GetHash()const;

protected:
    void Initialize(IMemoryAllocator &Allocator, Uint32 NumUBs, Uint32 NumSBs, Uint32 NumImgs, Uint32 NumSmplImgs, Uint32 NumACs, Uint32 NumSepImgs, Uint32 NumSepSmpls);

    __forceinline SPIRVShaderResourceAttribs& GetResAttribs(Uint32 n, Uint32 NumResources, Uint32 Offset)noexcept
    {
        VERIFY(n < NumResources, "Resource index (", n, ") is out of range. Max allowed index: ", NumResources-1);
        VERIFY_EXPR(Offset + n < m_BufferEndOffset);
        return reinterpret_cast<SPIRVShaderResourceAttribs*>(m_MemoryBuffer.get())[Offset + n];
    }

    __forceinline const SPIRVShaderResourceAttribs& GetResAttribs(Uint32 n, Uint32 NumResources, Uint32 Offset)const noexcept
    {
        VERIFY(n < NumResources, "Resource index (", n, ") is out of range. Max allowed index: ", NumResources-1);
        VERIFY_EXPR(Offset + n < m_BufferEndOffset);
        return reinterpret_cast<SPIRVShaderResourceAttribs*>(m_MemoryBuffer.get())[Offset + n];
    }

    SPIRVShaderResourceAttribs& GetUB      (Uint32 n)noexcept{ return GetResAttribs(n, GetNumUBs(),      0                      ); }
    SPIRVShaderResourceAttribs& GetSB      (Uint32 n)noexcept{ return GetResAttribs(n, GetNumSBs(),      m_StorageBufferOffset  ); }
    SPIRVShaderResourceAttribs& GetImg     (Uint32 n)noexcept{ return GetResAttribs(n, GetNumImgs(),     m_StorageImageOffset   ); }
    SPIRVShaderResourceAttribs& GetSmplImg (Uint32 n)noexcept{ return GetResAttribs(n, GetNumSmplImgs(), m_SampledImageOffset   ); }
    SPIRVShaderResourceAttribs& GetAC      (Uint32 n)noexcept{ return GetResAttribs(n, GetNumACs(),      m_AtomicCounterOffset  ); }
    SPIRVShaderResourceAttribs& GetSepImg  (Uint32 n)noexcept{ return GetResAttribs(n, GetNumSepImgs(),  m_SeparateImageOffset  ); }
    SPIRVShaderResourceAttribs& GetSepSmpl (Uint32 n)noexcept{ return GetResAttribs(n, GetNumSepSmpls(), m_SeparateSamplerOffset); }

private:
    // Memory buffer that holds all resources as continuous chunk of memory:
    // |  UBs  |  SBs  |  StrgImgs  |  SmplImgs  |  ACs  |  SepImgs  |  SepSamplers  |
    std::unique_ptr< void, STDDeleterRawMem<void> > m_MemoryBuffer;

    using OffsetType = Uint16;
    OffsetType m_StorageBufferOffset   = 0;
    OffsetType m_StorageImageOffset    = 0;
    OffsetType m_SampledImageOffset    = 0;
    OffsetType m_AtomicCounterOffset   = 0;
    OffsetType m_SeparateImageOffset   = 0;
    OffsetType m_SeparateSamplerOffset = 0;
    OffsetType m_BufferEndOffset       = 0;

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
