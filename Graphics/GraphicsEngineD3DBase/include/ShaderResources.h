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
//  If texture SRV is assigned a sampler, it is cross-referenced through SamplerOrTexSRVId:
// 
//                           _____________________SamplerOrTexSRVId___________________
//                          |                                                         |
//                          |                                                         V
//   |  CBs   |   ...   TexSRV[n] ...   | TexUAVs | BufSRVs | BufUAVs |  Sam[0] ...  Sam[SamplerId] ... |
//                          A                                                         |
//                          '---------------------SamplerOrTexSRVId-------------------'
//

#include <memory>

#define NOMINMAX
#include <d3dcommon.h>

#include "Shader.h"
#include "STDAllocator.h"
#include "HashUtils.h"
#include "StringPool.h"
#include "D3DShaderResourceLoader.h"

namespace Diligent
{

inline bool IsAllowedType(SHADER_VARIABLE_TYPE VarType, Uint32 AllowedTypeBits)noexcept
{
    return ((1 << VarType) & AllowedTypeBits) != 0;
}

inline Uint32 GetAllowedTypeBits(const SHADER_VARIABLE_TYPE* AllowedVarTypes, Uint32 NumAllowedTypes)noexcept
{
    if (AllowedVarTypes == nullptr)
        return 0xFFFFFFFF;

    Uint32 AllowedTypeBits = 0;
    for (Uint32 i=0; i < NumAllowedTypes; ++i)
        AllowedTypeBits |= 1 << AllowedVarTypes[i];
    return AllowedTypeBits;
}


struct D3DShaderResourceAttribs 
{
    const char* const Name;

    const Uint16 BindPoint;
    const Uint16 BindCount;

private:
    //            4              3               4                 20                    1
    // bit | 0  1  2  3   |  4   5   6   |  7  8  9  10 | 11  12  13   ...   30 |        31         |   
    //     |              |              |              |                       |                   |
    //     |  InputType   | VariableType |   SRV Dim    | SamplerOrTexSRVIdBits | StaticSamplerFlag |
    static constexpr const Uint32 ShaderInputTypeBits    = 4;
    static constexpr const Uint32 VariableTypeBits       = 3;
    static constexpr const Uint32 SRVDimBits             = 4;
    static constexpr const Uint32 SamplerOrTexSRVIdBits = 20;
    static constexpr const Uint32 StaticSamplerFlagBits  = 1;
    static_assert(ShaderInputTypeBits + VariableTypeBits + SRVDimBits + SamplerOrTexSRVIdBits + StaticSamplerFlagBits == 32, "Attributes are better be packed into 32 bits");

    static_assert(D3D_SIT_UAV_RWSTRUCTURED_WITH_COUNTER < (1 << ShaderInputTypeBits), "Not enough bits to represent D3D_SHADER_INPUT_TYPE");
    static_assert(SHADER_VARIABLE_TYPE_NUM_TYPES        < (1 << VariableTypeBits),    "Not enough bits to represent SHADER_VARIABLE_TYPE");
    static_assert(D3D_SRV_DIMENSION_BUFFEREX            < (1 << SRVDimBits),          "Not enough bits to represent D3D_SRV_DIMENSION");

