/*
 *  Copyright 2019-2020 Diligent Graphics LLC
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

#include "D3D12/TestingEnvironmentD3D12.hpp"
#include "D3D12/TestingSwapChainD3D12.hpp"
#include "../include/DXGITypeConversions.hpp"
#include "../include/d3dx12_win.h"

#include "Align.hpp"
#include "BasicMath.hpp"

#include "DeviceContextD3D12.h"

#include "InlineShaders/RayTracingTestHLSL.h"

namespace Diligent
{

namespace Testing
{

namespace
{

struct RTContext
{
    ID3D12Device5*                       pDevice = nullptr;
    CComPtr<ID3D12GraphicsCommandList4>  pCmdList;
    CComPtr<ID3D12StateObject>           pRayTracingSO;
    CComPtr<ID3D12StateObjectProperties> pStateObjectProperties;
    CComPtr<ID3D12RootSignature>         pRootSignature;
    CComPtr<ID3D12Resource>              pBLAS;
    UINT64                               BLASBuildScratchSize  = 0;
    UINT64                               BLASUpdateScratchSize = 0;
    CComPtr<ID3D12Resource>              pTLAS;
    UINT64                               TLASBuildScratchSize  = 0;
    UINT64                               TLASUpdateScratchSize = 0;
    CComPtr<ID3D12Resource>              pScratchBuffer;
    CComPtr<ID3D12Resource>              pVertexBuffer;
    CComPtr<ID3D12Resource>              pIndexBuffer;
    CComPtr<ID3D12Resource>              pInstanceBuffer;
    CComPtr<ID3D12Resource>              pSBTBuffer;
    CComPtr<ID3D12Resource>              pUploadBuffer;
    void*                                MappedPtr     = nullptr;
    size_t                               MappedOffset  = 0;
    ID3D12Resource*                      pRenderTarget = nullptr;
    CComPtr<ID3D12DescriptorHeap>        pDescHeap;
    Uint32                               DescHeapCount  = 0;
    Uint32                               DescHandleSize = 0;

    RTContext()
    {}

    ~RTContext()
    {
        if (pUploadBuffer && MappedPtr)
            pUploadBuffer->Unmap(0, nullptr);
    }

    static constexpr UINT DescriptorHeapSize = 16;
};

template <typename PSOCtorType>
void InitializeRTContext(RTContext& Ctx, ISwapChain* pSwapChain, PSOCtorType&& PSOCtor)
{
    auto* pEnv                   = TestingEnvironmentD3D12::GetInstance();
    auto* pTestingSwapChainD3D12 = ValidatedCast<TestingSwapChainD3D12>(pSwapChain);

    auto hr = pEnv->GetD3D12Device()->QueryInterface(IID_PPV_ARGS(&Ctx.pDevice));
    ASSERT_HRESULT_SUCCEEDED(hr) << "Failed to get ID3D12Device2";

    Ctx.pRenderTarget = pTestingSwapChainD3D12->GetD3D12RenderTarget();

    hr = pEnv->CreateGraphicsCommandList()->QueryInterface(IID_PPV_ARGS(&Ctx.pCmdList));
    ASSERT_HRESULT_SUCCEEDED(hr) << "Failed to get ID3D12GraphicsCommandList4";

    // create root signature
    {
        D3D12_ROOT_SIGNATURE_DESC RootSignatureDesc   = {};
        D3D12_DESCRIPTOR_RANGE    DescriptorRanges[2] = {};
        D3D12_ROOT_PARAMETER      Params[1]           = {};

        DescriptorRanges[0].RangeType                         = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
        DescriptorRanges[0].NumDescriptors                    = 1;
        DescriptorRanges[0].BaseShaderRegister                = 0;
        DescriptorRanges[0].RegisterSpace                     = 0;
        DescriptorRanges[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

        DescriptorRanges[1].RangeType                         = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
        DescriptorRanges[1].NumDescriptors                    = 1;
        DescriptorRanges[1].BaseShaderRegister                = 0;
        DescriptorRanges[1].RegisterSpace                     = 0;
        DescriptorRanges[1].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

        Params[0].ParameterType                       = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        Params[0].ShaderVisibility                    = D3D12_SHADER_VISIBILITY_ALL;
        Params[0].DescriptorTable.NumDescriptorRanges = _countof(DescriptorRanges);
        Params[0].DescriptorTable.pDescriptorRanges   = DescriptorRanges;

        RootSignatureDesc.Flags         = D3D12_ROOT_SIGNATURE_FLAG_NONE;
        RootSignatureDesc.NumParameters = _countof(Params);
        RootSignatureDesc.pParameters   = Params;

        CComPtr<ID3DBlob> signature;
        hr = D3D12SerializeRootSignature(&RootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, nullptr);
        ASSERT_HRESULT_SUCCEEDED(hr);

        hr = Ctx.pDevice->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&Ctx.pRootSignature));
        ASSERT_HRESULT_SUCCEEDED(hr);
    }

    // create ray tracing state object
    {
        std::vector<D3D12_STATE_SUBOBJECT>   Subobjects;
        std::vector<D3D12_EXPORT_DESC>       ExportDescs;
        std::vector<D3D12_DXIL_LIBRARY_DESC> LibDescs;
        std::vector<D3D12_HIT_GROUP_DESC>    HitGroups;
        std::vector<CComPtr<ID3DBlob>>       ShadersByteCode;

        PSOCtor(Subobjects, ExportDescs, LibDescs, HitGroups, ShadersByteCode);

        D3D12_RAYTRACING_PIPELINE_CONFIG PipelineConfig;
        PipelineConfig.MaxTraceRecursionDepth = 1;
        Subobjects.push_back({D3D12_STATE_SUBOBJECT_TYPE_RAYTRACING_PIPELINE_CONFIG, &PipelineConfig});

        D3D12_RAYTRACING_SHADER_CONFIG ShaderConfig;
        ShaderConfig.MaxAttributeSizeInBytes = D3D12_RAYTRACING_MAX_ATTRIBUTE_SIZE_IN_BYTES;
        ShaderConfig.MaxPayloadSizeInBytes   = 4 * sizeof(float);
        Subobjects.push_back({D3D12_STATE_SUBOBJECT_TYPE_RAYTRACING_SHADER_CONFIG, &ShaderConfig});

        D3D12_GLOBAL_ROOT_SIGNATURE GlobalRoot;
        GlobalRoot.pGlobalRootSignature = Ctx.pRootSignature;
        Subobjects.push_back({D3D12_STATE_SUBOBJECT_TYPE_GLOBAL_ROOT_SIGNATURE, &GlobalRoot});

        D3D12_STATE_OBJECT_DESC RTPipelineDesc;
        RTPipelineDesc.Type          = D3D12_STATE_OBJECT_TYPE_RAYTRACING_PIPELINE;
        RTPipelineDesc.NumSubobjects = static_cast<UINT>(Subobjects.size());
        RTPipelineDesc.pSubobjects   = Subobjects.data();

        hr = Ctx.pDevice->CreateStateObject(&RTPipelineDesc, IID_PPV_ARGS(&Ctx.pRayTracingSO));
        ASSERT_HRESULT_SUCCEEDED(hr) << "Failed to create state object";

        hr = Ctx.pRayTracingSO->QueryInterface(IID_PPV_ARGS(&Ctx.pStateObjectProperties));
        ASSERT_HRESULT_SUCCEEDED(hr) << "Failed to get state object properties";
    }

    // create descriptor heap
    {
        D3D12_DESCRIPTOR_HEAP_DESC Desc = {};

        Desc.Type           = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        Desc.NumDescriptors = Ctx.DescriptorHeapSize;
        Desc.Flags          = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
        Desc.NodeMask       = 0;

        hr = Ctx.pDevice->CreateDescriptorHeap(&Desc, IID_PPV_ARGS(&Ctx.pDescHeap));
        ASSERT_HRESULT_SUCCEEDED(hr) << "Failed to create descriptor heap";

        Ctx.DescHeapCount  = 0;
        Ctx.DescHandleSize = Ctx.pDevice->GetDescriptorHandleIncrementSize(Desc.Type);

        D3D12_UNORDERED_ACCESS_VIEW_DESC UAVDesc = {};

        UAVDesc.Format        = DXGI_FORMAT_R8G8B8A8_UNORM;
        UAVDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;

        D3D12_CPU_DESCRIPTOR_HANDLE UAVHandle = Ctx.pDescHeap->GetCPUDescriptorHandleForHeapStart();
        ASSERT_LT(Ctx.DescHeapCount, Ctx.DescriptorHeapSize);
        UAVHandle.ptr += Ctx.DescHandleSize * Ctx.DescHeapCount++;
        Ctx.pDevice->CreateUnorderedAccessView(pTestingSwapChainD3D12->GetD3D12RenderTarget(), nullptr, &UAVDesc, UAVHandle);
    }
}

void CreateBLAS(RTContext& Ctx, D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS& BottomLevelInputs)
{
    D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO BottomLevelPrebuildInfo = {};

    BottomLevelInputs.Type        = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;
    BottomLevelInputs.Flags       = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_NONE;
    BottomLevelInputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;

    Ctx.pDevice->GetRaytracingAccelerationStructurePrebuildInfo(&BottomLevelInputs, &BottomLevelPrebuildInfo);
    ASSERT_GT(BottomLevelPrebuildInfo.ResultDataMaxSizeInBytes, UINT64{0});

    D3D12_HEAP_PROPERTIES HeapProps;
    HeapProps.Type                 = D3D12_HEAP_TYPE_DEFAULT;
    HeapProps.CPUPageProperty      = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
    HeapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
    HeapProps.CreationNodeMask     = 1;
    HeapProps.VisibleNodeMask      = 1;

    D3D12_RESOURCE_DESC ASDesc = {};
    ASDesc.Dimension           = D3D12_RESOURCE_DIMENSION_BUFFER;
    ASDesc.Alignment           = 0;
    ASDesc.Width               = BottomLevelPrebuildInfo.ResultDataMaxSizeInBytes;
    ASDesc.Height              = 1;
    ASDesc.DepthOrArraySize    = 1;
    ASDesc.MipLevels           = 1;
    ASDesc.Format              = DXGI_FORMAT_UNKNOWN;
    ASDesc.SampleDesc.Count    = 1;
    ASDesc.SampleDesc.Quality  = 0;
    ASDesc.Layout              = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    ASDesc.Flags               = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

    auto hr = Ctx.pDevice->CreateCommittedResource(&HeapProps, D3D12_HEAP_FLAG_NONE,
                                                   &ASDesc, D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE, nullptr,
                                                   IID_PPV_ARGS(&Ctx.pBLAS));
    ASSERT_HRESULT_SUCCEEDED(hr) << "Failed to create acceleration structure";

    Ctx.BLASBuildScratchSize  = BottomLevelPrebuildInfo.ScratchDataSizeInBytes;
    Ctx.BLASUpdateScratchSize = BottomLevelPrebuildInfo.UpdateScratchDataSizeInBytes;
}

void CreateTLAS(RTContext& Ctx, D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS& TopLevelInputs)
{
    D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO TopLevelPrebuildInfo = {};

    TopLevelInputs.Type        = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;
    TopLevelInputs.Flags       = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_NONE;
    TopLevelInputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;

    Ctx.pDevice->GetRaytracingAccelerationStructurePrebuildInfo(&TopLevelInputs, &TopLevelPrebuildInfo);
    ASSERT_GT(TopLevelPrebuildInfo.ResultDataMaxSizeInBytes, UINT64{0});

    D3D12_HEAP_PROPERTIES HeapProps;
    HeapProps.Type                 = D3D12_HEAP_TYPE_DEFAULT;
    HeapProps.CPUPageProperty      = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
    HeapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
    HeapProps.CreationNodeMask     = 1;
    HeapProps.VisibleNodeMask      = 1;

    D3D12_RESOURCE_DESC ASDesc = {};
    ASDesc.Dimension           = D3D12_RESOURCE_DIMENSION_BUFFER;
    ASDesc.Alignment           = 0;
    ASDesc.Width               = TopLevelPrebuildInfo.ResultDataMaxSizeInBytes;
    ASDesc.Height              = 1;
    ASDesc.DepthOrArraySize    = 1;
    ASDesc.MipLevels           = 1;
    ASDesc.Format              = DXGI_FORMAT_UNKNOWN;
    ASDesc.SampleDesc.Count    = 1;
    ASDesc.SampleDesc.Quality  = 0;
    ASDesc.Layout              = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    ASDesc.Flags               = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

    auto hr = Ctx.pDevice->CreateCommittedResource(&HeapProps, D3D12_HEAP_FLAG_NONE,
                                                   &ASDesc, D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE, nullptr,
                                                   IID_PPV_ARGS(&Ctx.pTLAS));
    ASSERT_HRESULT_SUCCEEDED(hr) << "Failed to create acceleration structure";

    Ctx.TLASBuildScratchSize  = TopLevelPrebuildInfo.ScratchDataSizeInBytes;
    Ctx.TLASUpdateScratchSize = TopLevelPrebuildInfo.UpdateScratchDataSizeInBytes;

    D3D12_SHADER_RESOURCE_VIEW_DESC SRVDesc          = {};
    SRVDesc.ViewDimension                            = D3D12_SRV_DIMENSION_RAYTRACING_ACCELERATION_STRUCTURE;
    SRVDesc.Shader4ComponentMapping                  = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    SRVDesc.Format                                   = DXGI_FORMAT_UNKNOWN;
    SRVDesc.RaytracingAccelerationStructure.Location = Ctx.pTLAS->GetGPUVirtualAddress();

    D3D12_CPU_DESCRIPTOR_HANDLE DescHandle = Ctx.pDescHeap->GetCPUDescriptorHandleForHeapStart();
    ASSERT_LT(Ctx.DescHeapCount, Ctx.DescriptorHeapSize);
    DescHandle.ptr += Ctx.DescHandleSize * Ctx.DescHeapCount++;

    Ctx.pDevice->CreateShaderResourceView(nullptr, &SRVDesc, DescHandle);
}

void CreateRTBuffers(RTContext& Ctx, Uint32 VBSize, Uint32 IBSize, Uint32 InstanceCount, Uint32 NumMissShaders, Uint32 NumHitShaders)
{
    D3D12_RESOURCE_DESC BuffDesc = {};
    BuffDesc.Dimension           = D3D12_RESOURCE_DIMENSION_BUFFER;
    BuffDesc.Alignment           = 0;
    BuffDesc.Height              = 1;
    BuffDesc.DepthOrArraySize    = 1;
    BuffDesc.MipLevels           = 1;
    BuffDesc.Format              = DXGI_FORMAT_UNKNOWN;
    BuffDesc.SampleDesc.Count    = 1;
    BuffDesc.SampleDesc.Quality  = 0;
    BuffDesc.Layout              = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    BuffDesc.Flags               = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

    D3D12_HEAP_PROPERTIES HeapProps;
    HeapProps.Type                 = D3D12_HEAP_TYPE_DEFAULT;
    HeapProps.CPUPageProperty      = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
    HeapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
    HeapProps.CreationNodeMask     = 1;
    HeapProps.VisibleNodeMask      = 1;

    BuffDesc.Width = std::max(Ctx.BLASBuildScratchSize, Ctx.BLASUpdateScratchSize);
    BuffDesc.Width = std::max(BuffDesc.Width, Ctx.TLASBuildScratchSize);
    BuffDesc.Width = std::max(BuffDesc.Width, Ctx.TLASUpdateScratchSize);

    auto hr = Ctx.pDevice->CreateCommittedResource(&HeapProps, D3D12_HEAP_FLAG_NONE,
                                                   &BuffDesc, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, nullptr,
                                                   IID_PPV_ARGS(&Ctx.pScratchBuffer));
    ASSERT_HRESULT_SUCCEEDED(hr) << "Failed to create buffer";

    size_t UploadSize = 0;

    if (VBSize > 0)
    {
        BuffDesc.Width = VBSize;

        hr = Ctx.pDevice->CreateCommittedResource(&HeapProps, D3D12_HEAP_FLAG_NONE,
                                                  &BuffDesc, D3D12_RESOURCE_STATE_COPY_DEST, nullptr,
                                                  IID_PPV_ARGS(&Ctx.pVertexBuffer));
        ASSERT_HRESULT_SUCCEEDED(hr) << "Failed to create buffer";
        UploadSize += BuffDesc.Width;
    }

    if (IBSize > 0)
    {
        BuffDesc.Width = IBSize;

        hr = Ctx.pDevice->CreateCommittedResource(&HeapProps, D3D12_HEAP_FLAG_NONE,
                                                  &BuffDesc, D3D12_RESOURCE_STATE_COPY_DEST, nullptr,
                                                  IID_PPV_ARGS(&Ctx.pIndexBuffer));
        ASSERT_HRESULT_SUCCEEDED(hr) << "Failed to create buffer";
        UploadSize += BuffDesc.Width;
    }

    if (InstanceCount > 0)
    {
        BuffDesc.Width = InstanceCount * sizeof(D3D12_RAYTRACING_INSTANCE_DESC);

        hr = Ctx.pDevice->CreateCommittedResource(&HeapProps, D3D12_HEAP_FLAG_NONE,
                                                  &BuffDesc, D3D12_RESOURCE_STATE_COPY_DEST, nullptr,
                                                  IID_PPV_ARGS(&Ctx.pInstanceBuffer));
        ASSERT_HRESULT_SUCCEEDED(hr) << "Failed to create buffer";
        UploadSize += BuffDesc.Width;
    }

    // SBT
    {
        const UINT64 handleSize = D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES;
        const UINT64 align      = D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT;

        BuffDesc.Width = Align(handleSize, align);
        BuffDesc.Width = Align(BuffDesc.Width + NumMissShaders * handleSize, align);
        BuffDesc.Width = Align(BuffDesc.Width + NumHitShaders * handleSize, align);

        hr = Ctx.pDevice->CreateCommittedResource(&HeapProps, D3D12_HEAP_FLAG_NONE,
                                                  &BuffDesc, D3D12_RESOURCE_STATE_COPY_DEST, nullptr,
                                                  IID_PPV_ARGS(&Ctx.pSBTBuffer));
        ASSERT_HRESULT_SUCCEEDED(hr) << "Failed to create buffer";
        UploadSize += BuffDesc.Width;
    }

    BuffDesc.Flags = D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE;
    HeapProps.Type = D3D12_HEAP_TYPE_UPLOAD;

    if (UploadSize > 0)
    {
        BuffDesc.Width = UploadSize;
        hr             = Ctx.pDevice->CreateCommittedResource(&HeapProps, D3D12_HEAP_FLAG_NONE,
                                                  &BuffDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
                                                  IID_PPV_ARGS(&Ctx.pUploadBuffer));
        ASSERT_HRESULT_SUCCEEDED(hr) << "Failed to create buffer";

        hr = Ctx.pUploadBuffer->Map(0, nullptr, &Ctx.MappedPtr);
        ASSERT_HRESULT_SUCCEEDED(hr) << "Failed to map buffer";

        Ctx.MappedOffset = 0;
    }
}

void UpdateBuffer(RTContext& Ctx, ID3D12Resource* pBuffer, size_t Offset, const void* pData, size_t DataSize)
{
    VERIFY_EXPR(pBuffer != nullptr);
    VERIFY_EXPR(pData != nullptr);

    Ctx.pCmdList->CopyBufferRegion(pBuffer, Offset, Ctx.pUploadBuffer, Ctx.MappedOffset, DataSize);

    std::memcpy(reinterpret_cast<char*>(Ctx.MappedPtr) + Ctx.MappedOffset, pData, DataSize);
    Ctx.MappedOffset += DataSize;
}

} // namespace


void RayTracingTriangleClosestHitReferenceD3D12(ISwapChain* pSwapChain)
{
    auto* pEnv                   = TestingEnvironmentD3D12::GetInstance();
    auto* pTestingSwapChainD3D12 = ValidatedCast<TestingSwapChainD3D12>(pSwapChain);

    const auto& SCDesc = pSwapChain->GetDesc();

    RTContext Ctx = {};
    InitializeRTContext(Ctx, pSwapChain,
                        [pEnv](auto& Subobjects, auto& ExportDescs, auto& LibDescs, auto& HitGroups, auto& ShadersByteCode) {
                            ShadersByteCode.resize(3);
                            ExportDescs.resize(ShadersByteCode.size());
                            LibDescs.resize(ShadersByteCode.size());
                            HitGroups.resize(1);

                            auto hr = pEnv->CompileDXILShader(HLSL::RayTracingTest1_RG, L"main", nullptr, 0, L"lib_6_3", &ShadersByteCode[0]);
                            ASSERT_HRESULT_SUCCEEDED(hr) << "Failed to compile ray gen shader";

                            hr = pEnv->CompileDXILShader(HLSL::RayTracingTest1_RM, L"main", nullptr, 0, L"lib_6_3", &ShadersByteCode[1]);
                            ASSERT_HRESULT_SUCCEEDED(hr) << "Failed to compile ray miss shader";

                            hr = pEnv->CompileDXILShader(HLSL::RayTracingTest1_RCH, L"main", nullptr, 0, L"lib_6_3", &ShadersByteCode[2]);
                            ASSERT_HRESULT_SUCCEEDED(hr) << "Failed to compile ray closest hit shader";

                            D3D12_EXPORT_DESC&       RGExportDesc = ExportDescs[0];
                            D3D12_DXIL_LIBRARY_DESC& RGLibDesc    = LibDescs[0];
                            RGExportDesc.Flags                    = D3D12_EXPORT_FLAG_NONE;
                            RGExportDesc.ExportToRename           = L"main"; // shader entry name
                            RGExportDesc.Name                     = L"Main";
                            RGLibDesc.DXILLibrary.BytecodeLength  = ShadersByteCode[0]->GetBufferSize();
                            RGLibDesc.DXILLibrary.pShaderBytecode = ShadersByteCode[0]->GetBufferPointer();
                            RGLibDesc.NumExports                  = 1;
                            RGLibDesc.pExports                    = &RGExportDesc;
                            Subobjects.push_back({D3D12_STATE_SUBOBJECT_TYPE_DXIL_LIBRARY, &RGLibDesc});

                            D3D12_EXPORT_DESC&       RMExportDesc = ExportDescs[1];
                            D3D12_DXIL_LIBRARY_DESC& RMLibDesc    = LibDescs[1];
                            RMExportDesc.Flags                    = D3D12_EXPORT_FLAG_NONE;
                            RMExportDesc.ExportToRename           = L"main"; // shader entry name
                            RMExportDesc.Name                     = L"Miss";
                            RMLibDesc.DXILLibrary.BytecodeLength  = ShadersByteCode[1]->GetBufferSize();
                            RMLibDesc.DXILLibrary.pShaderBytecode = ShadersByteCode[1]->GetBufferPointer();
                            RMLibDesc.NumExports                  = 1;
                            RMLibDesc.pExports                    = &RMExportDesc;
                            Subobjects.push_back({D3D12_STATE_SUBOBJECT_TYPE_DXIL_LIBRARY, &RMLibDesc});

                            D3D12_EXPORT_DESC&       RCHExportDesc = ExportDescs[2];
                            D3D12_DXIL_LIBRARY_DESC& RCHLibDesc    = LibDescs[2];
                            RCHExportDesc.Flags                    = D3D12_EXPORT_FLAG_NONE;
                            RCHExportDesc.ExportToRename           = L"main"; // shader entry name
                            RCHExportDesc.Name                     = L"ClosestHitShader";
                            RCHLibDesc.DXILLibrary.BytecodeLength  = ShadersByteCode[2]->GetBufferSize();
                            RCHLibDesc.DXILLibrary.pShaderBytecode = ShadersByteCode[2]->GetBufferPointer();
                            RCHLibDesc.NumExports                  = 1;
                            RCHLibDesc.pExports                    = &RCHExportDesc;
                            Subobjects.push_back({D3D12_STATE_SUBOBJECT_TYPE_DXIL_LIBRARY, &RCHLibDesc});

                            D3D12_HIT_GROUP_DESC& HitGroupDesc    = HitGroups[0];
                            HitGroupDesc.HitGroupExport           = L"HitGroup";
                            HitGroupDesc.Type                     = D3D12_HIT_GROUP_TYPE_TRIANGLES;
                            HitGroupDesc.ClosestHitShaderImport   = L"ClosestHitShader";
                            HitGroupDesc.AnyHitShaderImport       = nullptr;
                            HitGroupDesc.IntersectionShaderImport = nullptr;
                            Subobjects.push_back({D3D12_STATE_SUBOBJECT_TYPE_HIT_GROUP, &HitGroupDesc});
                        });

    // Create acceleration structures
    {
        D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC    BLASDesc          = {};
        D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS& BottomLevelInputs = BLASDesc.Inputs;
        D3D12_RAYTRACING_GEOMETRY_DESC                        Geometry          = {};
        D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC    TLASDesc          = {};
        D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS& TopLevelInputs    = TLASDesc.Inputs;
        D3D12_RAYTRACING_INSTANCE_DESC                        Instance          = {};

        const float3 Vertices[] = //
            {
                float3{0.25f, 0.25f, 0.0f},
                float3{0.75f, 0.25f, 0.0f},
                float3{0.50f, 0.75f, 0.0f} //
            };

        Geometry.Type                                 = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES;
        Geometry.Flags                                = D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE;
        Geometry.Triangles.VertexBuffer.StartAddress  = 0;
        Geometry.Triangles.VertexBuffer.StrideInBytes = sizeof(Vertices[0]);
        Geometry.Triangles.VertexFormat               = DXGI_FORMAT_R32G32B32_FLOAT;
        Geometry.Triangles.VertexCount                = _countof(Vertices);
        Geometry.Triangles.IndexCount                 = 0;
        Geometry.Triangles.IndexFormat                = DXGI_FORMAT_UNKNOWN;
        Geometry.Triangles.IndexBuffer                = 0;
        Geometry.Triangles.Transform3x4               = 0;

        BottomLevelInputs.pGeometryDescs = &Geometry;
        BottomLevelInputs.NumDescs       = 1;

        TopLevelInputs.NumDescs = 1;

        CreateBLAS(Ctx, BottomLevelInputs);
        CreateTLAS(Ctx, TopLevelInputs);
        CreateRTBuffers(Ctx, sizeof(Vertices), 0, 1, 1, 1);

        Instance.InstanceID                          = 0;
        Instance.InstanceContributionToHitGroupIndex = 0;
        Instance.InstanceMask                        = 0xFF;
        Instance.Flags                               = D3D12_RAYTRACING_INSTANCE_FLAG_NONE;
        Instance.AccelerationStructure               = Ctx.pBLAS->GetGPUVirtualAddress();
        Instance.Transform[0][0]                     = 1.0f;
        Instance.Transform[1][1]                     = 1.0f;
        Instance.Transform[2][2]                     = 1.0f;

        UpdateBuffer(Ctx, Ctx.pVertexBuffer, 0, Vertices, sizeof(Vertices));
        UpdateBuffer(Ctx, Ctx.pInstanceBuffer, 0, &Instance, sizeof(Instance));

        // Vertex & instance buffer barriers
        {
            std::vector<D3D12_RESOURCE_BARRIER> Barriers;
            D3D12_RESOURCE_BARRIER              Barrier;

            Barrier.Type                   = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            Barrier.Flags                  = D3D12_RESOURCE_BARRIER_FLAG_NONE;
            Barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
            Barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
            Barrier.Transition.StateAfter  = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;

            if (Ctx.pVertexBuffer)
            {
                Barrier.Transition.pResource = Ctx.pVertexBuffer;
                Barriers.push_back(Barrier);
            }
            if (Ctx.pIndexBuffer)
            {
                Barrier.Transition.pResource = Ctx.pIndexBuffer;
                Barriers.push_back(Barrier);
            }
            if (Ctx.pInstanceBuffer)
            {
                Barrier.Transition.pResource = Ctx.pInstanceBuffer;
                Barriers.push_back(Barrier);
            }
            Ctx.pCmdList->ResourceBarrier(static_cast<UINT>(Barriers.size()), Barriers.data());
        }

        Geometry.Triangles.VertexBuffer.StartAddress = Ctx.pVertexBuffer->GetGPUVirtualAddress();

        BLASDesc.DestAccelerationStructureData    = Ctx.pBLAS->GetGPUVirtualAddress();
        BLASDesc.ScratchAccelerationStructureData = Ctx.pScratchBuffer->GetGPUVirtualAddress();
        BLASDesc.SourceAccelerationStructureData  = 0;

        ASSERT_TRUE(Geometry.Triangles.VertexBuffer.StartAddress != 0);
        ASSERT_TRUE(BLASDesc.DestAccelerationStructureData != 0);
        ASSERT_TRUE(BLASDesc.ScratchAccelerationStructureData != 0);

        Ctx.pCmdList->BuildRaytracingAccelerationStructure(&BLASDesc, 0, nullptr);

        // UAV barrier for scratch buffer
        {
            D3D12_RESOURCE_BARRIER Barrier;
            Barrier.Type          = D3D12_RESOURCE_BARRIER_TYPE_UAV;
            Barrier.Flags         = D3D12_RESOURCE_BARRIER_FLAG_NONE;
            Barrier.UAV.pResource = Ctx.pScratchBuffer;

            Ctx.pCmdList->ResourceBarrier(1, &Barrier);
        }

        TopLevelInputs.InstanceDescs = Ctx.pInstanceBuffer->GetGPUVirtualAddress();

        TLASDesc.DestAccelerationStructureData    = Ctx.pTLAS->GetGPUVirtualAddress();
        TLASDesc.ScratchAccelerationStructureData = Ctx.pScratchBuffer->GetGPUVirtualAddress();
        TLASDesc.SourceAccelerationStructureData  = 0;

        ASSERT_TRUE(TLASDesc.DestAccelerationStructureData != 0);
        ASSERT_TRUE(TLASDesc.ScratchAccelerationStructureData != 0);

        Ctx.pCmdList->BuildRaytracingAccelerationStructure(&TLASDesc, 0, nullptr);
    }

    // Clear render target
    {
        pTestingSwapChainD3D12->TransitionRenderTarget(Ctx.pCmdList, D3D12_RESOURCE_STATE_RENDER_TARGET);

        auto RTVDesriptorHandle = pTestingSwapChainD3D12->GetRTVDescriptorHandle();

        Ctx.pCmdList->OMSetRenderTargets(1, &RTVDesriptorHandle, FALSE, nullptr);

        float ClearColor[] = {0, 0, 0, 0};
        Ctx.pCmdList->ClearRenderTargetView(RTVDesriptorHandle, ClearColor, 0, nullptr);

        Ctx.pCmdList->OMSetRenderTargets(0, nullptr, FALSE, nullptr);
    }

    // Trace rays
    {
        pTestingSwapChainD3D12->TransitionRenderTarget(Ctx.pCmdList, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

        ID3D12DescriptorHeap* DescHeaps[] = {Ctx.pDescHeap};

        Ctx.pCmdList->SetPipelineState1(Ctx.pRayTracingSO);
        Ctx.pCmdList->SetComputeRootSignature(Ctx.pRootSignature);

        Ctx.pCmdList->SetDescriptorHeaps(_countof(DescHeaps), &DescHeaps[0]);
        Ctx.pCmdList->SetComputeRootDescriptorTable(0, DescHeaps[0]->GetGPUDescriptorHandleForHeapStart());

        D3D12_DISPATCH_RAYS_DESC Desc = {};

        Desc.Width  = SCDesc.Width;
        Desc.Height = SCDesc.Height;
        Desc.Depth  = 1;

        const UINT64 handleSize     = D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES;
        const UINT64 align          = D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT;
        const size_t RayGenOffset   = 0;
        const size_t RayMissOffset  = Align(RayGenOffset + handleSize, align);
        const size_t HitGroupOffset = Align(RayMissOffset + handleSize, align);

        Desc.RayGenerationShaderRecord.StartAddress = Ctx.pSBTBuffer->GetGPUVirtualAddress() + RayGenOffset;
        Desc.RayGenerationShaderRecord.SizeInBytes  = handleSize;
        Desc.MissShaderTable.StartAddress           = Ctx.pSBTBuffer->GetGPUVirtualAddress() + RayMissOffset;
        Desc.MissShaderTable.SizeInBytes            = handleSize;
        Desc.MissShaderTable.StrideInBytes          = handleSize;
        Desc.HitGroupTable.StartAddress             = Ctx.pSBTBuffer->GetGPUVirtualAddress() + HitGroupOffset;
        Desc.HitGroupTable.SizeInBytes              = handleSize;
        Desc.HitGroupTable.StrideInBytes            = handleSize;

        UpdateBuffer(Ctx, Ctx.pSBTBuffer, RayGenOffset, Ctx.pStateObjectProperties->GetShaderIdentifier(L"Main"), handleSize);
        UpdateBuffer(Ctx, Ctx.pSBTBuffer, RayMissOffset, Ctx.pStateObjectProperties->GetShaderIdentifier(L"Miss"), handleSize);
        UpdateBuffer(Ctx, Ctx.pSBTBuffer, HitGroupOffset, Ctx.pStateObjectProperties->GetShaderIdentifier(L"HitGroup"), handleSize);

        // SBT buffer barrier
        {
            D3D12_RESOURCE_BARRIER Barrier;
            Barrier.Type                   = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            Barrier.Flags                  = D3D12_RESOURCE_BARRIER_FLAG_NONE;
            Barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
            Barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
            Barrier.Transition.StateAfter  = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
            Barrier.Transition.pResource   = Ctx.pSBTBuffer;
            Ctx.pCmdList->ResourceBarrier(1, &Barrier);
        }

        Ctx.pCmdList->DispatchRays(&Desc);
    }

    Ctx.pCmdList->Close();
    ID3D12CommandList* pCmdLits[] = {Ctx.pCmdList};

    RefCntAutoPtr<IDeviceContextD3D12> pContextD3D12{pEnv->GetDeviceContext(), IID_DeviceContextD3D12};

    auto* pQeueD3D12  = pContextD3D12->LockCommandQueue();
    auto* pd3d12Queue = pQeueD3D12->GetD3D12CommandQueue();

    pd3d12Queue->ExecuteCommandLists(_countof(pCmdLits), pCmdLits);
    pEnv->IdleCommandQueue(pd3d12Queue);

    pContextD3D12->UnlockCommandQueue();
}


void RayTracingTriangleAnyHitReferenceD3D12(ISwapChain* pSwapChain)
{
    auto* pEnv                   = TestingEnvironmentD3D12::GetInstance();
    auto* pTestingSwapChainD3D12 = ValidatedCast<TestingSwapChainD3D12>(pSwapChain);

    const auto& SCDesc = pSwapChain->GetDesc();

    RTContext Ctx = {};
    InitializeRTContext(Ctx, pSwapChain,
                        [pEnv](auto& Subobjects, auto& ExportDescs, auto& LibDescs, auto& HitGroups, auto& ShadersByteCode) {
                            ShadersByteCode.resize(4);
                            ExportDescs.resize(ShadersByteCode.size());
                            LibDescs.resize(ShadersByteCode.size());
                            HitGroups.resize(1);

                            auto hr = pEnv->CompileDXILShader(HLSL::RayTracingTest2_RG, L"main", nullptr, 0, L"lib_6_3", &ShadersByteCode[0]);
                            ASSERT_HRESULT_SUCCEEDED(hr) << "Failed to compile ray gen shader";

                            hr = pEnv->CompileDXILShader(HLSL::RayTracingTest2_RM, L"main", nullptr, 0, L"lib_6_3", &ShadersByteCode[1]);
                            ASSERT_HRESULT_SUCCEEDED(hr) << "Failed to compile ray miss shader";

                            hr = pEnv->CompileDXILShader(HLSL::RayTracingTest2_RCH, L"main", nullptr, 0, L"lib_6_3", &ShadersByteCode[2]);
                            ASSERT_HRESULT_SUCCEEDED(hr) << "Failed to compile ray closest hit shader";

                            hr = pEnv->CompileDXILShader(HLSL::RayTracingTest2_RAH, L"main", nullptr, 0, L"lib_6_3", &ShadersByteCode[3]);
                            ASSERT_HRESULT_SUCCEEDED(hr) << "Failed to compile ray any hit shader";

                            D3D12_EXPORT_DESC&       RGExportDesc = ExportDescs[0];
                            D3D12_DXIL_LIBRARY_DESC& RGLibDesc    = LibDescs[0];
                            RGExportDesc.Flags                    = D3D12_EXPORT_FLAG_NONE;
                            RGExportDesc.ExportToRename           = L"main"; // shader entry name
                            RGExportDesc.Name                     = L"Main";
                            RGLibDesc.DXILLibrary.BytecodeLength  = ShadersByteCode[0]->GetBufferSize();
                            RGLibDesc.DXILLibrary.pShaderBytecode = ShadersByteCode[0]->GetBufferPointer();
                            RGLibDesc.NumExports                  = 1;
                            RGLibDesc.pExports                    = &RGExportDesc;
                            Subobjects.push_back({D3D12_STATE_SUBOBJECT_TYPE_DXIL_LIBRARY, &RGLibDesc});

                            D3D12_EXPORT_DESC&       RMExportDesc = ExportDescs[1];
                            D3D12_DXIL_LIBRARY_DESC& RMLibDesc    = LibDescs[1];
                            RMExportDesc.Flags                    = D3D12_EXPORT_FLAG_NONE;
                            RMExportDesc.ExportToRename           = L"main"; // shader entry name
                            RMExportDesc.Name                     = L"Miss";
                            RMLibDesc.DXILLibrary.BytecodeLength  = ShadersByteCode[1]->GetBufferSize();
                            RMLibDesc.DXILLibrary.pShaderBytecode = ShadersByteCode[1]->GetBufferPointer();
                            RMLibDesc.NumExports                  = 1;
                            RMLibDesc.pExports                    = &RMExportDesc;
                            Subobjects.push_back({D3D12_STATE_SUBOBJECT_TYPE_DXIL_LIBRARY, &RMLibDesc});

                            D3D12_EXPORT_DESC&       RCHExportDesc = ExportDescs[2];
                            D3D12_DXIL_LIBRARY_DESC& RCHLibDesc    = LibDescs[2];
                            RCHExportDesc.Flags                    = D3D12_EXPORT_FLAG_NONE;
                            RCHExportDesc.ExportToRename           = L"main"; // shader entry name
                            RCHExportDesc.Name                     = L"ClosestHitShader";
                            RCHLibDesc.DXILLibrary.BytecodeLength  = ShadersByteCode[2]->GetBufferSize();
                            RCHLibDesc.DXILLibrary.pShaderBytecode = ShadersByteCode[2]->GetBufferPointer();
                            RCHLibDesc.NumExports                  = 1;
                            RCHLibDesc.pExports                    = &RCHExportDesc;
                            Subobjects.push_back({D3D12_STATE_SUBOBJECT_TYPE_DXIL_LIBRARY, &RCHLibDesc});

                            D3D12_EXPORT_DESC&       RAHExportDesc = ExportDescs[3];
                            D3D12_DXIL_LIBRARY_DESC& RAHLibDesc    = LibDescs[3];
                            RAHExportDesc.Flags                    = D3D12_EXPORT_FLAG_NONE;
                            RAHExportDesc.ExportToRename           = L"main"; // shader entry name
                            RAHExportDesc.Name                     = L"AnyHitShader";
                            RAHLibDesc.DXILLibrary.BytecodeLength  = ShadersByteCode[3]->GetBufferSize();
                            RAHLibDesc.DXILLibrary.pShaderBytecode = ShadersByteCode[3]->GetBufferPointer();
                            RAHLibDesc.NumExports                  = 1;
                            RAHLibDesc.pExports                    = &RAHExportDesc;
                            Subobjects.push_back({D3D12_STATE_SUBOBJECT_TYPE_DXIL_LIBRARY, &RAHLibDesc});

                            D3D12_HIT_GROUP_DESC& HitGroupDesc    = HitGroups[0];
                            HitGroupDesc.HitGroupExport           = L"HitGroup";
                            HitGroupDesc.Type                     = D3D12_HIT_GROUP_TYPE_TRIANGLES;
                            HitGroupDesc.ClosestHitShaderImport   = L"ClosestHitShader";
                            HitGroupDesc.AnyHitShaderImport       = L"AnyHitShader";
                            HitGroupDesc.IntersectionShaderImport = nullptr;
                            Subobjects.push_back({D3D12_STATE_SUBOBJECT_TYPE_HIT_GROUP, &HitGroupDesc});
                        });

    // Create acceleration structures
    {
        D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC    BLASDesc          = {};
        D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS& BottomLevelInputs = BLASDesc.Inputs;
        D3D12_RAYTRACING_GEOMETRY_DESC                        Geometry          = {};
        D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC    TLASDesc          = {};
        D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS& TopLevelInputs    = TLASDesc.Inputs;
        D3D12_RAYTRACING_INSTANCE_DESC                        Instance          = {};

        const float3 Vertices[] = //
            {
                float3{0.25f, 0.25f, 0.0f}, float3{0.75f, 0.25f, 0.0f}, float3{0.50f, 0.75f, 0.0f},
                float3{0.50f, 0.10f, 0.1f}, float3{0.90f, 0.90f, 0.1f}, float3{0.10f, 0.90f, 0.1f},
                float3{0.40f, 1.00f, 0.2f}, float3{0.20f, 0.40f, 0.2f}, float3{1.00f, 0.70f, 0.2f} //
            };

        Geometry.Type                                 = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES;
        Geometry.Flags                                = D3D12_RAYTRACING_GEOMETRY_FLAG_NONE;
        Geometry.Triangles.VertexBuffer.StartAddress  = 0;
        Geometry.Triangles.VertexBuffer.StrideInBytes = sizeof(Vertices[0]);
        Geometry.Triangles.VertexFormat               = DXGI_FORMAT_R32G32B32_FLOAT;
        Geometry.Triangles.VertexCount                = _countof(Vertices);
        Geometry.Triangles.IndexCount                 = 0;
        Geometry.Triangles.IndexFormat                = DXGI_FORMAT_UNKNOWN;
        Geometry.Triangles.IndexBuffer                = 0;
        Geometry.Triangles.Transform3x4               = 0;

        BottomLevelInputs.pGeometryDescs = &Geometry;
        BottomLevelInputs.NumDescs       = 1;

        TopLevelInputs.NumDescs = 1;

        CreateBLAS(Ctx, BottomLevelInputs);
        CreateTLAS(Ctx, TopLevelInputs);
        CreateRTBuffers(Ctx, sizeof(Vertices), 0, 1, 1, 1);

        Instance.InstanceID                          = 0;
        Instance.InstanceContributionToHitGroupIndex = 0;
        Instance.InstanceMask                        = 0xFF;
        Instance.Flags                               = D3D12_RAYTRACING_INSTANCE_FLAG_NONE;
        Instance.AccelerationStructure               = Ctx.pBLAS->GetGPUVirtualAddress();
        Instance.Transform[0][0]                     = 1.0f;
        Instance.Transform[1][1]                     = 1.0f;
        Instance.Transform[2][2]                     = 1.0f;

        UpdateBuffer(Ctx, Ctx.pVertexBuffer, 0, Vertices, sizeof(Vertices));
        UpdateBuffer(Ctx, Ctx.pInstanceBuffer, 0, &Instance, sizeof(Instance));

        // Vertex & instance buffer barriers
        {
            std::vector<D3D12_RESOURCE_BARRIER> Barriers;
            D3D12_RESOURCE_BARRIER              Barrier;

            Barrier.Type                   = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            Barrier.Flags                  = D3D12_RESOURCE_BARRIER_FLAG_NONE;
            Barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
            Barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
            Barrier.Transition.StateAfter  = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;

            if (Ctx.pVertexBuffer)
            {
                Barrier.Transition.pResource = Ctx.pVertexBuffer;
                Barriers.push_back(Barrier);
            }
            if (Ctx.pIndexBuffer)
            {
                Barrier.Transition.pResource = Ctx.pIndexBuffer;
                Barriers.push_back(Barrier);
            }
            if (Ctx.pInstanceBuffer)
            {
                Barrier.Transition.pResource = Ctx.pInstanceBuffer;
                Barriers.push_back(Barrier);
            }
            Ctx.pCmdList->ResourceBarrier(static_cast<UINT>(Barriers.size()), Barriers.data());
        }

        Geometry.Triangles.VertexBuffer.StartAddress = Ctx.pVertexBuffer->GetGPUVirtualAddress();

        BLASDesc.DestAccelerationStructureData    = Ctx.pBLAS->GetGPUVirtualAddress();
        BLASDesc.ScratchAccelerationStructureData = Ctx.pScratchBuffer->GetGPUVirtualAddress();
        BLASDesc.SourceAccelerationStructureData  = 0;

        ASSERT_TRUE(Geometry.Triangles.VertexBuffer.StartAddress != 0);
        ASSERT_TRUE(BLASDesc.DestAccelerationStructureData != 0);
        ASSERT_TRUE(BLASDesc.ScratchAccelerationStructureData != 0);

        Ctx.pCmdList->BuildRaytracingAccelerationStructure(&BLASDesc, 0, nullptr);

        // UAV barrier for scratch buffer
        {
            D3D12_RESOURCE_BARRIER Barrier;
            Barrier.Type          = D3D12_RESOURCE_BARRIER_TYPE_UAV;
            Barrier.Flags         = D3D12_RESOURCE_BARRIER_FLAG_NONE;
            Barrier.UAV.pResource = Ctx.pScratchBuffer;

            Ctx.pCmdList->ResourceBarrier(1, &Barrier);
        }

        TopLevelInputs.InstanceDescs = Ctx.pInstanceBuffer->GetGPUVirtualAddress();

        TLASDesc.DestAccelerationStructureData    = Ctx.pTLAS->GetGPUVirtualAddress();
        TLASDesc.ScratchAccelerationStructureData = Ctx.pScratchBuffer->GetGPUVirtualAddress();
        TLASDesc.SourceAccelerationStructureData  = 0;

        ASSERT_TRUE(TLASDesc.DestAccelerationStructureData != 0);
        ASSERT_TRUE(TLASDesc.ScratchAccelerationStructureData != 0);

        Ctx.pCmdList->BuildRaytracingAccelerationStructure(&TLASDesc, 0, nullptr);
    }

    // Clear render target
    {
        pTestingSwapChainD3D12->TransitionRenderTarget(Ctx.pCmdList, D3D12_RESOURCE_STATE_RENDER_TARGET);

        auto RTVDesriptorHandle = pTestingSwapChainD3D12->GetRTVDescriptorHandle();

        Ctx.pCmdList->OMSetRenderTargets(1, &RTVDesriptorHandle, FALSE, nullptr);

        float ClearColor[] = {0, 0, 0, 0};
        Ctx.pCmdList->ClearRenderTargetView(RTVDesriptorHandle, ClearColor, 0, nullptr);

        Ctx.pCmdList->OMSetRenderTargets(0, nullptr, FALSE, nullptr);
    }

    // Trace rays
    {
        pTestingSwapChainD3D12->TransitionRenderTarget(Ctx.pCmdList, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

        ID3D12DescriptorHeap* DescHeaps[] = {Ctx.pDescHeap};

        Ctx.pCmdList->SetPipelineState1(Ctx.pRayTracingSO);
        Ctx.pCmdList->SetComputeRootSignature(Ctx.pRootSignature);

        Ctx.pCmdList->SetDescriptorHeaps(_countof(DescHeaps), &DescHeaps[0]);
        Ctx.pCmdList->SetComputeRootDescriptorTable(0, DescHeaps[0]->GetGPUDescriptorHandleForHeapStart());

        D3D12_DISPATCH_RAYS_DESC Desc = {};

        Desc.Width  = SCDesc.Width;
        Desc.Height = SCDesc.Height;
        Desc.Depth  = 1;

        const UINT64 handleSize     = D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES;
        const UINT64 align          = D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT;
        const size_t RayGenOffset   = 0;
        const size_t RayMissOffset  = Align(RayGenOffset + handleSize, align);
        const size_t HitGroupOffset = Align(RayMissOffset + handleSize, align);

        Desc.RayGenerationShaderRecord.StartAddress = Ctx.pSBTBuffer->GetGPUVirtualAddress() + RayGenOffset;
        Desc.RayGenerationShaderRecord.SizeInBytes  = handleSize;
        Desc.MissShaderTable.StartAddress           = Ctx.pSBTBuffer->GetGPUVirtualAddress() + RayMissOffset;
        Desc.MissShaderTable.SizeInBytes            = handleSize;
        Desc.MissShaderTable.StrideInBytes          = handleSize;
        Desc.HitGroupTable.StartAddress             = Ctx.pSBTBuffer->GetGPUVirtualAddress() + HitGroupOffset;
        Desc.HitGroupTable.SizeInBytes              = handleSize;
        Desc.HitGroupTable.StrideInBytes            = handleSize;

        UpdateBuffer(Ctx, Ctx.pSBTBuffer, RayGenOffset, Ctx.pStateObjectProperties->GetShaderIdentifier(L"Main"), handleSize);
        UpdateBuffer(Ctx, Ctx.pSBTBuffer, RayMissOffset, Ctx.pStateObjectProperties->GetShaderIdentifier(L"Miss"), handleSize);
        UpdateBuffer(Ctx, Ctx.pSBTBuffer, HitGroupOffset, Ctx.pStateObjectProperties->GetShaderIdentifier(L"HitGroup"), handleSize);

        // SBT buffer barrier
        {
            D3D12_RESOURCE_BARRIER Barrier;
            Barrier.Type                   = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            Barrier.Flags                  = D3D12_RESOURCE_BARRIER_FLAG_NONE;
            Barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
            Barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
            Barrier.Transition.StateAfter  = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
            Barrier.Transition.pResource   = Ctx.pSBTBuffer;
            Ctx.pCmdList->ResourceBarrier(1, &Barrier);
        }

        Ctx.pCmdList->DispatchRays(&Desc);
    }

    Ctx.pCmdList->Close();
    ID3D12CommandList* pCmdLits[] = {Ctx.pCmdList};

    RefCntAutoPtr<IDeviceContextD3D12> pContextD3D12{pEnv->GetDeviceContext(), IID_DeviceContextD3D12};

    auto* pQeueD3D12  = pContextD3D12->LockCommandQueue();
    auto* pd3d12Queue = pQeueD3D12->GetD3D12CommandQueue();

    pd3d12Queue->ExecuteCommandLists(_countof(pCmdLits), pCmdLits);
    pEnv->IdleCommandQueue(pd3d12Queue);

    pContextD3D12->UnlockCommandQueue();
}


void RayTracingProceduralIntersectionReferenceD3D12(ISwapChain* pSwapChain)
{
    auto* pEnv                   = TestingEnvironmentD3D12::GetInstance();
    auto* pTestingSwapChainD3D12 = ValidatedCast<TestingSwapChainD3D12>(pSwapChain);

    const auto& SCDesc = pSwapChain->GetDesc();

    RTContext Ctx = {};
    InitializeRTContext(Ctx, pSwapChain,
                        [pEnv](auto& Subobjects, auto& ExportDescs, auto& LibDescs, auto& HitGroups, auto& ShadersByteCode) {
                            ShadersByteCode.resize(4);
                            ExportDescs.resize(ShadersByteCode.size());
                            LibDescs.resize(ShadersByteCode.size());
                            HitGroups.resize(1);

                            auto hr = pEnv->CompileDXILShader(HLSL::RayTracingTest3_RG, L"main", nullptr, 0, L"lib_6_3", &ShadersByteCode[0]);
                            ASSERT_HRESULT_SUCCEEDED(hr) << "Failed to compile ray gen shader";

                            hr = pEnv->CompileDXILShader(HLSL::RayTracingTest3_RM, L"main", nullptr, 0, L"lib_6_3", &ShadersByteCode[1]);
                            ASSERT_HRESULT_SUCCEEDED(hr) << "Failed to compile ray miss shader";

                            hr = pEnv->CompileDXILShader(HLSL::RayTracingTest3_RCH, L"main", nullptr, 0, L"lib_6_3", &ShadersByteCode[2]);
                            ASSERT_HRESULT_SUCCEEDED(hr) << "Failed to compile ray closest hit shader";

                            hr = pEnv->CompileDXILShader(HLSL::RayTracingTest3_RI, L"main", nullptr, 0, L"lib_6_3", &ShadersByteCode[3]);
                            ASSERT_HRESULT_SUCCEEDED(hr) << "Failed to compile ray intersection shader";

                            D3D12_EXPORT_DESC&       RGExportDesc = ExportDescs[0];
                            D3D12_DXIL_LIBRARY_DESC& RGLibDesc    = LibDescs[0];
                            RGExportDesc.Flags                    = D3D12_EXPORT_FLAG_NONE;
                            RGExportDesc.ExportToRename           = L"main"; // shader entry name
                            RGExportDesc.Name                     = L"Main";
                            RGLibDesc.DXILLibrary.BytecodeLength  = ShadersByteCode[0]->GetBufferSize();
                            RGLibDesc.DXILLibrary.pShaderBytecode = ShadersByteCode[0]->GetBufferPointer();
                            RGLibDesc.NumExports                  = 1;
                            RGLibDesc.pExports                    = &RGExportDesc;
                            Subobjects.push_back({D3D12_STATE_SUBOBJECT_TYPE_DXIL_LIBRARY, &RGLibDesc});

                            D3D12_EXPORT_DESC&       RMExportDesc = ExportDescs[1];
                            D3D12_DXIL_LIBRARY_DESC& RMLibDesc    = LibDescs[1];
                            RMExportDesc.Flags                    = D3D12_EXPORT_FLAG_NONE;
                            RMExportDesc.ExportToRename           = L"main"; // shader entry name
                            RMExportDesc.Name                     = L"Miss";
                            RMLibDesc.DXILLibrary.BytecodeLength  = ShadersByteCode[1]->GetBufferSize();
                            RMLibDesc.DXILLibrary.pShaderBytecode = ShadersByteCode[1]->GetBufferPointer();
                            RMLibDesc.NumExports                  = 1;
                            RMLibDesc.pExports                    = &RMExportDesc;
                            Subobjects.push_back({D3D12_STATE_SUBOBJECT_TYPE_DXIL_LIBRARY, &RMLibDesc});

                            D3D12_EXPORT_DESC&       RCHExportDesc = ExportDescs[2];
                            D3D12_DXIL_LIBRARY_DESC& RCHLibDesc    = LibDescs[2];
                            RCHExportDesc.Flags                    = D3D12_EXPORT_FLAG_NONE;
                            RCHExportDesc.ExportToRename           = L"main"; // shader entry name
                            RCHExportDesc.Name                     = L"ClosestHitShader";
                            RCHLibDesc.DXILLibrary.BytecodeLength  = ShadersByteCode[2]->GetBufferSize();
                            RCHLibDesc.DXILLibrary.pShaderBytecode = ShadersByteCode[2]->GetBufferPointer();
                            RCHLibDesc.NumExports                  = 1;
                            RCHLibDesc.pExports                    = &RCHExportDesc;
                            Subobjects.push_back({D3D12_STATE_SUBOBJECT_TYPE_DXIL_LIBRARY, &RCHLibDesc});

                            D3D12_EXPORT_DESC&       RIExportDesc = ExportDescs[3];
                            D3D12_DXIL_LIBRARY_DESC& RILibDesc    = LibDescs[3];
                            RIExportDesc.Flags                    = D3D12_EXPORT_FLAG_NONE;
                            RIExportDesc.ExportToRename           = L"main"; // shader entry name
                            RIExportDesc.Name                     = L"IntersectionShader";
                            RILibDesc.DXILLibrary.BytecodeLength  = ShadersByteCode[3]->GetBufferSize();
                            RILibDesc.DXILLibrary.pShaderBytecode = ShadersByteCode[3]->GetBufferPointer();
                            RILibDesc.NumExports                  = 1;
                            RILibDesc.pExports                    = &RIExportDesc;
                            Subobjects.push_back({D3D12_STATE_SUBOBJECT_TYPE_DXIL_LIBRARY, &RILibDesc});

                            D3D12_HIT_GROUP_DESC& HitGroupDesc    = HitGroups[0];
                            HitGroupDesc.HitGroupExport           = L"HitGroup";
                            HitGroupDesc.Type                     = D3D12_HIT_GROUP_TYPE_PROCEDURAL_PRIMITIVE;
                            HitGroupDesc.ClosestHitShaderImport   = L"ClosestHitShader";
                            HitGroupDesc.AnyHitShaderImport       = nullptr;
                            HitGroupDesc.IntersectionShaderImport = L"IntersectionShader";
                            Subobjects.push_back({D3D12_STATE_SUBOBJECT_TYPE_HIT_GROUP, &HitGroupDesc});
                        });

    // Create acceleration structures
    {
        D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC    BLASDesc          = {};
        D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS& BottomLevelInputs = BLASDesc.Inputs;
        D3D12_RAYTRACING_GEOMETRY_DESC                        Geometry          = {};
        D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC    TLASDesc          = {};
        D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS& TopLevelInputs    = TLASDesc.Inputs;
        D3D12_RAYTRACING_INSTANCE_DESC                        Instance          = {};

        const float3 Boxes[] = //
            {
                float3{0.25f, 0.5f, 2.0f} - float3{1.0f, 1.0f, 1.0f},
                float3{0.25f, 0.5f, 2.0f} + float3{1.0f, 1.0f, 1.0f} //
            };

        Geometry.Type                      = D3D12_RAYTRACING_GEOMETRY_TYPE_PROCEDURAL_PRIMITIVE_AABBS;
        Geometry.Flags                     = D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE;
        Geometry.AABBs.AABBs.StartAddress  = 0;
        Geometry.AABBs.AABBs.StrideInBytes = 0;
        Geometry.AABBs.AABBCount           = _countof(Boxes) / 2;

        BottomLevelInputs.pGeometryDescs = &Geometry;
        BottomLevelInputs.NumDescs       = 1;

        TopLevelInputs.NumDescs = 1;

        CreateBLAS(Ctx, BottomLevelInputs);
        CreateTLAS(Ctx, TopLevelInputs);
        CreateRTBuffers(Ctx, sizeof(Boxes), 0, 1, 1, 1);

        Instance.InstanceID                          = 0;
        Instance.InstanceContributionToHitGroupIndex = 0;
        Instance.InstanceMask                        = 0xFF;
        Instance.Flags                               = D3D12_RAYTRACING_INSTANCE_FLAG_NONE;
        Instance.AccelerationStructure               = Ctx.pBLAS->GetGPUVirtualAddress();
        Instance.Transform[0][0]                     = 1.0f;
        Instance.Transform[1][1]                     = 1.0f;
        Instance.Transform[2][2]                     = 1.0f;

        UpdateBuffer(Ctx, Ctx.pVertexBuffer, 0, Boxes, sizeof(Boxes));
        UpdateBuffer(Ctx, Ctx.pInstanceBuffer, 0, &Instance, sizeof(Instance));

        // vertex & instance buffer barrier
        {
            std::vector<D3D12_RESOURCE_BARRIER> Barriers;
            D3D12_RESOURCE_BARRIER              Barrier;

            Barrier.Type                   = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            Barrier.Flags                  = D3D12_RESOURCE_BARRIER_FLAG_NONE;
            Barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
            Barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
            Barrier.Transition.StateAfter  = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;

            if (Ctx.pVertexBuffer)
            {
                Barrier.Transition.pResource = Ctx.pVertexBuffer;
                Barriers.push_back(Barrier);
            }
            if (Ctx.pIndexBuffer)
            {
                Barrier.Transition.pResource = Ctx.pIndexBuffer;
                Barriers.push_back(Barrier);
            }
            if (Ctx.pInstanceBuffer)
            {
                Barrier.Transition.pResource = Ctx.pInstanceBuffer;
                Barriers.push_back(Barrier);
            }
            Ctx.pCmdList->ResourceBarrier(static_cast<UINT>(Barriers.size()), Barriers.data());
        }

        Geometry.AABBs.AABBs.StartAddress = Ctx.pVertexBuffer->GetGPUVirtualAddress();

        BLASDesc.DestAccelerationStructureData    = Ctx.pBLAS->GetGPUVirtualAddress();
        BLASDesc.ScratchAccelerationStructureData = Ctx.pScratchBuffer->GetGPUVirtualAddress();
        BLASDesc.SourceAccelerationStructureData  = 0;

        ASSERT_TRUE(Geometry.AABBs.AABBs.StartAddress != 0);
        ASSERT_TRUE(BLASDesc.DestAccelerationStructureData != 0);
        ASSERT_TRUE(BLASDesc.ScratchAccelerationStructureData != 0);

        Ctx.pCmdList->BuildRaytracingAccelerationStructure(&BLASDesc, 0, nullptr);

        // UAV barrier for scratch buffer
        {
            D3D12_RESOURCE_BARRIER Barrier;
            Barrier.Type          = D3D12_RESOURCE_BARRIER_TYPE_UAV;
            Barrier.Flags         = D3D12_RESOURCE_BARRIER_FLAG_NONE;
            Barrier.UAV.pResource = Ctx.pScratchBuffer;

            Ctx.pCmdList->ResourceBarrier(1, &Barrier);
        }

        TopLevelInputs.InstanceDescs = Ctx.pInstanceBuffer->GetGPUVirtualAddress();

        TLASDesc.DestAccelerationStructureData    = Ctx.pTLAS->GetGPUVirtualAddress();
        TLASDesc.ScratchAccelerationStructureData = Ctx.pScratchBuffer->GetGPUVirtualAddress();
        TLASDesc.SourceAccelerationStructureData  = 0;

        ASSERT_TRUE(TLASDesc.DestAccelerationStructureData != 0);
        ASSERT_TRUE(TLASDesc.ScratchAccelerationStructureData != 0);

        Ctx.pCmdList->BuildRaytracingAccelerationStructure(&TLASDesc, 0, nullptr);
    }

    // Clear render target
    {
        pTestingSwapChainD3D12->TransitionRenderTarget(Ctx.pCmdList, D3D12_RESOURCE_STATE_RENDER_TARGET);

        auto RTVDesriptorHandle = pTestingSwapChainD3D12->GetRTVDescriptorHandle();

        Ctx.pCmdList->OMSetRenderTargets(1, &RTVDesriptorHandle, FALSE, nullptr);

        float ClearColor[] = {0, 0, 0, 0};
        Ctx.pCmdList->ClearRenderTargetView(RTVDesriptorHandle, ClearColor, 0, nullptr);

        Ctx.pCmdList->OMSetRenderTargets(0, nullptr, FALSE, nullptr);
    }

    // Trace rays
    {
        pTestingSwapChainD3D12->TransitionRenderTarget(Ctx.pCmdList, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

        ID3D12DescriptorHeap* DescHeaps[] = {Ctx.pDescHeap};

        Ctx.pCmdList->SetPipelineState1(Ctx.pRayTracingSO);
        Ctx.pCmdList->SetComputeRootSignature(Ctx.pRootSignature);

        Ctx.pCmdList->SetDescriptorHeaps(_countof(DescHeaps), &DescHeaps[0]);
        Ctx.pCmdList->SetComputeRootDescriptorTable(0, DescHeaps[0]->GetGPUDescriptorHandleForHeapStart());

        D3D12_DISPATCH_RAYS_DESC Desc = {};

        Desc.Width  = SCDesc.Width;
        Desc.Height = SCDesc.Height;
        Desc.Depth  = 1;

        const UINT64 handleSize     = D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES;
        const UINT64 align          = D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT;
        const size_t RayGenOffset   = 0;
        const size_t RayMissOffset  = Align(RayGenOffset + handleSize, align);
        const size_t HitGroupOffset = Align(RayMissOffset + handleSize, align);

        Desc.RayGenerationShaderRecord.StartAddress = Ctx.pSBTBuffer->GetGPUVirtualAddress() + RayGenOffset;
        Desc.RayGenerationShaderRecord.SizeInBytes  = handleSize;
        Desc.MissShaderTable.StartAddress           = Ctx.pSBTBuffer->GetGPUVirtualAddress() + RayMissOffset;
        Desc.MissShaderTable.SizeInBytes            = handleSize;
        Desc.MissShaderTable.StrideInBytes          = handleSize;
        Desc.HitGroupTable.StartAddress             = Ctx.pSBTBuffer->GetGPUVirtualAddress() + HitGroupOffset;
        Desc.HitGroupTable.SizeInBytes              = handleSize;
        Desc.HitGroupTable.StrideInBytes            = handleSize;

        UpdateBuffer(Ctx, Ctx.pSBTBuffer, RayGenOffset, Ctx.pStateObjectProperties->GetShaderIdentifier(L"Main"), handleSize);
        UpdateBuffer(Ctx, Ctx.pSBTBuffer, RayMissOffset, Ctx.pStateObjectProperties->GetShaderIdentifier(L"Miss"), handleSize);
        UpdateBuffer(Ctx, Ctx.pSBTBuffer, HitGroupOffset, Ctx.pStateObjectProperties->GetShaderIdentifier(L"HitGroup"), handleSize);

        // SBT buffer barrier
        {
            D3D12_RESOURCE_BARRIER Barrier;
            Barrier.Type                   = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            Barrier.Flags                  = D3D12_RESOURCE_BARRIER_FLAG_NONE;
            Barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
            Barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
            Barrier.Transition.StateAfter  = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
            Barrier.Transition.pResource   = Ctx.pSBTBuffer;
            Ctx.pCmdList->ResourceBarrier(1, &Barrier);
        }

        Ctx.pCmdList->DispatchRays(&Desc);
    }

    Ctx.pCmdList->Close();
    ID3D12CommandList* pCmdLits[] = {Ctx.pCmdList};

    RefCntAutoPtr<IDeviceContextD3D12> pContextD3D12{pEnv->GetDeviceContext(), IID_DeviceContextD3D12};

    auto* pQeueD3D12  = pContextD3D12->LockCommandQueue();
    auto* pd3d12Queue = pQeueD3D12->GetD3D12CommandQueue();

    pd3d12Queue->ExecuteCommandLists(_countof(pCmdLits), pCmdLits);
    pEnv->IdleCommandQueue(pd3d12Queue);

    pContextD3D12->UnlockCommandQueue();
}

} // namespace Testing

} // namespace Diligent
