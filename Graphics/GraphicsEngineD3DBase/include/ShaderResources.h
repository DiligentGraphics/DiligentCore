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

namespace Diligent
{

static const Char* D3DSamplerSuffix = "_sampler";

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
    D3DShaderResourceAttribs(String &&_Name, 
                             UINT _BindPoint, 
                             UINT _BindCount, 
                             D3D_SHADER_INPUT_TYPE _InputType, 
                             SHADER_VARIABLE_TYPE _VariableType, 
                             D3D_SRV_DIMENSION SRVDimension,
                             Uint32 SamplerId, 
                             bool _IsStaticSampler) :
        Name(std::move(_Name)),
        BindPoint(static_cast<Uint16>(_BindPoint)),
        BindCount(static_cast<Uint16>(_BindCount)),
        PackedAttribs( PackAttribs(_InputType, _VariableType, SRVDimension, SamplerId, _IsStaticSampler)  )
    {
        VERIFY( static_cast<Uint32>(_InputType) <= ShaderInputTypeMask, "Shader input type is out of expected range");
        VERIFY( static_cast<Uint32>(_VariableType) <= VariableTypeMask, "Variable type is out of expected range");
        VERIFY( static_cast<Uint32>(SRVDimension) <= SRVDimMask, "SRV dimensions is out of expected range");
        VERIFY(SamplerId <= SamplerIdMask, "Sampler Id is out of allowed range" );
        VERIFY_EXPR(GetInputType() == _InputType);
        VERIFY_EXPR(GetVariableType() == _VariableType);
        VERIFY(_BindPoint <= MaxBindPoint || _BindPoint == InvalidBindPoint, "Bind Point is out of allowed range" );
        VERIFY(_BindCount <= MaxBindCount, "Bind Count is out of allowed range" );
#ifdef _DEBUG
        if(_InputType==D3D_SIT_SAMPLER)
        {
            VERIFY_EXPR(IsStaticSampler() == _IsStaticSampler);
        }
        else
        {
            VERIFY(!_IsStaticSampler, "Only samplers can be marked as static");
        }

        if (_InputType == D3D_SIT_TEXTURE)
        {
            VERIFY_EXPR(GetSamplerId() == SamplerId);
        }
        else
        {
            VERIFY(SamplerId == InvalidSamplerId, "Only textures can be assigned valid texture sampler");
        }

        if(_IsStaticSampler)
        {
            VERIFY( _InputType == D3D_SIT_SAMPLER, "Invalid input type: D3D_SIT_SAMPLER is expected" );
        }
#endif
    }

    D3DShaderResourceAttribs(const D3DShaderResourceAttribs& rhs) = default;

    D3DShaderResourceAttribs(const D3DShaderResourceAttribs& rhs, Uint32 SamplerId)noexcept : 
        Name(rhs.Name),
        BindPoint(rhs.BindPoint),
        BindCount(rhs.BindCount),
        PackedAttribs( PackAttribs(rhs.GetInputType(), rhs.GetVariableType(), rhs.GetSRVDimension(), SamplerId, false) )
    {
        VERIFY(GetInputType() == D3D_SIT_TEXTURE, "Only textures can be assigned a texture sampler");

        VERIFY_EXPR(GetInputType() == rhs.GetInputType());
        VERIFY_EXPR(GetVariableType() == rhs.GetVariableType());
        VERIFY_EXPR(GetSamplerId() == SamplerId);
    }

    D3DShaderResourceAttribs(D3DShaderResourceAttribs&& rhs, Uint32 SamplerId)noexcept : 
        Name(std::move(rhs.Name)),
        BindPoint(rhs.BindPoint),
        BindCount(rhs.BindCount),
        PackedAttribs( PackAttribs(rhs.GetInputType(), rhs.GetVariableType(), rhs.GetSRVDimension(), SamplerId, false) )
    {
        VERIFY(GetInputType() == D3D_SIT_TEXTURE, "Only textures can be assigned a texture sampler");

        VERIFY_EXPR(GetInputType() == rhs.GetInputType());
        VERIFY_EXPR(GetVariableType() == rhs.GetVariableType());
        VERIFY_EXPR(GetSamplerId() == SamplerId);
    }