    // We need to use Uint32 instead of the actual type for reliability and correctness.
    // There originally was a problem when the type of InputType was D3D_SHADER_INPUT_TYPE:
    // the value of D3D_SIT_UAV_RWBYTEADDRESS (8) was interpreted as -8 (as the underlying enum type 
    // is signed) causing errors
    const Uint32  InputType          : ShaderInputTypeBits;     // Max value: D3D_SIT_UAV_RWSTRUCTURED_WITH_COUNTER == 11
    const Uint32  VariableType       : VariableTypeBits;        // Max value: SHADER_VARIABLE_TYPE_DYNAMIC == 2
    const Uint32  SRVDimension       : SRVDimBits;              // Max value: D3D_SRV_DIMENSION_BUFFEREX == 11
          Uint32  SamplerOrTexSRVId  : SamplerOrTexSRVIdBits;   // Max value: 1048575
    const Uint32  StaticSamplerFlag  : StaticSamplerFlagBits;   // Needs to be Uint32, otherwise sizeof(D3DShaderResourceAttribs)==24
                                                                // (https://stackoverflow.com/questions/308364/c-bitfield-packing-with-bools)

public:
    static constexpr const Uint32 InvalidSamplerId = (1 << SamplerOrTexSRVIdBits) - 1;
    static constexpr const Uint32 InvalidTexSRVId  = (1 << SamplerOrTexSRVIdBits) - 1;
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
        Name               (_Name),
        BindPoint          (static_cast<decltype(BindPoint)>   (_BindPoint)),
        BindCount          (static_cast<decltype(BindCount)>   (_BindCount)),
        InputType          (static_cast<decltype(InputType)>   (_InputType)),
        VariableType       (static_cast<decltype(VariableType)>(_VariableType)),
        SRVDimension       (static_cast<decltype(SRVDimension)>(_SRVDimension)),
        SamplerOrTexSRVId  (_SamplerId),
        StaticSamplerFlag  (_IsStaticSampler ? 1 : 0)
    {
#ifdef _DEBUG
        VERIFY(_BindPoint <= MaxBindPoint || _BindPoint == InvalidBindPoint, "Bind Point is out of allowed range");
        VERIFY(_BindCount <= MaxBindCount, "Bind Count is out of allowed range");
        VERIFY(_InputType    < (1 << ShaderInputTypeBits),   "Shader input type is out of expected range");
        VERIFY(_VariableType < (1 << VariableTypeBits),      "Variable type is out of expected range");
        VERIFY(_SRVDimension < (1 << SRVDimBits),            "SRV dimensions is out of expected range");
        VERIFY(_SamplerId    < (1 << SamplerOrTexSRVIdBits), "SamplerOrTexSRVId is out of representable range");

        if (_InputType==D3D_SIT_SAMPLER)
            VERIFY_EXPR(IsStaticSampler() == _IsStaticSampler);
        else
            VERIFY(!_IsStaticSampler, "Only samplers can be labeled as static");

        if (_InputType == D3D_SIT_TEXTURE && _SRVDimension != D3D_SRV_DIMENSION_BUFFER)
            VERIFY_EXPR(GetSamplerId() == _SamplerId);
        else
            VERIFY(_SamplerId == InvalidSamplerId, "Only texture SRV can be assigned a valid texture sampler");

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
        VERIFY(GetInputType() == D3D_SIT_TEXTURE && GetSRVDimension() != D3D_SRV_DIMENSION_BUFFER, "Only texture SRV can be assigned a texture sampler");
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
            rhs.SamplerOrTexSRVId,
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
        VERIFY(GetInputType() == D3D_SIT_TEXTURE && GetSRVDimension() != D3D_SRV_DIMENSION_BUFFER, "Invalid input type: D3D_SIT_TEXTURE is expected" );
        return SamplerOrTexSRVId;
    }

    void SetTexSRVId(Uint32 TexSRVId)
    {
        VERIFY(GetInputType() == D3D_SIT_SAMPLER, "Invalid input type: D3D_SIT_SAMPLER is expected" );
        VERIFY(TexSRVId < (1 << SamplerOrTexSRVIdBits), "TexSRVId (", TexSRVId, ") is out of representable range");
        SamplerOrTexSRVId = TexSRVId;
    }

    Uint32 GetTexSRVId()const
    {
        VERIFY(GetInputType() == D3D_SIT_SAMPLER, "Invalid input type: D3D_SIT_SAMPLER is expected" );
        return SamplerOrTexSRVId;
    }

    bool IsStaticSampler()const
    {
        VERIFY(GetInputType() == D3D_SIT_SAMPLER, "Invalid input type: D3D_SIT_SAMPLER is expected" );
        return StaticSamplerFlag != 0;
    }

    bool ValidSamplerAssigned()const
    {
        return GetSamplerId() != InvalidSamplerId;
    }

    bool ValidTexSRVAssigned()const
    {
        return GetTexSRVId() != InvalidTexSRVId;
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
        return BindPoint          == Attribs.BindPoint          &&
               BindCount          == Attribs.BindCount          &&
               InputType          == Attribs.InputType          &&
               VariableType       == Attribs.VariableType       &&
               SRVDimension       == Attribs.SRVDimension       &&
               SamplerOrTexSRVId  == Attribs.SamplerOrTexSRVId  &&
               StaticSamplerFlag  == Attribs.StaticSamplerFlag;
    }

