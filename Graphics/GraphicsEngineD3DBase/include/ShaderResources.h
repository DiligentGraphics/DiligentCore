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
/// Declaration of Diligent::ShaderResources class
/// See http://diligentgraphics.com/diligent-engine/architecture/d3d12/shader-resources/

// ShaderResources class uses continuous chunk of memory to store all resources, as follows:
//
//
//       m_MemoryBuffer            m_TexSRVOffset                      m_TexUAVOffset                      m_BufSRVOffset                      m_BufUAVOffset                      m_SamplersOffset              m_MemorySize
//        |                         |                                   |                                   |                                   |                                   |                             |
//        |  CB[0]  ...  CB[Ncb-1]  |  TexSRV[0]  ...  TexSRV[Ntsrv-1]  |  TexUAV[0]  ...  TexUAV[Ntuav-1]  |  BufSRV[0]  ...  BufSRV[Nbsrv-1]  |  BufUAV[0]  ...  BufUAV[Nbuav-1]  |  Sam[0]  ...  Sam[Nsam-1]   |
//
//  Ncb   - number of constant buffers
//  Ntsrv - number of texture SRVs
//  Ntuav - number of texture UAVs
//  Nbsrv - number of buffer SRVs
//  Nbuav - number of buffer UAVs
//  Nsam  - number of samplers
//  
//
//  If texture SRV is assigned a sampler, it is referenced through SamplerId:
// 
//                           _________________________SamplerId_______________________
//                          |                                                         |
//                          |                                                         V
//   |  CBs   |   ...   TexSRV[n] ...   | TexUAVs | BufSRVs | BufUAVs |  Sam[0] ...  Sam[SamplerId] ... |
//  
//

#include <memory>

#define NOMINMAX
#include <d3dcommon.h>

#include "Shader.h"
#include "STDAllocator.h"
#include "HashUtils.h"
#include "StringPool.h"

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


struct D3DShaderResourceAttribs 
{
    const char* const Name;

    const Uint16 BindPoint;
    const Uint16 BindCount;

private:
    //            4              3               4              20                1
    // bit | 0  1  2  3   |  4   5   6   |  7  8  9  10 | 11  12 ...  30 |        31         |   
    //     |              |              |              |                |                   |
    //     |  InputType   | VariableType |   SRV Dim    |    SamplerId   | StaticSamplerFlag |
    static constexpr const Uint32 ShaderInputTypeBits    = 4;
    static constexpr const Uint32 VariableTypeBits       = 3;
    static constexpr const Uint32 SRVDimBits             = 4;
    static constexpr const Uint32 SamplerIdBits          = 20;
    static constexpr const Uint32 StaticSamplerFlagBits  = 1;
    static_assert(ShaderInputTypeBits + VariableTypeBits + SRVDimBits + SamplerIdBits + StaticSamplerFlagBits == 32, "Attributes are better be packed into 32 bits");

    static_assert(D3D_SIT_UAV_RWSTRUCTURED_WITH_COUNTER < (1 << ShaderInputTypeBits), "Not enough bits to represent D3D_SHADER_INPUT_TYPE");
    static_assert(SHADER_VARIABLE_TYPE_NUM_TYPES        < (1 << VariableTypeBits),    "Not enough bits to represent SHADER_VARIABLE_TYPE");
    static_assert(D3D_SRV_DIMENSION_BUFFEREX            < (1 << SRVDimBits),          "Not enough bits to represent D3D_SRV_DIMENSION");

