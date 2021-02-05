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
#include "../../../ThirdParty/GPUOpenShaderUtils/DXBCChecksum.h"

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
static_assert(sizeof(DXBCHeader) == 32, "The size of DXBC header must be 32 bytes");

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
static_assert(sizeof(ResourceDefChunkHeader) == 36, "The size of resource definition chunk header must be 36 bytes");

struct ResourceBindingInfo50
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
static_assert(sizeof(ResourceBindingInfo50) == 32, "The size of SM50 resource binding info struct must be 32 bytes");

struct ResourceBindingInfo51
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
static_assert(sizeof(ResourceBindingInfo51) == 40, "The size of SM51 resource binding info struct must be 40 bytes");

#define FOURCC(a, b, c, d) (Uint32{(d) << 24} | Uint32{(c) << 16} | Uint32{(b) << 8} | Uint32{a})

constexpr Uint32 DXBCFourCC = FOURCC('D', 'X', 'B', 'C');
constexpr Uint32 RDEFFourCC = FOURCC('R', 'D', 'E', 'F');


inline bool PatchSpace(ResourceBindingInfo51& Res, Uint32 Space)
{
    Res.Space = Space;
    return true;
}

inline bool PatchSpace(ResourceBindingInfo50&, Uint32 Space)
{
    return Space == 0 || Space == ~0U;
}

template <typename ResourceBindingInfoType>
bool RemapShaderResources(const DXBCUtils::TResourceBindingMap& ResourceMap, const void* EndPtr, ResourceDefChunkHeader* RDEFHeader)
{
    VERIFY_EXPR(RDEFHeader->Magic == RDEFFourCC);

    auto* Ptr        = reinterpret_cast<char*>(RDEFHeader) + sizeof(ChunkHeader);
    auto* ResBinding = reinterpret_cast<ResourceBindingInfoType*>(Ptr + RDEFHeader->ResBindingOffset);
    if (ResBinding + RDEFHeader->ResBindingCount > EndPtr)
    {
        LOG_ERROR_MESSAGE("Resource binding data is outside of the specified byte code range. The byte code may be corrupted.");
        return false;
    }

    for (Uint32 r = 0; r < RDEFHeader->ResBindingCount; ++r)
    {
        auto&       Res  = ResBinding[r];
        const char* Name = Ptr + Res.NameOffset;
        if (Name + 1 > EndPtr)
        {
            LOG_ERROR_MESSAGE("Resource name pointer is outside of the specified byte code range. The byte code may be corrupted.");
            return false;
        }

        auto Iter = ResourceMap.find(HashMapStringKey{Name});
        if (Iter == ResourceMap.end())
        {
            LOG_ERROR_MESSAGE("Failed to find '", Name, "' in the resource mapping.");
            return false;
        }

        Res.BindPoint = Iter->second.BindPoint;
        if (!PatchSpace(Res, Iter->second.Space))
        {
            LOG_ERROR_MESSAGE("Can not change space for resource '", Name, "' because the shader was not compiled for SM 5.1.");
            return false;
        }
    }
    return true;
}

} // namespace


bool DXBCUtils::RemapResourceBindings(const TResourceBindingMap& ResourceMap,
                                      void*                      pBytecode,
                                      size_t                     Size)
{
    if (pBytecode == nullptr)
    {
        LOG_ERROR_MESSAGE("pBytecode must not be null.");
        return false;
    }

    auto* const       Ptr    = static_cast<char*>(pBytecode);
    const void* const EndPtr = Ptr + Size;

    if (Size < sizeof(DXBCHeader))
    {
        LOG_ERROR_MESSAGE("The size of the byte code (", Size, ") is too small to contain the DXBC header. The byte code may be corrupted.");
        return false;
    }

    auto& Header = *reinterpret_cast<DXBCHeader*>(Ptr);
    if (Header.TotalSize != Size)
    {
        LOG_ERROR_MESSAGE("The byte code size (", Header.TotalSize, ") specified in the header does not match the actual size (", Size,
                          "). The byte code may be corrupted.");
        return false;
    }

#ifdef DILIGENT_DEVELOPMENT
    {
        DWORD Checksum[4] = {};
        CalculateDXBCChecksum(reinterpret_cast<BYTE*>(Ptr), static_cast<DWORD>(Size), Checksum);

        DEV_CHECK_ERR(Checksum[0] == Header.Checksum[0] &&
                          Checksum[1] == Header.Checksum[1] &&
                          Checksum[2] == Header.Checksum[2] &&
                          Checksum[3] == Header.Checksum[3],
                      "Unexpected checksum. The byte code may be corrupted or the container format may have changed.");
    }
#endif

    if (Header.Magic != DXBCFourCC)
    {
        LOG_ERROR_MESSAGE("Bytecode header does not contain the 'DXBC' magic number. The byte code may be corrupted.");
        return false;
    }

    const Uint32* Chunks = reinterpret_cast<Uint32*>(Ptr + sizeof(Header));

    bool RemappingOK = false;
    for (Uint32 i = 0; i < Header.ChunkCount; ++i)
    {
        auto* pChunk = reinterpret_cast<ChunkHeader*>(Ptr + Chunks[i]);
        if (pChunk + 1 > EndPtr)
        {
            LOG_ERROR_MESSAGE("Not enough space for the chunk header. The byte code may be corrupted.");
            return false;
        }

        if (pChunk->Magic == RDEFFourCC)
        {
            auto* RDEFHeader = reinterpret_cast<ResourceDefChunkHeader*>(pChunk);

            if (RDEFHeader->MajorVersion == 5 && RDEFHeader->MinorVersion == 1)
            {
                RemappingOK = RemapShaderResources<ResourceBindingInfo51>(ResourceMap, EndPtr, RDEFHeader);
            }
            else if (RDEFHeader->MajorVersion == 5 && RDEFHeader->MinorVersion == 0 || RDEFHeader->MajorVersion < 5)
            {
                RemappingOK = RemapShaderResources<ResourceBindingInfo50>(ResourceMap, EndPtr, RDEFHeader);
            }
            else
            {
                LOG_ERROR_MESSAGE("Unexpected shader model: ", RDEFHeader->MajorVersion, '.', RDEFHeader->MinorVersion);
                RemappingOK = false;
            }

            if (!RemappingOK)
                return false;

            break;
        }
    }

    if (!RemappingOK)
    {
        LOG_ERROR_MESSAGE("Failed to find 'RDEF' chunk with the resource definition.");
        return false;
    }

    // update checksum
    DWORD Checksum[4] = {};
    CalculateDXBCChecksum(reinterpret_cast<BYTE*>(Ptr), static_cast<DWORD>(Size), Checksum);

    static_assert(sizeof(Header.Checksum) == sizeof(Checksum), "Unexpected checksum size");
    memcpy(Header.Checksum, Checksum, sizeof(Header.Checksum));

    return true;
}

} // namespace Diligent