    D3DShaderResourceAttribs(D3DShaderResourceAttribs&& rhs)noexcept : 
        Name(std::move(rhs.Name)),
        BindPoint(rhs.BindPoint),
        BindCount(rhs.BindCount),
        PackedAttribs( rhs.PackedAttribs )
    {
    }

    D3D_SHADER_INPUT_TYPE GetInputType()const
    {
        return static_cast<D3D_SHADER_INPUT_TYPE>( (PackedAttribs >> ShaderInputTypeBitOffset) & ShaderInputTypeMask );
    }
    
    SHADER_VARIABLE_TYPE GetVariableType()const
    {
        return static_cast<SHADER_VARIABLE_TYPE>( (PackedAttribs >> VariableTypeBitOffset) & VariableTypeMask );
    }

    D3D_SRV_DIMENSION GetSRVDimension()const
    {
        return static_cast<D3D_SRV_DIMENSION>( (PackedAttribs >> SRVDimBitOffset) & SRVDimMask );
    }

    Uint32 GetSamplerId()const
    {
        VERIFY( GetInputType() == D3D_SIT_TEXTURE, "Invalid input type: D3D_SIT_TEXTURE is expected" );
        return (PackedAttribs >> SamplerIdBitOffset) & SamplerIdMask;
    }

    bool IsStaticSampler()const
    {
        VERIFY( GetInputType() == D3D_SIT_SAMPLER, "Invalid input type: D3D_SIT_SAMPLER is expected" );
        return (PackedAttribs & (1 << IsStaticSamplerFlagBitOffset)) != 0;
    }

    bool IsValidSampler()const
    {
        VERIFY( GetInputType() == D3D_SIT_TEXTURE, "Invalid input type: D3D_SIT_TEXTURE is expected" );
        return GetSamplerId() != InvalidSamplerId;
    }

    bool IsValidBindPoint()const
    {
        return BindPoint != InvalidBindPoint;
    }

    static constexpr Uint16 InvalidBindPoint = std::numeric_limits<Uint16>::max();

    String Name; // Move ctor will not work if it is const
    const Uint16 BindPoint;
    const Uint16 BindCount;

    String GetPrintName(Uint32 ArrayInd)const
    {
        VERIFY_EXPR(ArrayInd < BindCount);
        if(BindCount > 1)
            return String(Name) + '[' + std::to_string(ArrayInd) + ']';
        else
            return Name;
    }
private:
    static constexpr Uint16 MaxBindPoint = InvalidBindPoint-1;
    static constexpr Uint16 MaxBindCount = std::numeric_limits<Uint16>::max();

    static constexpr Uint32 ShaderInputTypeBits = 4; // Max value: D3D_SIT_UAV_RWSTRUCTURED_WITH_COUNTER==11
    static constexpr Uint32 ShaderInputTypeMask = (1 << ShaderInputTypeBits)-1;
    static constexpr Uint32 ShaderInputTypeBitOffset = 0;
    static_assert( D3D_SIT_UAV_RWSTRUCTURED_WITH_COUNTER <= ShaderInputTypeMask, "Not enough bits to represent D3D_SHADER_INPUT_TYPE" );

    static constexpr Uint32 VariableTypeBits = 3; // Max value: SHADER_VARIABLE_TYPE_DYNAMIC == 2
    static constexpr Uint32 VariableTypeMask = (1<<VariableTypeBits)-1;
    static constexpr Uint32 VariableTypeBitOffset = ShaderInputTypeBitOffset + ShaderInputTypeBits;
    static_assert( SHADER_VARIABLE_TYPE_NUM_TYPES-1 <= VariableTypeMask, "Not enough bits to represent SHADER_VARIABLE_TYPE" );