    // We need to use Uint32 instead of the actual type for reliability and correctness.
    // There originally was a problem when the type of InputType was D3D_SHADER_INPUT_TYPE:
    // the value of D3D_SIT_UAV_RWBYTEADDRESS (8) was interpreted as -8 (as the underlying enum type 
    // is signed) causing errors
    const Uint32  InputType         : ShaderInputTypeBits;      // Max value: D3D_SIT_UAV_RWSTRUCTURED_WITH_COUNTER == 11
    const Uint32  VariableType      : VariableTypeBits;         // Max value: SHADER_VARIABLE_TYPE_DYNAMIC == 2
    const Uint32  SRVDimension      : SRVDimBits;               // Max value: D3D_SRV_DIMENSION_BUFFEREX == 11
    const Uint32  SamplerId         : SamplerIdBits;            // Max value: 1048575
    const Uint32  StaticSamplerFlag : StaticSamplerFlagBits;    // Needs to be Uint32, otherwise sizeof(D3DShaderResourceAttribs)==24
                                                                // (https://stackoverflow.com/questions/308364/c-bitfield-packing-with-bools)

public:
    static constexpr const Uint32 InvalidSamplerId = (1 << SamplerIdBits) - 1;
    static constexpr const Uint16 InvalidBindPoint = std::numeric_limits<Uint16>::max();
    static constexpr const Uint16 MaxBindPoint     = InvalidBindPoint - 1;
    static constexpr const Uint16 MaxBindCount     = std::numeric_limits<Uint16>::max();


    D3DShaderResourceAttribs(const char*            _Name, 
                             UINT                   _BindPoint, 
                             UINT                   _BindCount, 
                             D3D_SHADER_INPUT_TYPE  _InputType, 
                             SHADER_VARIABLE_TYPE   _VariableType, 
                             D3D_SRV_DIMENSION      _SRVDimension,
                             Uint32                 _SamplerId, 
                             bool                   _IsStaticSampler)noexcept :
        Name             (_Name),
        BindPoint        (static_cast<decltype(BindPoint)>   (_BindPoint)),
        BindCount        (static_cast<decltype(BindCount)>   (_BindCount)),
        InputType        (static_cast<decltype(InputType)>   (_InputType)),
        VariableType     (static_cast<decltype(VariableType)>(_VariableType)),
        SRVDimension     (static_cast<decltype(SRVDimension)>(_SRVDimension)),
        SamplerId        (_SamplerId),
        StaticSamplerFlag(_IsStaticSampler ? 1 : 0)
    {
        VERIFY(_BindPoint <= MaxBindPoint || _BindPoint == InvalidBindPoint, "Bind Point is out of allowed range");
        VERIFY(_BindCount <= MaxBindCount, "Bind Count is out of allowed range");
        VERIFY(_InputType     < (1 << ShaderInputTypeBits), "Shader input type is out of expected range");
        VERIFY(_VariableType  < (1 << VariableTypeBits),    "Variable type is out of expected range");
        VERIFY(_SRVDimension  < (1 << SRVDimBits),          "SRV dimensions is out of expected range");
        VERIFY(_SamplerId     < (1 << SamplerIdBits),       "SamplerId is out of representable range");
#ifdef _DEBUG
        if (_InputType==D3D_SIT_SAMPLER)
            VERIFY_EXPR(IsStaticSampler() == _IsStaticSampler);
        else
            VERIFY(!_IsStaticSampler, "Only samplers can be marked as static");

        if (_InputType == D3D_SIT_TEXTURE)
            VERIFY_EXPR(SamplerId == _SamplerId);
        else
            VERIFY(SamplerId == InvalidSamplerId, "Only textures can be assigned a valid texture sampler");

        if (_IsStaticSampler)
            VERIFY( _InputType == D3D_SIT_SAMPLER, "Invalid input type: D3D_SIT_SAMPLER is expected" );
#endif
    }

    D3DShaderResourceAttribs(StringPool& NamesPool, const D3DShaderResourceAttribs& rhs, Uint32 SamplerId)noexcept : 
        D3DShaderResourceAttribs
        {
            NamesPool.CopyString(rhs.Name),
            rhs.BindPoint,
            rhs.BindCount,
            rhs.GetInputType(),
            rhs.GetVariableType(),
            rhs.GetSRVDimension(),
            SamplerId,
            false
        }
    {
        VERIFY(InputType == D3D_SIT_TEXTURE, "Only textures can be assigned a texture sampler");
    }

