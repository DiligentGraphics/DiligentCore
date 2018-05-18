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

#include <iomanip>
#include "SPIRVShaderResources.h"
#include "spirv_cross.hpp"
#include "ShaderBase.h"
#include "GraphicsAccessories.h"

namespace Diligent
{

template<typename Type>
Type GetResourceArraySize(const spirv_cross::Compiler& Compiler,
                          const spirv_cross::Resource& Res)
{
    const auto& type = Compiler.get_type(Res.type_id);
    uint32_t arrSize = 1;
    if(!type.array.empty())
    {
        // https://github.com/KhronosGroup/SPIRV-Cross/wiki/Reflection-API-user-guide#querying-array-types
        VERIFY(type.array.size() == 1, "Only one-dimensional arrays are currently supported");
        arrSize = type.array[0];
    }
    VERIFY(arrSize <= std::numeric_limits<Type>::max(), "Array size exceeds maximum representable value ", std::numeric_limits<Type>::max());
    return static_cast<Type>(arrSize);
}

static uint32_t GetDecorationOffset(const spirv_cross::Compiler& Compiler,
                                    const spirv_cross::Resource& Res,
                                    spv::Decoration Decoration)
{
    VERIFY(Compiler.has_decoration(Res.id, Decoration), "Res \'", Res.name, "\' has no requested decoration");
    uint32_t offset = 0;
    auto declared = Compiler.get_binary_offset_for_decoration(Res.id, Decoration, offset);
    VERIFY(declared, "Requested decoration is not declared");
    return offset;
}

SPIRVShaderResourceAttribs::SPIRVShaderResourceAttribs(const spirv_cross::Compiler&  Compiler,
                                                       const spirv_cross::Resource&  Res, 
                                                       const char*                   _Name,
                                                       ResourceType                  _Type, 
                                                       SHADER_VARIABLE_TYPE          _VarType,
                                                       Int32                         _StaticSamplerInd)noexcept :
    Name(_Name),
    ArraySize(GetResourceArraySize<decltype(ArraySize)>(Compiler, Res)),
    Type(_Type),
    VarType(_VarType),
    StaticSamplerInd(static_cast<decltype(StaticSamplerInd)>(_StaticSamplerInd)),
    BindingDecorationOffset(GetDecorationOffset(Compiler, Res, spv::Decoration::DecorationBinding)),
    DescriptorSetDecorationOffset(GetDecorationOffset(Compiler, Res, spv::Decoration::DecorationDescriptorSet))
{
    VERIFY(_StaticSamplerInd >= std::numeric_limits<decltype(StaticSamplerInd)>::min() && 
           _StaticSamplerInd <= std::numeric_limits<decltype(StaticSamplerInd)>::max(), "Static sampler index is out of representable range" );
}

static Int32 FindStaticSampler(const ShaderDesc& shaderDesc, const std::string& SamplerName)
{
    for(Uint32 s=0; s < shaderDesc.NumStaticSamplers; ++s)
    {
        const auto& StSam = shaderDesc.StaticSamplers[s];
        if(SamplerName.compare(StSam.TextureName) == 0)
            return s;
    }

    return -1;
}

SPIRVShaderResources::SPIRVShaderResources(IMemoryAllocator&         Allocator, 
                                           IRenderDevice*            pRenderDevice,
                                           std::vector<uint32_t>     spirv_binary,
                                           const ShaderDesc&         shaderDesc) :
    m_MemoryBuffer(nullptr, STDDeleterRawMem<void>(Allocator)),
    m_ShaderType(shaderDesc.ShaderType)
{
    // https://github.com/KhronosGroup/SPIRV-Cross/wiki/Reflection-API-user-guide
    spirv_cross::Compiler Compiler(std::move(spirv_binary));

    // The SPIR-V is now parsed, and we can perform reflection on it.
    spirv_cross::ShaderResources resources = Compiler.get_shader_resources();

    size_t ResourceNamesPoolSize = 0;
    for(auto *pResType : 
        {
            &resources.uniform_buffers,
            &resources.storage_buffers,
            &resources.storage_images,
            &resources.sampled_images,
            &resources.atomic_counters,
            &resources.push_constant_buffers,
            &resources.separate_images,
            &resources.separate_samplers
        })
    {
        for(const auto &res : *pResType)
            ResourceNamesPoolSize += res.name.length() + 1;
    }

    Initialize(Allocator, 
               static_cast<Uint32>(resources.uniform_buffers.size()),
               static_cast<Uint32>(resources.storage_buffers.size()),
               static_cast<Uint32>(resources.storage_images.size()),
               static_cast<Uint32>(resources.sampled_images.size()),
               static_cast<Uint32>(resources.atomic_counters.size()),
               static_cast<Uint32>(resources.separate_images.size()),
               static_cast<Uint32>(resources.separate_samplers.size()),
               shaderDesc.NumStaticSamplers,
               ResourceNamesPoolSize);

    {
        Uint32 CurrUB = 0;
        for (const auto &UB : resources.uniform_buffers)
        {
            new (&GetUB(CurrUB++)) 
                SPIRVShaderResourceAttribs(Compiler, 
                                           UB, 
                                           m_ResourceNames.CopyString(UB.name), 
                                           SPIRVShaderResourceAttribs::ResourceType::UniformBuffer, 
                                           GetShaderVariableType(UB.name, shaderDesc), 
                                           -1);
        }
        VERIFY_EXPR(CurrUB == GetNumUBs());
    }

    {
        Uint32 CurrSB = 0;
        for (const auto &SB : resources.storage_buffers)
        {
            new (&GetSB(CurrSB++))
                SPIRVShaderResourceAttribs(Compiler, 
                                           SB, 
                                           m_ResourceNames.CopyString(SB.name),
                                           SPIRVShaderResourceAttribs::ResourceType::StorageBuffer,
                                           GetShaderVariableType(SB.name, shaderDesc), 
                                           -1);
        }
        VERIFY_EXPR(CurrSB == GetNumSBs());
    }

    {
        Uint32 CurrSmplImg = 0;
        for (const auto &SmplImg : resources.sampled_images)
        {
            auto StaticSamplerInd = FindStaticSampler(shaderDesc, SmplImg.name);
            const auto& type = Compiler.get_type(SmplImg.type_id);
            auto ResType = type.image.dim == spv::DimBuffer ?
                SPIRVShaderResourceAttribs::ResourceType::UniformTexelBuffer :
                SPIRVShaderResourceAttribs::ResourceType::SampledImage;
            new (&GetSmplImg(CurrSmplImg++)) 
                SPIRVShaderResourceAttribs(Compiler, 
                                           SmplImg, 
                                           m_ResourceNames.CopyString(SmplImg.name), 
                                           ResType, 
                                           GetShaderVariableType(SmplImg.name, shaderDesc), 
                                           StaticSamplerInd);
        }
        VERIFY_EXPR(CurrSmplImg == GetNumSmplImgs()); 
    }

    {
        Uint32 CurrImg = 0;
        for (const auto &Img : resources.storage_images)
        {
            const auto& type = Compiler.get_type(Img.type_id);
            auto ResType = type.image.dim == spv::DimBuffer ?
                SPIRVShaderResourceAttribs::ResourceType::StorageTexelBuffer :
                SPIRVShaderResourceAttribs::ResourceType::StorageImage;
            new (&GetImg(CurrImg++)) 
                SPIRVShaderResourceAttribs(Compiler, 
                                           Img, 
                                           m_ResourceNames.CopyString(Img.name), 
                                           ResType, 
                                           GetShaderVariableType(Img.name, shaderDesc), 
                                           -1);
        }
        VERIFY_EXPR(CurrImg == GetNumImgs());
    }

    {
        Uint32 CurrAC = 0;
        for (const auto &AC : resources.atomic_counters)
        {
            new (&GetAC(CurrAC++))
                SPIRVShaderResourceAttribs(Compiler, 
                                           AC, 
                                           m_ResourceNames.CopyString(AC.name),
                                           SPIRVShaderResourceAttribs::ResourceType::AtomicCounter,
                                           GetShaderVariableType(AC.name, shaderDesc), 
                                           -1);
        }
        VERIFY_EXPR(CurrAC == GetNumACs());
    }

    {
        Uint32 CurrSepImg = 0;
        for (const auto &SepImg : resources.separate_images)
        {
            new (&GetSepImg(CurrSepImg++))
                SPIRVShaderResourceAttribs(Compiler, 
                                           SepImg, 
                                           m_ResourceNames.CopyString(SepImg.name),
                                           SPIRVShaderResourceAttribs::ResourceType::SeparateImage,
                                           GetShaderVariableType(SepImg.name, shaderDesc), 
                                           -1);
        }
        VERIFY_EXPR(CurrSepImg == GetNumSepImgs());
    }

    {
        Uint32 CurrSepSmpl = 0;
        for (const auto &SepSam : resources.separate_samplers)
        {
            auto StaticSamplerInd = FindStaticSampler(shaderDesc, SepSam.name);
            new (&GetSepSmpl(CurrSepSmpl++))
                SPIRVShaderResourceAttribs(Compiler, 
                                           SepSam, 
                                           m_ResourceNames.CopyString(SepSam.name),
                                           SPIRVShaderResourceAttribs::ResourceType::SeparateSampler, 
                                           GetShaderVariableType(SepSam.name, shaderDesc), 
                                           StaticSamplerInd);
        }
        VERIFY_EXPR(CurrSepSmpl == GetNumSepSmpls());
    }

    VERIFY(m_ResourceNames.GetRemainingSize() == 0, "Names pool must be empty");

    for (Uint32 s = 0; s < m_NumStaticSamplers; ++s)
    {
        SamplerPtrType &pStaticSampler = GetStaticSampler(s);
        new (std::addressof(pStaticSampler)) SamplerPtrType();
        pRenderDevice->CreateSampler(shaderDesc.StaticSamplers[s].Desc, &pStaticSampler);
    }

    //LOG_INFO_MESSAGE(DumpResources());

#ifdef _DEBUG
    if (shaderDesc.NumVariables != 0)
    {
        for (Uint32 v = 0; v < shaderDesc.NumVariables; ++v)
        {
            bool VariableFound = false;
            const auto *VarName = shaderDesc.VariableDesc[v].Name;
            auto VarType = shaderDesc.VariableDesc[v].Type;

            for (Uint32 res = 0; res < GetTotalResources(); ++res)
            {
                const auto &ResAttribs = GetResource(res);
                if (strcmp(ResAttribs.Name, VarName) == 0)
                {
                    VariableFound = true;
                    break;
                }
            }
            if (!VariableFound)
            {
                LOG_WARNING_MESSAGE("Variable '", VarName, "' labeled as ", GetShaderVariableTypeLiteralName(VarType), " not found in shader \'", shaderDesc.Name, "'");
            }
        }
    }

    if (shaderDesc.NumStaticSamplers != 0)
    {
        for (Uint32 s = 0; s < shaderDesc.NumStaticSamplers; ++s)
        {
            bool SamplerFound = false;
            const auto *SamName = shaderDesc.StaticSamplers[s].TextureName;
            for (Uint32 i = 0; i < GetNumSmplImgs(); ++i)
            {
                const auto &SmplImg = GetSmplImg(i);
                if (strcmp(SmplImg.Name, SamName) == 0)
                {
                    SamplerFound = true;
                    break;
                }
            }

            if(SamplerFound)
                continue;

            for (Uint32 i = 0; i < GetNumSepSmpls(); ++i)
            {
                const auto &SepSmpl = GetSepSmpl(i);
                if (strcmp(SepSmpl.Name, SamName) == 0)
                {
                    SamplerFound = true;
                    break;
                }
            }

            if (!SamplerFound)
            {
                LOG_WARNING_MESSAGE("Static sampler '", SamName, "' not found in shader \'", shaderDesc.Name, "'");
            }
        }
    }
#endif
}

void SPIRVShaderResources::Initialize(IMemoryAllocator& Allocator, 
                                      Uint32            NumUBs, 
                                      Uint32            NumSBs, 
                                      Uint32            NumImgs, 
                                      Uint32            NumSmplImgs, 
                                      Uint32            NumACs,
                                      Uint32            NumSepImgs, 
                                      Uint32            NumSepSmpls, 
                                      Uint32            NumStaticSamplers,
                                      size_t            ResourceNamesPoolSize)
{
    VERIFY(&m_MemoryBuffer.get_deleter().m_Allocator == &Allocator, "Incosistent allocators provided");

    static constexpr OffsetType UniformBufferOffset = 0;

    const auto MaxOffset = static_cast<Uint32>(std::numeric_limits<OffsetType>::max());
    VERIFY(UniformBufferOffset + NumUBs <= MaxOffset, "Max offset exceeded");
    m_StorageBufferOffset = UniformBufferOffset + static_cast<OffsetType>(NumUBs);

    VERIFY(m_StorageBufferOffset + NumSBs <= MaxOffset, "Max offset exceeded");
    m_StorageImageOffset = m_StorageBufferOffset + static_cast<OffsetType>(NumSBs);

    VERIFY(m_StorageImageOffset + NumImgs <= MaxOffset, "Max offset exceeded");
    m_SampledImageOffset = m_StorageImageOffset + static_cast<OffsetType>(NumImgs);

    VERIFY(m_SampledImageOffset + NumSmplImgs <= MaxOffset, "Max offset exceeded");
    m_AtomicCounterOffset = m_SampledImageOffset + static_cast<OffsetType>(NumSmplImgs);

    VERIFY(m_AtomicCounterOffset + NumACs <= MaxOffset, "Max offset exceeded");
    m_SeparateImageOffset = m_AtomicCounterOffset + static_cast<OffsetType>(NumACs);

    VERIFY(m_SeparateImageOffset + NumSepImgs <= MaxOffset, "Max offset exceeded");
    m_SeparateSamplerOffset = m_SeparateImageOffset + static_cast<OffsetType>(NumSepImgs);

    VERIFY(m_SeparateSamplerOffset + NumSepSmpls <= MaxOffset, "Max offset exceeded");
    m_TotalResources = m_SeparateSamplerOffset + static_cast<OffsetType>(NumSepSmpls);

    VERIFY(m_NumStaticSamplers <= MaxOffset, "Max offset exceeded");
    m_NumStaticSamplers = static_cast<OffsetType>(NumStaticSamplers);

    static_assert(sizeof(SPIRVShaderResourceAttribs) % sizeof(void*) == 0, "Size of SPIRVShaderResourceAttribs struct must be multiple of sizeof(void*)");
    static_assert(sizeof(SamplerPtrType) % sizeof(void*) == 0, "Size of SamplerPtrType must be multiple of sizeof(void*)");
    auto MemorySize = m_TotalResources * sizeof(SPIRVShaderResourceAttribs) + 
                      m_NumStaticSamplers * sizeof(SamplerPtrType) +
                      ResourceNamesPoolSize * sizeof(char);

    VERIFY_EXPR(GetNumUBs()      == NumUBs);
    VERIFY_EXPR(GetNumSBs()      == NumSBs);
    VERIFY_EXPR(GetNumImgs()     == NumImgs);
    VERIFY_EXPR(GetNumSmplImgs() == NumSmplImgs);
    VERIFY_EXPR(GetNumACs()      == NumACs);
    VERIFY_EXPR(GetNumSepImgs()  == NumSepImgs);
    VERIFY_EXPR(GetNumSepSmpls() == NumSepSmpls);

    if (MemorySize)
    {
        auto *pRawMem = Allocator.Allocate(MemorySize, "Memory for shader resources", __FILE__, __LINE__);
        m_MemoryBuffer.reset(pRawMem);
        char* NamesPool = reinterpret_cast<char*>(m_MemoryBuffer.get()) + 
                          m_TotalResources * sizeof(SPIRVShaderResourceAttribs) +
                          m_NumStaticSamplers * sizeof(SamplerPtrType);
        m_ResourceNames.AssignMemory(NamesPool, ResourceNamesPoolSize);
    }
}

SPIRVShaderResources::~SPIRVShaderResources()
{
    for (Uint32 n = 0; n < GetNumUBs(); ++n)
        GetUB(n).~SPIRVShaderResourceAttribs();

    for (Uint32 n = 0; n < GetNumSBs(); ++n)
        GetSB(n).~SPIRVShaderResourceAttribs();

    for (Uint32 n = 0; n < GetNumImgs(); ++n)
        GetImg(n).~SPIRVShaderResourceAttribs();

    for (Uint32 n = 0; n < GetNumSmplImgs(); ++n)
        GetSmplImg(n).~SPIRVShaderResourceAttribs();

    for (Uint32 n = 0; n < GetNumACs(); ++n)
        GetAC(n).~SPIRVShaderResourceAttribs();

    for (Uint32 n = 0; n < GetNumSepImgs(); ++n)
        GetSepImg(n).~SPIRVShaderResourceAttribs();

    for (Uint32 n = 0; n < GetNumSepSmpls(); ++n)
        GetSepSmpl(n).~SPIRVShaderResourceAttribs();

    for (Uint32 n = 0; n < GetNumStaticSamplers(); ++n)
        GetStaticSampler(n).~SamplerPtrType();
}

void SPIRVShaderResources::CountResources(const SHADER_VARIABLE_TYPE *AllowedVarTypes,
                                          Uint32 NumAllowedTypes, 
                                          Uint32& NumUBs, 
                                          Uint32& NumSBs, 
                                          Uint32& NumImgs, 
                                          Uint32& NumSmplImgs, 
                                          Uint32& NumACs, 
                                          Uint32& NumSepImgs,
                                          Uint32& NumSepSmpls)const noexcept
{
    Uint32 AllowedTypeBits = GetAllowedTypeBits(AllowedVarTypes, NumAllowedTypes);
    NumUBs      = 0;
    NumSBs      = 0;
    NumImgs     = 0;
    NumSmplImgs = 0;
    NumACs      = 0;
    NumSepImgs  = 0;
    NumSepSmpls = 0;

    ProcessResources(
        AllowedVarTypes, NumAllowedTypes,

        [&](const SPIRVShaderResourceAttribs &UB, Uint32)
        {
            VERIFY_EXPR(IsAllowedType(UB.VarType, AllowedTypeBits));
            ++NumUBs;
        },
        [&](const SPIRVShaderResourceAttribs& SB, Uint32)
        {
            VERIFY_EXPR(IsAllowedType(SB.VarType, AllowedTypeBits));
            ++NumSBs;
        },
        [&](const SPIRVShaderResourceAttribs &Img, Uint32)
        {
            VERIFY_EXPR(IsAllowedType(Img.VarType, AllowedTypeBits));
            ++NumImgs;
        },
        [&](const SPIRVShaderResourceAttribs &SmplImg, Uint32)
        {
            VERIFY_EXPR(IsAllowedType(SmplImg.VarType, AllowedTypeBits));
            ++NumSmplImgs;
        },
        [&](const SPIRVShaderResourceAttribs &AC, Uint32)
        {
            VERIFY_EXPR(IsAllowedType(AC.VarType, AllowedTypeBits));
            ++NumACs;
        },
        [&](const SPIRVShaderResourceAttribs &SepImg, Uint32)
        {
            VERIFY_EXPR(IsAllowedType(SepImg.VarType, AllowedTypeBits));
            ++NumSepImgs;
        },
        [&](const SPIRVShaderResourceAttribs &SepSmpl, Uint32)
        {
            VERIFY_EXPR(IsAllowedType(SepSmpl.VarType, AllowedTypeBits));
            ++NumSepSmpls;
        }
    );
}

std::string SPIRVShaderResources::DumpResources()
{
    std::stringstream ss;
    ss << "Resource counters (" << GetTotalResources() << " total):" << std::endl << "UBs: " << GetNumUBs() << "; SBs: " 
        << GetNumSBs() << "; Imgs: " << GetNumImgs() << "; Smpl Imgs: " << GetNumSmplImgs() << "; ACs: " << GetNumACs() 
        << "; Sep Imgs: " << GetNumSepImgs() << "; Sep Smpls: " << GetNumSepSmpls() << '.' << std::endl
        << "Num Static Samplers: " << GetNumStaticSamplers() << std::endl << "Resources:";
    
    Uint32 ResNum = 0;
    auto DumpResource = [&ss, &ResNum](const SPIRVShaderResourceAttribs& Res)
    {
        std::stringstream FullResNameSS;
        FullResNameSS << '\'' << Res.Name;
        if (Res.ArraySize > 1)
            FullResNameSS << '[' << Res.ArraySize << ']';
        FullResNameSS << '\'';
        ss << std::setw(32) << FullResNameSS.str();
        ss << " (" << GetShaderVariableTypeLiteralName(Res.VarType) << ")";

        if (Res.StaticSamplerInd >= 0)
        {
            ss << " Static sampler: " << Int32{ Res.StaticSamplerInd };
        }
        ++ResNum;
    };

    ProcessResources(nullptr, 0,
        [&](const SPIRVShaderResourceAttribs &UB, Uint32)
        {
            VERIFY(UB.Type == SPIRVShaderResourceAttribs::ResourceType::UniformBuffer, "Unexpected resource type");
            ss << std::endl << std::setw(3) << ResNum << " Uniform Buffer  ";
            DumpResource(UB);
        },
        [&](const SPIRVShaderResourceAttribs& SB, Uint32)
        {
            VERIFY(SB.Type == SPIRVShaderResourceAttribs::ResourceType::StorageBuffer, "Unexpected resource type");
            ss << std::endl << std::setw(3) << ResNum << " Storage Buffer  ";
            DumpResource(SB);
        },
        [&](const SPIRVShaderResourceAttribs &Img, Uint32)
        {
            if(Img.Type == SPIRVShaderResourceAttribs::ResourceType::StorageImage)
                ss << std::endl << std::setw(3) << ResNum << " Storage Image   ";
            else if(Img.Type == SPIRVShaderResourceAttribs::ResourceType::StorageTexelBuffer)
                ss << std::endl << std::setw(3) << ResNum << " Storage Txl Buff";
            else
                UNEXPECTED("Unexpected resource type");
            DumpResource(Img);
        },
        [&](const SPIRVShaderResourceAttribs &SmplImg, Uint32)
        {
            if (SmplImg.Type == SPIRVShaderResourceAttribs::ResourceType::SampledImage)
                ss << std::endl << std::setw(3) << ResNum << " Sampled Image   ";
            else if (SmplImg.Type == SPIRVShaderResourceAttribs::ResourceType::UniformTexelBuffer)
                ss << std::endl << std::setw(3) << ResNum << " Uniform Txl Buff";
            else
                UNEXPECTED("Unexpected resource type");
            DumpResource(SmplImg);
        },
        [&](const SPIRVShaderResourceAttribs &AC, Uint32)
        {
            VERIFY(AC.Type == SPIRVShaderResourceAttribs::ResourceType::AtomicCounter, "Unexpected resource type");
            ss << std::endl << std::setw(3) << ResNum << " Atomic Cntr     ";
            DumpResource(AC);
        },
        [&](const SPIRVShaderResourceAttribs &SepImg, Uint32)
        {
            VERIFY(SepImg.Type == SPIRVShaderResourceAttribs::ResourceType::SeparateImage, "Unexpected resource type");
            ss << std::endl << std::setw(3) << ResNum << " Separate Img    ";
            DumpResource(SepImg);
        },
        [&](const SPIRVShaderResourceAttribs &SepSmpl, Uint32)
        {
            VERIFY(SepSmpl.Type == SPIRVShaderResourceAttribs::ResourceType::SeparateSampler, "Unexpected resource type");
            ss << std::endl << std::setw(3) << ResNum << " Separate Smpl   ";
            DumpResource(SepSmpl);
        }
    );
    VERIFY_EXPR(ResNum == GetTotalResources());
    
    return ss.str();
}

}