    static constexpr Uint32 SRVDimBits = 4; // Max value: D3D_SRV_DIMENSION_BUFFEREX == 11
    static constexpr Uint32 SRVDimMask = (1<<SRVDimBits)-1;
    static constexpr Uint32 SRVDimBitOffset = VariableTypeBitOffset + VariableTypeBits;
    static_assert( D3D_SRV_DIMENSION_BUFFEREX <= SRVDimMask, "Not enough bits to represent D3D_SRV_DIMENSION" );

    static constexpr Uint32 SamplerIdBits = 32 - 1 - ShaderInputTypeBits - VariableTypeBits - SRVDimBits;
    static constexpr Uint32 SamplerIdMask = (1 << SamplerIdBits) - 1;
    static constexpr Uint32 SamplerIdBitOffset = SRVDimBitOffset + SRVDimBits;
public:
    static constexpr Uint32 InvalidSamplerId = SamplerIdMask;
private:

    static constexpr Uint32 IsStaticSamplerFlagBits = 1;
    static constexpr Uint32 IsStaticSamplerFlagMask = (1 << IsStaticSamplerFlagBits) - 1;
    static constexpr Uint32 IsStaticSamplerFlagBitOffset = SamplerIdBitOffset + SamplerIdBits;
    static_assert(IsStaticSamplerFlagBitOffset == 31, "Unexpected static sampler flag offset");

    static Uint32 PackAttribs(D3D_SHADER_INPUT_TYPE _InputType, SHADER_VARIABLE_TYPE _VariableType, D3D_SRV_DIMENSION SRVDimension, Uint32 SamplerId, bool _IsStaticSampler)
    {
        return ((static_cast<Uint32>(_InputType) & ShaderInputTypeMask) << ShaderInputTypeBitOffset) | 
               ((static_cast<Uint32>(_VariableType) & VariableTypeMask) << VariableTypeBitOffset ) |
               ((static_cast<Uint32>(SRVDimension) & SRVDimMask) << SRVDimBitOffset) |
               ((SamplerId & SamplerIdMask) << SamplerIdBitOffset) |
               ((_IsStaticSampler ? 1 : 0) << IsStaticSamplerFlagBitOffset);
    }

    //            4              3               4              20                1
    // bit | 0  1  2  3   |  4   5   6   |  7  8  9  10 | 11  12 ...  30 |        31         |   
    //     |              |              |              |                |                   |
    //     |  InputType   | VariableType |   SRV Dim    |    SamplerId   | StaticSamplerFlag |
    const Uint32 PackedAttribs;
};


/// Diligent::ShaderResources class
class ShaderResources
{
public:
    ShaderResources(IMemoryAllocator &Allocator, SHADER_TYPE ShaderType);

    // Copies specified types of resources from another ShaderResources objects
    // Only resources listed in AllowedVarTypes are copied
    ShaderResources(IMemoryAllocator &Allocator, 
                    const ShaderResources& SrcResources, 
                    const SHADER_VARIABLE_TYPE *AllowedVarTypes, 
                    Uint32 NumAllowedTypes);

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
    Uint32 GetNumSamplers()const noexcept{ return (m_BufferEndOffset- m_SamplersOffset); }

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
            if( IsAllowedType(CB.GetVariableType(), AllowedTypeBits) )
                HandleCB(CB);
        }

        for(Uint32 n=0; n < GetNumTexSRV(); ++n)
        {
            const auto &TexSRV = GetTexSRV(n);
            if( IsAllowedType(TexSRV.GetVariableType(), AllowedTypeBits) )
                HandleTexSRV(TexSRV);
        }
    
        for(Uint32 n=0; n < GetNumTexUAV(); ++n)
        {
            const auto &TexUAV = GetTexUAV(n);
            if( IsAllowedType(TexUAV.GetVariableType(), AllowedTypeBits) )
                HandleTexUAV(TexUAV);
        }

        for(Uint32 n=0; n < GetNumBufSRV(); ++n)
        {
            const auto &BufSRV = GetBufSRV(n);
            if( IsAllowedType(BufSRV.GetVariableType(), AllowedTypeBits) )
                HandleBufSRV(BufSRV);
        }

        for(Uint32 n=0; n < GetNumBufUAV(); ++n)
        {
            const auto& BufUAV = GetBufUAV(n);
            if( IsAllowedType(BufUAV.GetVariableType(), AllowedTypeBits) )
                HandleBufUAV(BufUAV);
        }
    }