    D3DShaderResourceAttribs(StringPool& NamesPool, const D3DShaderResourceAttribs& rhs)noexcept : 
        D3DShaderResourceAttribs
        {
            NamesPool.CopyString(rhs.Name),
            rhs.BindPoint,
            rhs.BindCount,
            rhs.GetInputType(),
            rhs.GetVariableType(),
            rhs.GetSRVDimension(),
            rhs.SamplerId,
            rhs.StaticSamplerFlag !=0 ? true : false
        }
    {
    }

    D3DShaderResourceAttribs             (const D3DShaderResourceAttribs&  rhs) = delete;
    D3DShaderResourceAttribs             (      D3DShaderResourceAttribs&& rhs) = default; // Required for vector<D3DShaderResourceAttribs>
    D3DShaderResourceAttribs& operator = (const D3DShaderResourceAttribs&  rhs) = delete;
    D3DShaderResourceAttribs& operator = (      D3DShaderResourceAttribs&& rhs) = delete;
    
    D3D_SHADER_INPUT_TYPE GetInputType()const
    {
        return static_cast<D3D_SHADER_INPUT_TYPE>(InputType);
    }

    SHADER_VARIABLE_TYPE GetVariableType()const
    {
        return static_cast<SHADER_VARIABLE_TYPE>(VariableType);
    }

    D3D_SRV_DIMENSION GetSRVDimension()const
    {
        return static_cast<D3D_SRV_DIMENSION>(SRVDimension);
    }

    Uint32 GetSamplerId()const
    {
        VERIFY( InputType == D3D_SIT_TEXTURE, "Invalid input type: D3D_SIT_TEXTURE is expected" );
        return SamplerId;
    }

    bool IsStaticSampler()const
    {
        VERIFY( InputType == D3D_SIT_SAMPLER, "Invalid input type: D3D_SIT_SAMPLER is expected" );
        return StaticSamplerFlag != 0;
    }

    bool IsValidSampler()const
    {
        return GetSamplerId() != InvalidSamplerId;
    }

    bool IsValidBindPoint()const
    {
        return BindPoint != InvalidBindPoint;
    }

    String GetPrintName(Uint32 ArrayInd)const
    {
        VERIFY_EXPR(ArrayInd < BindCount);
        if(BindCount > 1)
            return String(Name) + '[' + std::to_string(ArrayInd) + ']';
        else
            return Name;
    }

    bool IsCompatibleWith(const D3DShaderResourceAttribs& Attribs)const
    {
        return BindPoint         == Attribs.BindPoint       &&
               BindCount         == Attribs.BindCount       &&
               InputType         == Attribs.InputType       &&
               VariableType      == Attribs.VariableType    &&
               SRVDimension      == Attribs.SRVDimension    &&
               SamplerId         == Attribs.SamplerId       &&
               StaticSamplerFlag == Attribs.StaticSamplerFlag;
    }

    size_t GetHash()const
    {
        return ComputeHash(BindPoint, BindCount, InputType, VariableType, SRVDimension, SamplerId, StaticSamplerFlag);
    }

    bool IsAllowedType(Uint32 AllowedTypeBits)const
    {
        return Diligent::IsAllowedType(GetVariableType(), AllowedTypeBits);
    }
};
static_assert(sizeof(D3DShaderResourceAttribs) == sizeof(void*) + sizeof(Uint32)*2, "Unexpected sizeof(D3DShaderResourceAttribs)");


/// Diligent::ShaderResources class
class ShaderResources
{
public:
    ShaderResources(SHADER_TYPE ShaderType);

    ShaderResources             (const ShaderResources&) = delete;
    ShaderResources             (ShaderResources&&)      = delete;
    ShaderResources& operator = (const ShaderResources&) = delete;
    ShaderResources& operator = (ShaderResources&&)      = delete;
    