    size_t GetHash()const
    {
        return ComputeHash(BindPoint, BindCount, InputType, VariableType, SRVDimension, SamplerOrTexSRVId, StaticSamplerFlag);
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
    
    Uint32 GetNumCBs()        const noexcept{ return (m_TexSRVOffset   - 0);                }
    Uint32 GetNumTexSRV()     const noexcept{ return (m_TexUAVOffset   - m_TexSRVOffset);   }
    Uint32 GetNumTexUAV()     const noexcept{ return (m_BufSRVOffset   - m_TexUAVOffset);   }
    Uint32 GetNumBufSRV()     const noexcept{ return (m_BufUAVOffset   - m_BufSRVOffset);   }
    Uint32 GetNumBufUAV()     const noexcept{ return (m_SamplersOffset - m_BufUAVOffset);   }
    Uint32 GetNumSamplers()   const noexcept{ return (m_TotalResources - m_SamplersOffset); }
    Uint32 GetTotalResources()const noexcept{ return  m_TotalResources;                     }

    const D3DShaderResourceAttribs& GetCB     (Uint32 n)const noexcept{ return GetResAttribs(n, GetNumCBs(),                   0); }
    const D3DShaderResourceAttribs& GetTexSRV (Uint32 n)const noexcept{ return GetResAttribs(n, GetNumTexSRV(),   m_TexSRVOffset); }
    const D3DShaderResourceAttribs& GetTexUAV (Uint32 n)const noexcept{ return GetResAttribs(n, GetNumTexUAV(),   m_TexUAVOffset); }
    const D3DShaderResourceAttribs& GetBufSRV (Uint32 n)const noexcept{ return GetResAttribs(n, GetNumBufSRV(),   m_BufSRVOffset); }
    const D3DShaderResourceAttribs& GetBufUAV (Uint32 n)const noexcept{ return GetResAttribs(n, GetNumBufUAV(),   m_BufUAVOffset); }
    const D3DShaderResourceAttribs& GetSampler(Uint32 n)const noexcept{ return GetResAttribs(n, GetNumSamplers(), m_SamplersOffset); }

    D3DShaderResourceCounters CountResources(const SHADER_VARIABLE_TYPE* AllowedVarTypes,
                                             Uint32                      NumAllowedTypes)const noexcept;

    SHADER_TYPE GetShaderType()const noexcept{return m_ShaderType;}

    // Processes only resources listed in AllowedVarTypes
    template<typename THandleCB,
             typename THandleSampler,
             typename THandleTexSRV,
             typename THandleTexUAV,
             typename THandleBufSRV,
             typename THandleBufUAV>
    void ProcessResources(const SHADER_VARIABLE_TYPE* AllowedVarTypes, 
                          Uint32                      NumAllowedTypes,
                          THandleCB                   HandleCB,
                          THandleSampler              HandleSampler,
                          THandleTexSRV               HandleTexSRV,
                          THandleTexUAV               HandleTexUAV,
                          THandleBufSRV               HandleBufSRV,
                          THandleBufUAV               HandleBufUAV)const
    {
        Uint32 AllowedTypeBits = GetAllowedTypeBits(AllowedVarTypes, NumAllowedTypes);

        for(Uint32 n=0; n < GetNumCBs(); ++n)
        {
            const auto& CB = GetCB(n);
            if( CB.IsAllowedType(AllowedTypeBits) )
                HandleCB(CB, n);
        }

        for(Uint32 n=0; n < GetNumSamplers(); ++n)
        {
            const auto& Sampler = GetSampler(n);
            if( Sampler.IsAllowedType(AllowedTypeBits) )
                HandleSampler(Sampler, n);
        }

        for(Uint32 n=0; n < GetNumTexSRV(); ++n)
        {
            const auto& TexSRV = GetTexSRV(n);
            if( TexSRV.IsAllowedType(AllowedTypeBits) )
                HandleTexSRV(TexSRV, n);
        }
    
        for(Uint32 n=0; n < GetNumTexUAV(); ++n)
        {
            const auto& TexUAV = GetTexUAV(n);
            if( TexUAV.IsAllowedType(AllowedTypeBits) )
                HandleTexUAV(TexUAV, n);
        }

        for(Uint32 n=0; n < GetNumBufSRV(); ++n)
        {
            const auto& BufSRV = GetBufSRV(n);
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

    bool        IsCompatibleWith(const ShaderResources& Resources) const;
    bool        IsUsingCombinedTextureSamplers() const { return m_SamplerSuffix != nullptr; }
    const char* GetCombinedSamplerSuffix()       const { return m_SamplerSuffix; }

    size_t GetHash()const;

protected:
    template<typename D3D_SHADER_DESC, 
             typename D3D_SHADER_INPUT_BIND_DESC,
             typename TShaderReflection, 
             typename TNewResourceHandler>
    void Initialize(ID3DBlob*           pShaderByteCode, 
                    TNewResourceHandler NewResHandler, 
                    const ShaderDesc&   ShdrDesc,
                    const Char*         SamplerSuffix);


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
        return reinterpret_cast<const D3DShaderResourceAttribs*>(m_MemoryBuffer.get())[Offset + n];
    }

    D3DShaderResourceAttribs& GetCB     (Uint32 n)noexcept{ return GetResAttribs(n, GetNumCBs(),                   0);   }
    D3DShaderResourceAttribs& GetTexSRV (Uint32 n)noexcept{ return GetResAttribs(n, GetNumTexSRV(),   m_TexSRVOffset);   }
    D3DShaderResourceAttribs& GetTexUAV (Uint32 n)noexcept{ return GetResAttribs(n, GetNumTexUAV(),   m_TexUAVOffset);   }
    D3DShaderResourceAttribs& GetBufSRV (Uint32 n)noexcept{ return GetResAttribs(n, GetNumBufSRV(),   m_BufSRVOffset);   }
    D3DShaderResourceAttribs& GetBufUAV (Uint32 n)noexcept{ return GetResAttribs(n, GetNumBufUAV(),   m_BufUAVOffset);   }
    D3DShaderResourceAttribs& GetSampler(Uint32 n)noexcept{ return GetResAttribs(n, GetNumSamplers(), m_SamplersOffset); }

private:
    void AllocateMemory(IMemoryAllocator&                Allocator, 
                        const D3DShaderResourceCounters& ResCounters,
                        size_t                           ResourceNamesPoolSize);

    Uint32 FindAssignedSamplerId(const D3DShaderResourceAttribs& TexSRV, const char* SamplerSuffix)const;

    // Memory buffer that holds all resources as continuous chunk of memory:
    // | CBs | TexSRVs | TexUAVs | BufSRVs | BufUAVs | Samplers |  Resource Names  |
    std::unique_ptr< void, STDDeleterRawMem<void> > m_MemoryBuffer;

    StringPool  m_ResourceNames;
    const char* m_SamplerSuffix = nullptr; // The suffix is put into the m_ResourceNames

    // Offsets in elements of D3DShaderResourceAttribs
    typedef Uint16 OffsetType;
    OffsetType m_TexSRVOffset   = 0;
    OffsetType m_TexUAVOffset   = 0;
    OffsetType m_BufSRVOffset   = 0;
    OffsetType m_BufUAVOffset   = 0;
    OffsetType m_SamplersOffset = 0;
    OffsetType m_TotalResources = 0;

    SHADER_TYPE m_ShaderType = SHADER_TYPE_UNKNOWN;
};


template<typename D3D_SHADER_DESC, 
         typename D3D_SHADER_INPUT_BIND_DESC,
         typename TShaderReflection, 
         typename TNewResourceHandler>
void ShaderResources::Initialize(ID3DBlob*           pShaderByteCode, 
                                 TNewResourceHandler NewResHandler, 
                                 const ShaderDesc&   ShdrDesc,
                                 const Char*         CombinedSamplerSuffix)
{
    Uint32 CurrCB = 0, CurrTexSRV = 0, CurrTexUAV = 0, CurrBufSRV = 0, CurrBufUAV = 0, CurrSampler = 0;
    LoadD3DShaderResources<D3D_SHADER_DESC, D3D_SHADER_INPUT_BIND_DESC, TShaderReflection>(
        pShaderByteCode,

        [&](const D3DShaderResourceCounters& ResCounters, size_t ResourceNamesPoolSize)
        {
            if (CombinedSamplerSuffix != nullptr)
                ResourceNamesPoolSize += strlen(CombinedSamplerSuffix)+1;

            AllocateMemory(GetRawAllocator(), ResCounters, ResourceNamesPoolSize);
        },

        [&](const D3DShaderResourceAttribs& CBAttribs)
        {
            VERIFY_EXPR(CBAttribs.GetInputType() == D3D_SIT_CBUFFER);
            auto* pNewCB = new (&GetCB(CurrCB++)) D3DShaderResourceAttribs(m_ResourceNames, CBAttribs);
            NewResHandler.OnNewCB(*pNewCB);
        },

        [&](const D3DShaderResourceAttribs& TexUAV)
        {
            VERIFY_EXPR(TexUAV.GetInputType() == D3D_SIT_UAV_RWTYPED && TexUAV.GetSRVDimension() != D3D_SRV_DIMENSION_BUFFER);
            auto* pNewTexUAV = new (&GetTexUAV(CurrTexUAV++)) D3DShaderResourceAttribs(m_ResourceNames, TexUAV);
            NewResHandler.OnNewTexUAV(*pNewTexUAV);
        },

        [&](const D3DShaderResourceAttribs& BuffUAV)
        {
            VERIFY_EXPR(BuffUAV.GetInputType() == D3D_SIT_UAV_RWTYPED && BuffUAV.GetSRVDimension() == D3D_SRV_DIMENSION_BUFFER || BuffUAV.GetInputType() == D3D_SIT_UAV_RWSTRUCTURED || BuffUAV.GetInputType() == D3D_SIT_UAV_RWBYTEADDRESS);
            auto* pNewBufUAV = new (&GetBufUAV(CurrBufUAV++)) D3DShaderResourceAttribs(m_ResourceNames, BuffUAV);
            NewResHandler.OnNewBuffUAV(*pNewBufUAV);
        },

        [&](const D3DShaderResourceAttribs& BuffSRV)
        {
            VERIFY_EXPR(BuffSRV.GetInputType() == D3D_SIT_TEXTURE && BuffSRV.GetSRVDimension() == D3D_SRV_DIMENSION_BUFFER || BuffSRV.GetInputType() == D3D_SIT_STRUCTURED || BuffSRV.GetInputType() == D3D_SIT_BYTEADDRESS);
            auto* pNewBuffSRV = new (&GetBufSRV(CurrBufSRV++)) D3DShaderResourceAttribs(m_ResourceNames, BuffSRV);
            NewResHandler.OnNewBuffSRV(*pNewBuffSRV);
        },

        [&](const D3DShaderResourceAttribs& SamplerAttribs)
        {
            VERIFY_EXPR(SamplerAttribs.GetInputType() == D3D_SIT_SAMPLER);
            auto* pNewSampler = new (&GetSampler(CurrSampler++)) D3DShaderResourceAttribs(m_ResourceNames, SamplerAttribs);
            NewResHandler.OnNewSampler(*pNewSampler);
        },

        [&](const D3DShaderResourceAttribs& TexAttribs)
        {
            VERIFY_EXPR(TexAttribs.GetInputType() == D3D_SIT_TEXTURE && TexAttribs.GetSRVDimension() != D3D_SRV_DIMENSION_BUFFER);
            VERIFY(CurrSampler == GetNumSamplers(), "All samplers must be initialized before texture SRVs" );

            auto SamplerId = CombinedSamplerSuffix != nullptr ? FindAssignedSamplerId(TexAttribs, CombinedSamplerSuffix) : D3DShaderResourceAttribs::InvalidSamplerId;
            auto* pNewTexSRV = new (&GetTexSRV(CurrTexSRV)) D3DShaderResourceAttribs(m_ResourceNames, TexAttribs, SamplerId);
            if (SamplerId != D3DShaderResourceAttribs::InvalidSamplerId)
            {
                GetSampler(SamplerId).SetTexSRVId(CurrTexSRV);
            }
            ++CurrTexSRV;
            NewResHandler.OnNewTexSRV(*pNewTexSRV);
        },

        ShdrDesc,
        CombinedSamplerSuffix);

    if (CombinedSamplerSuffix != nullptr)
    {
        m_SamplerSuffix = m_ResourceNames.CopyString(CombinedSamplerSuffix);

#ifdef DEVELOPMENT
        for (Uint32 n=0; n < GetNumSamplers(); ++n)
        {
            const auto& Sampler = GetSampler(n);
            if (!Sampler.ValidTexSRVAssigned())
                LOG_ERROR_MESSAGE("Shader '", ShdrDesc.Name, "' uses combined texture samplers, but sampler '", Sampler.Name, "' is not assigned to any texture");
        }
#endif
    }

    VERIFY_EXPR(m_ResourceNames.GetRemainingSize() == 0);
    VERIFY(CurrCB      == GetNumCBs(),      "Not all CBs are initialized which will cause a crash when ~D3DShaderResourceAttribs() is called");
    VERIFY(CurrTexSRV  == GetNumTexSRV(),   "Not all Tex SRVs are initialized which will cause a crash when ~D3DShaderResourceAttribs() is called" );
    VERIFY(CurrTexUAV  == GetNumTexUAV(),   "Not all Tex UAVs are initialized which will cause a crash when ~D3DShaderResourceAttribs() is called" );
    VERIFY(CurrBufSRV  == GetNumBufSRV(),   "Not all Buf SRVs are initialized which will cause a crash when ~D3DShaderResourceAttribs() is called" );
    VERIFY(CurrBufUAV  == GetNumBufUAV(),   "Not all Buf UAVs are initialized which will cause a crash when ~D3DShaderResourceAttribs() is called" );
    VERIFY(CurrSampler == GetNumSamplers(), "Not all Samplers are initialized which will cause a crash when ~D3DShaderResourceAttribs() is called" );
}

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