protected:
    void Initialize(IMemoryAllocator &Allocator, Uint32 NumCBs, Uint32 NumTexSRVs, Uint32 NumTexUAVs, Uint32 NumBufSRVs, Uint32 NumBufUAVs, Uint32 NumSamplers);

    __forceinline D3DShaderResourceAttribs& GetResAttribs(Uint32 n, Uint32 NumResources, Uint32 Offset)noexcept
    {
        VERIFY(n < NumResources, "Resource index (", n, ") is out of range. Max allowed index: ", NumResources-1);
        VERIFY_EXPR(Offset + n < m_BufferEndOffset);
        return reinterpret_cast<D3DShaderResourceAttribs*>(m_MemoryBuffer.get())[Offset + n];
    }

    __forceinline const D3DShaderResourceAttribs& GetResAttribs(Uint32 n, Uint32 NumResources, Uint32 Offset)const noexcept
    {
        VERIFY(n < NumResources, "Resource index (", n, ") is out of range. Max allowed index: ", NumResources-1);
        VERIFY_EXPR(Offset + n < m_BufferEndOffset);
        return reinterpret_cast<D3DShaderResourceAttribs*>(m_MemoryBuffer.get())[Offset + n];
    }

    D3DShaderResourceAttribs& GetCB     (Uint32 n)noexcept{ return GetResAttribs(n, GetNumCBs(),                   0); }
    D3DShaderResourceAttribs& GetTexSRV (Uint32 n)noexcept{ return GetResAttribs(n, GetNumTexSRV(),   m_TexSRVOffset); }
    D3DShaderResourceAttribs& GetTexUAV (Uint32 n)noexcept{ return GetResAttribs(n, GetNumTexUAV(),   m_TexUAVOffset); }
    D3DShaderResourceAttribs& GetBufSRV (Uint32 n)noexcept{ return GetResAttribs(n, GetNumBufSRV(),   m_BufSRVOffset); }
    D3DShaderResourceAttribs& GetBufUAV (Uint32 n)noexcept{ return GetResAttribs(n, GetNumBufUAV(),   m_BufUAVOffset); }
    D3DShaderResourceAttribs& GetSampler(Uint32 n)noexcept{ return GetResAttribs(n, GetNumSamplers(), m_SamplersOffset); }

    Uint32 FindAssignedSamplerId(const D3DShaderResourceAttribs& TexSRV)const;

private:
    // Memory buffer that holds all resources as continuous chunk of memory:
    // | CBs | TexSRVs | TexUAVs | BufSRVs | BufUAVs | Samplers |
    std::unique_ptr< void, STDDeleterRawMem<void> > m_MemoryBuffer;
        
    // Offsets in elements of D3DShaderResourceAttribs
    typedef Uint16 OffsetType;
    OffsetType m_TexSRVOffset = 0;
    OffsetType m_TexUAVOffset = 0;
    OffsetType m_BufSRVOffset = 0;
    OffsetType m_BufUAVOffset = 0;
    OffsetType m_SamplersOffset = 0;
    OffsetType m_BufferEndOffset = 0;

    SHADER_TYPE m_ShaderType = SHADER_TYPE_UNKNOWN;
};

}