    ~ShaderResources();
    
    Uint32 GetNumCBs()     const noexcept{ return (m_TexSRVOffset   - 0);                }
    Uint32 GetNumTexSRV()  const noexcept{ return (m_TexUAVOffset   - m_TexSRVOffset);   }
    Uint32 GetNumTexUAV()  const noexcept{ return (m_BufSRVOffset   - m_TexUAVOffset);   }
    Uint32 GetNumBufSRV()  const noexcept{ return (m_BufUAVOffset   - m_BufSRVOffset);   }
    Uint32 GetNumBufUAV()  const noexcept{ return (m_SamplersOffset - m_BufUAVOffset);   }
    Uint32 GetNumSamplers()const noexcept{ return (m_TotalResources - m_SamplersOffset); }

    const D3DShaderResourceAttribs& GetCB     (Uint32 n)const noexcept{ return GetResAttribs(n, GetNumCBs(),                   0); }
    const D3DShaderResourceAttribs& GetTexSRV (Uint32 n)const noexcept{ return GetResAttribs(n, GetNumTexSRV(),   m_TexSRVOffset); }
    const D3DShaderResourceAttribs& GetTexUAV (Uint32 n)const noexcept{ return GetResAttribs(n, GetNumTexUAV(),   m_TexUAVOffset); }
    const D3DShaderResourceAttribs& GetBufSRV (Uint32 n)const noexcept{ return GetResAttribs(n, GetNumBufSRV(),   m_BufSRVOffset); }
    const D3DShaderResourceAttribs& GetBufUAV (Uint32 n)const noexcept{ return GetResAttribs(n, GetNumBufUAV(),   m_BufUAVOffset); }
    const D3DShaderResourceAttribs& GetSampler(Uint32 n)const noexcept{ return GetResAttribs(n, GetNumSamplers(), m_SamplersOffset); }

    
    void CountResources(const SHADER_VARIABLE_TYPE *AllowedVarTypes, Uint32 NumAllowedTypes, 
                        Uint32& NumCBs, Uint32& NumTexSRVs, Uint32& NumTexUAVs, 
                        Uint32& NumBufSRVs, Uint32& NumBufUAVs, Uint32& NumSamplers)const noexcept;

    SHADER_TYPE GetShaderType()const noexcept{return m_ShaderType;}

    // Process only resources listed in AllowedVarTypes
    template<typename THandleCB,
             typename THandleTexSRV,
             typename THandleTexUAV,
             typename THandleBufSRV,
             typename THandleBufUAV>
    void ProcessResources(const SHADER_VARIABLE_TYPE *AllowedVarTypes, 
                          Uint32 NumAllowedTypes,
                          THandleCB HandleCB,
                          THandleTexSRV HandleTexSRV,
                          THandleTexUAV HandleTexUAV,
                          THandleBufSRV HandleBufSRV,
                          THandleBufUAV HandleBufUAV)const
    {
        Uint32 AllowedTypeBits = GetAllowedTypeBits(AllowedVarTypes, NumAllowedTypes);

        for(Uint32 n=0; n < GetNumCBs(); ++n)
        {
            const auto& CB = GetCB(n);
            if( CB.IsAllowedType(AllowedTypeBits) )
                HandleCB(CB, n);
        }

        for(Uint32 n=0; n < GetNumTexSRV(); ++n)
        {
            const auto &TexSRV = GetTexSRV(n);
            if( TexSRV.IsAllowedType(AllowedTypeBits) )
                HandleTexSRV(TexSRV, n);
        }
    
        for(Uint32 n=0; n < GetNumTexUAV(); ++n)
        {
            const auto &TexUAV = GetTexUAV(n);
            if( TexUAV.IsAllowedType(AllowedTypeBits) )
                HandleTexUAV(TexUAV, n);
        }

        for(Uint32 n=0; n < GetNumBufSRV(); ++n)
        {
            const auto &BufSRV = GetBufSRV(n);
            if( BufSRV.IsAllowedType(AllowedTypeBits) )
                HandleBufSRV(BufSRV, n);
        }

        for(Uint32 n=0; n < GetNumBufUAV(); ++n)
        {
            const auto& BufUAV = GetBufUAV(n);
            if( BufUAV.IsAllowedType(AllowedTypeBits) )
                HandleBufUAV(BufUAV, n);
        }
    }

