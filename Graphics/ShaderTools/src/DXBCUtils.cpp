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

#include <d3d11shader.h>
#include "DXBCUtils.hpp"
#include "DXBCChecksum.h"

namespace Diligent
{
namespace
{
struct DXBCHeader
{
    Uint32 Magic;       // 0..3 "DXBC"
    Uint32 Checksum[4]; // 4..19
    Uint32 Reserved;    // 20..23
    Uint32 TotalSize;   // 24..27
    Uint32 ChunkCount;  // 28..31
};
static_assert(sizeof(DXBCHeader) == 32, "");

struct ChunkHeader
{
    Uint32 Magic;  // 0..3 fourCC
    Uint32 Length; // 4..7
};

struct ResourceDefChunkHeader : ChunkHeader
{
    Uint32 CBuffCount;          // 8..11
    Uint32 CBuffOffset;         // 12..15, from start of chunk data
    Uint32 ResBindingCount;     // 16..19
    Uint32 ResBindingOffset;    // 20..23, from start of chunk data
    Uint8  MinorVersion;        // 24
    Uint8  MajorVersion;        // 25
    Uint16 ShaderType;          // 26..27
    Uint32 Flags;               // 28..31
    Uint32 CreatorStringOffset; // 32..35, from start of chunk data
};
static_assert(sizeof(ResourceDefChunkHeader) == 36, "");

struct ResourceBindingInfo11
{
    Uint32                   NameOffset;       // 0..3, from start of chunk data
    D3D_SHADER_INPUT_TYPE    ShaderInputType;  // 4..7
    D3D_RESOURCE_RETURN_TYPE ReturnType;       // 8..11
    D3D_SRV_DIMENSION        ViewDim;          // 12..15
    Uint32                   NumSamples;       // 16..19
    Uint32                   BindPoint;        // 20..23
    Uint32                   BindCount;        // 24..27
    D3D_SHADER_INPUT_FLAGS   ShaderInputFlags; // 28..31
};
static_assert(sizeof(ResourceBindingInfo11) == 32, "");

struct ResourceBindingInfo12
{
    Uint32                   NameOffset;       // 0..3, from start of chunk data
    D3D_SHADER_INPUT_TYPE    ShaderInputType;  // 4..7
    D3D_RESOURCE_RETURN_TYPE ReturnType;       // 8..11
    D3D_SRV_DIMENSION        ViewDim;          // 12..15
    Uint32                   NumSamples;       // 16..19
    Uint32                   BindPoint;        // 20..23
    Uint32                   BindCount;        // 24..27
    D3D_SHADER_INPUT_FLAGS   ShaderInputFlags; // 28..31
    Uint32                   Space;            // 32..35
    Uint32                   Reserved;         // 36..39
};
static_assert(sizeof(ResourceBindingInfo12) == 40, "");

#define FOURCC(a, b, c, d) (Uint32{(d) << 24} | Uint32{(c) << 16} | Uint32{(b) << 8} | Uint32{a})

constexpr Uint32 DXBCFourCC = FOURCC('D', 'X', 'B', 'C');
constexpr Uint32 RDEFFourCC = FOURCC('R', 'D', 'E', 'F');


bool RemapShaderResources(const DXBCUtils::TResourceBindingMap& ResourceMap, const void* EndPtr, ResourceDefChunkHeader* RDEFHeader)
{
    VERIFY_EXPR(RDEFHeader->Magic == RDEFFourCC);
    VERIFY_EXPR((RDEFHeader->MajorVersion == 5 && RDEFHeader->MinorVersion == 0) || RDEFHeader->MajorVersion < 5);

    auto* Ptr        = reinterpret_cast<char*>(RDEFHeader) + sizeof(ChunkHeader);
    auto* ResBinding = reinterpret_cast<ResourceBindingInfo11*>(Ptr + RDEFHeader->ResBindingOffset);
    VERIFY((ResBinding + RDEFHeader->ResBindingCount) <= EndPtr, "Resource bindings is outside of buffer range.");

    for (Uint32 r = 0; r < RDEFHeader->ResBindingCount; ++r)
    {
        auto&       Res  = ResBinding[r];
        const char* Name = Ptr + Res.NameOffset;
        VERIFY(Name < EndPtr, "Resource name pointer is outside of buffer range.");

        auto Iter = ResourceMap.find(HashMapStringKey{Name});
        if (Iter == ResourceMap.end())
        {
            LOG_ERROR("Failed to find '", Name, "' in ResourceMap.");
            return false;
        }

        if (Iter->second.Space != 0 && Iter->second.Space != ~0u)
        {
            LOG_ERROR("Can not change space for resource '", Name, "' because shader is not compiled for SM 5.1");
            return false;
        }

        Res.BindPoint = Iter->second.BindPoint;
    }
    return true;
}

bool RemapShaderResourcesSM51(const DXBCUtils::TResourceBindingMap& ResourceMap, const void* EndPtr, ResourceDefChunkHeader* RDEFHeader)
{
    VERIFY_EXPR(RDEFHeader->Magic == RDEFFourCC);
    VERIFY_EXPR(RDEFHeader->MajorVersion == 5 && RDEFHeader->MinorVersion == 1);

    auto* Ptr        = reinterpret_cast<char*>(RDEFHeader) + sizeof(ChunkHeader);
    auto* ResBinding = reinterpret_cast<ResourceBindingInfo12*>(Ptr + RDEFHeader->ResBindingOffset);
    VERIFY((ResBinding + RDEFHeader->ResBindingCount) <= EndPtr, "Resource bindings is outside of buffer range.");

    for (Uint32 r = 0; r < RDEFHeader->ResBindingCount; ++r)
    {
        auto&       Res  = ResBinding[r];
        const char* Name = Ptr + Res.NameOffset;
        VERIFY(Name < EndPtr, "Resource name pointer is outside of buffer range.");

        auto Iter = ResourceMap.find(HashMapStringKey{Name});
        if (Iter == ResourceMap.end())
        {
            LOG_ERROR("Failed to find '", Name, "' in ResourceMap.");
            return false;
        }

        Res.BindPoint = Iter->second.BindPoint;
        Res.Space     = Iter->second.Space;
    }
    return true;
}
} // namespace


bool DXBCUtils::RemapDXBCResources(const TResourceBindingMap& ResourceMap,
                                   ID3DBlob*                  pBytecode)
{
    if (pBytecode == nullptr)
    {
        LOG_ERROR("pBytecode must not be null.");
        return false;
    }

    char* const       Ptr    = static_cast<char*>(pBytecode->GetBufferPointer());
    const auto        Size   = pBytecode->GetBufferSize();
    const void* const EndPtr = Ptr + Size;

    if (Size < sizeof(DXBCHeader))
    {
        LOG_ERROR("Size of bytecode is too small.");
        return false;
    }

    auto& Header = *reinterpret_cast<DXBCHeader*>(Ptr);
    VERIFY_EXPR(Header.TotalSize == Size);

#ifdef DILIGENT_DEBUG
    {
        DWORD Checksum[4] = {};
        CalculateDXBCChecksum(reinterpret_cast<BYTE*>(Ptr), static_cast<DWORD>(Size), Checksum);

        VERIFY_EXPR(Checksum[0] == Header.Checksum[0]);
        VERIFY_EXPR(Checksum[1] == Header.Checksum[1]);
        VERIFY_EXPR(Checksum[2] == Header.Checksum[2]);
        VERIFY_EXPR(Checksum[3] == Header.Checksum[3]);
    }
#endif

    if (Header.Magic != DXBCFourCC)
    {
        LOG_ERROR("Bytecode header must contain 'DXBC' magic number.");
        return false;
    }

    const Uint32* Chunks = reinterpret_cast<Uint32*>(Ptr + sizeof(Header));
    bool          Result = true;

    for (Uint32 i = 0; i < Header.ChunkCount; ++i)
    {
        auto& Chunk = *reinterpret_cast<ChunkHeader*>(Ptr + Chunks[i]);
        VERIFY((&Chunk + 1) <= EndPtr, "Pointer to chunk is outside of buffer range.");

        if (Chunk.Magic == RDEFFourCC)
        {
            auto* RDEFHeader = reinterpret_cast<ResourceDefChunkHeader*>(&Chunk);

            if (RDEFHeader->MajorVersion == 5 && RDEFHeader->MinorVersion == 1)
                Result = RemapShaderResourcesSM51(ResourceMap, EndPtr, RDEFHeader);
            else
                Result = RemapShaderResources(ResourceMap, EndPtr, RDEFHeader);
            break;
        }
    }

    if (!Result)
    {
        LOG_ERROR("Failed to find chunk 'RDEF' with resource definition.");
        return false;
    }

    // update checksum
    DWORD Checksum[4] = {};
    CalculateDXBCChecksum(reinterpret_cast<BYTE*>(Ptr), static_cast<DWORD>(Size), Checksum);

    static_assert(sizeof(Header.Checksum) == sizeof(Checksum), "");
    memcpy(Header.Checksum, Checksum, sizeof(Header.Checksum));

    return true;
}

} // namespace Diligent