    bool IsCompatibleWith(const ShaderResources &Resources)const;
    
    size_t GetHash()const;

protected:
    void Initialize(IMemoryAllocator& Allocator, 
                    Uint32            NumCBs, 
                    Uint32            NumTexSRVs, 
                    Uint32            NumTexUAVs, 
                    Uint32            NumBufSRVs, 
                    Uint32            NumBufUAVs, 
                    Uint32            NumSamplers,
                    size_t            ResourceNamesPoolSize);

    __forceinline D3DShaderResourceAttribs& GetResAttribs(Uint32 n, Uint32 NumResources, Uint32 Offset)noexcept
    {
        VERIFY(n < NumResources, "Resource index (", n, ") is out of range. Resource array size: ", NumResources);
        VERIFY_EXPR(Offset + n < m_TotalResources);
        return reinterpret_cast<D3DShaderResourceAttribs*>(m_MemoryBuffer.get())[Offset + n];
    }

    __forceinline const D3DShaderResourceAttribs& GetResAttribs(Uint32 n, Uint32 NumResources, Uint32 Offset)const noexcept
    {
        VERIFY(n < NumResources, "Resource index (", n, ") is out of range. Resource array size: ", NumResources);
        VERIFY_EXPR(Offset + n < m_TotalResources);
        return reinterpret_cast<D3DShaderResourceAttribs*>(m_MemoryBuffer.get())[Offset + n];
    }

    D3DShaderResourceAttribs& GetCB     (Uint32 n)noexcept{ return GetResAttribs(n, GetNumCBs(),                   0); }
    D3DShaderResourceAttribs& GetTexSRV (Uint32 n)noexcept{ return GetResAttribs(n, GetNumTexSRV(),   m_TexSRVOffset); }
    D3DShaderResourceAttribs& GetTexUAV (Uint32 n)noexcept{ return GetResAttribs(n, GetNumTexUAV(),   m_TexUAVOffset); }
    D3DShaderResourceAttribs& GetBufSRV (Uint32 n)noexcept{ return GetResAttribs(n, GetNumBufSRV(),   m_BufSRVOffset); }
    D3DShaderResourceAttribs& GetBufUAV (Uint32 n)noexcept{ return GetResAttribs(n, GetNumBufUAV(),   m_BufUAVOffset); }
    D3DShaderResourceAttribs& GetSampler(Uint32 n)noexcept{ return GetResAttribs(n, GetNumSamplers(), m_SamplersOffset); }

    Uint32 FindAssignedSamplerId(const D3DShaderResourceAttribs& TexSRV, const char* SamplerSuffix)const;

private:
    // Memory buffer that holds all resources as continuous chunk of memory:
    // | CBs | TexSRVs | TexUAVs | BufSRVs | BufUAVs | Samplers |  Resource Names  |
    std::unique_ptr< void, STDDeleterRawMem<void> > m_MemoryBuffer;

protected:
    StringPool m_ResourceNames;

private:    
    // Offsets in elements of D3DShaderResourceAttribs
    typedef Uint16 OffsetType;
    OffsetType m_TexSRVOffset = 0;
    OffsetType m_TexUAVOffset = 0;
    OffsetType m_BufSRVOffset = 0;
    OffsetType m_BufUAVOffset = 0;
    OffsetType m_SamplersOffset = 0;
    OffsetType m_TotalResources = 0;

    SHADER_TYPE m_ShaderType = SHADER_TYPE_UNKNOWN;
};

}

namespace std
{
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
}
