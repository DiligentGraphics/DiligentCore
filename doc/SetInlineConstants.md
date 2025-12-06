# SetInlineConstants Implementation Guide

## Overview

Inline constants (also known as push constants in Vulkan or root constants in Direct3D12) provide an efficient way to pass small, frequently-updated data directly from the CPU to shaders without creating buffers or modifying descriptor sets.

This document describes the implementation of `SetInlineConstants` API in DiligentCore for D3D12 and Vulkan backends.

## API Usage

### Defining Inline Constants in Pipeline Resource Signature

```cpp
PipelineResourceDesc Resources[] = {
    {
        .Name         = "Constants",
        .ShaderStages = SHADER_TYPE_VERTEX,
        .ArraySize    = 16,  // Number of 32-bit values (not bytes!)
        .ResourceType = SHADER_RESOURCE_TYPE_CONSTANT_BUFFER,
        .VarType      = SHADER_RESOURCE_VARIABLE_TYPE_STATIC,
        .Flags        = PIPELINE_RESOURCE_FLAG_INLINE_CONSTANTS
    }
};

PipelineResourceSignatureDesc PRSDesc;
PRSDesc.Resources    = Resources;
PRSDesc.NumResources = _countof(Resources);
pDevice->CreatePipelineResourceSignature(PRSDesc, &pPRS);
```

### Setting Inline Constants at Runtime

```cpp
// For static variables, use the signature
pPRS->GetStaticVariableByName(SHADER_TYPE_VERTEX, "Constants")
    ->SetInlineConstants(pData, FirstConstant, NumConstants);

// For mutable/dynamic variables, use the SRB
pSRB->GetVariableByName(SHADER_TYPE_VERTEX, "Constants")
    ->SetInlineConstants(pData, FirstConstant, NumConstants);
```

### Parameters

| Parameter       | Description                                           |
|-----------------|-------------------------------------------------------|
| `pData`         | Pointer to the constant data (array of 32-bit values) |
| `FirstConstant` | Index of the first 32-bit constant to update          |
| `NumConstants`  | Number of 32-bit constants to update                  |

## Limitations

- **Maximum size**: 64 constants (256 bytes) per inline constant resource
- **Resource type**: Only `SHADER_RESOURCE_TYPE_CONSTANT_BUFFER` supports inline constants
- **Flag exclusivity**: `PIPELINE_RESOURCE_FLAG_INLINE_CONSTANTS` cannot be combined with other flags

---

## D3D12 Backend Implementation

### Architecture Overview

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                        User Application                                      │
├─────────────────────────────────────────────────────────────────────────────┤
│  pVariable->SetInlineConstants(pData, FirstConstant, NumConstants)          │
└──────────────────────────────────┬──────────────────────────────────────────┘
                                   │
                                   ▼
┌─────────────────────────────────────────────────────────────────────────────┐
│                    ShaderVariableManagerD3D12                                │
│  ┌───────────────────────────────────────────────────────────────────────┐  │
│  │ SetInlineConstants():                                                  │  │
│  │   1. Get ResourceAttribs (RootIndex, OffsetFromTableStart)            │  │
│  │   2. Validate parameters (DEV build only)                             │  │
│  │   3. Call m_ResourceCache.SetInlineConstants(...)                     │  │
│  └───────────────────────────────────────────────────────────────────────┘  │
└──────────────────────────────────┬──────────────────────────────────────────┘
                                   │
                                   ▼
┌─────────────────────────────────────────────────────────────────────────────┐
│                     ShaderResourceCacheD3D12                                 │
│  ┌───────────────────────────────────────────────────────────────────────┐  │
│  │ SetInlineConstants():                                                  │  │
│  │   1. Get RootTable by RootIndex                                       │  │
│  │   2. Get Resource (CPUDescriptorHandle.ptr points to storage)         │  │
│  │   3. memcpy(pDstConstants + FirstConstant, pConstants, NumConstants)  │  │
│  └───────────────────────────────────────────────────────────────────────┘  │
│                                                                              │
│  Memory Layout:                                                              │
│  ┌─────────────────────────────────────────────────────────────────────┐    │
│  │ RootTables | Resources | DescriptorAllocations | InlineConstStorage │    │
│  └─────────────────────────────────────────────────────────────────────┘    │
│                                                     ▲                        │
│                   Resource.CPUDescriptorHandle.ptr ─┘                        │
└──────────────────────────────────┬──────────────────────────────────────────┘
                                   │
                                   ▼ (During CommitShaderResources)
┌─────────────────────────────────────────────────────────────────────────────┐
│               PipelineResourceSignatureD3D12Impl                             │
│  ┌───────────────────────────────────────────────────────────────────────┐  │
│  │ CommitRootConstants():                                                 │  │
│  │   while (ConstantsMask != 0) {                                        │  │
│  │     RootInd = ExtractLSB(ConstantsMask);                              │  │
│  │     pConstants = (Uint32*)Res.CPUDescriptorHandle.ptr;                │  │
│  │     NumConstants = Res.BufferRangeSize;                               │  │
│  │     pd3d12CmdList->SetGraphicsRoot32BitConstants(                     │  │
│  │         BaseRootIndex + RootInd, NumConstants, pConstants, 0);        │  │
│  │   }                                                                    │  │
│  └───────────────────────────────────────────────────────────────────────┘  │
└─────────────────────────────────────────────────────────────────────────────┘
```

### Step 1: Resource Signature Creation

When creating a `PipelineResourceSignature` with `PIPELINE_RESOURCE_FLAG_INLINE_CONSTANTS`:

**File**: `PipelineResourceSignatureD3D12Impl.cpp`

```cpp
case SHADER_RESOURCE_TYPE_CONSTANT_BUFFER:
    if ((ResDesc.Flags & PIPELINE_RESOURCE_FLAG_INLINE_CONSTANTS) != 0)
    {
        // Use D3D12 root constants instead of descriptor table or root CBV
        d3d12RootParamType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
    }
    // ...
```

The `RootParamsBuilder` allocates a dedicated root parameter:

**File**: `RootParamsManager.cpp`

```cpp
void RootParamsBuilder::AllocateResourceSlot(...)
{
    if (RootParameterType == D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS)
    {
        OffsetFromTableStart = 0;
        // ArraySize = number of 32-bit values
        AddRootConstants(RootIndex, Register, Space, ArraySize, ShaderVisibility, ParameterGroup);
    }
}
```

### Step 2: Resource Cache Initialization

The `ShaderResourceCacheD3D12` allocates memory for inline constants storage:

**File**: `ShaderResourceCacheD3D12.cpp`

```cpp
void ShaderResourceCacheD3D12::Initialize(...)
{
    // Calculate total inline constant values
    Uint32 TotalInlineConstantValues = 0;
    for (const RootParameter& RootConsts : RootParams.GetRootConstants())
    {
        TotalInlineConstantValues += RootConsts.d3d12RootParam.Constants.Num32BitValues;
    }

    // Allocate memory including space for inline constants
    AllocateMemory(MemAllocator, TotalInlineConstantValues);

    // Initialize inline constant resources
    Uint32* pCurrInlineConstValueStorage = GetInlineConstantStorage();
    for (Uint32 i = 0; i < NumRootConstants; ++i)
    {
        Resource& Res = GetResource(ResIdx);
        // Store pointer to inline constants storage in CPUDescriptorHandle.ptr
        Res.CPUDescriptorHandle.ptr = reinterpret_cast<SIZE_T>(pCurrInlineConstValueStorage);
        Res.Type = SHADER_RESOURCE_TYPE_CONSTANT_BUFFER;
        Res.BufferRangeSize = RootConsts.d3d12RootParam.Constants.Num32BitValues;
        
        pCurrInlineConstValueStorage += RootConsts.d3d12RootParam.Constants.Num32BitValues;
        m_RootConstantsMask |= (Uint64{1} << Uint64{RootConsts.RootIndex});
    }
}
```

### Step 3: SetInlineConstants Implementation

**File**: `ShaderVariableManagerD3D12.cpp`

```cpp
void ShaderVariableManagerD3D12::SetInlineConstants(Uint32      ResIndex,
                                                    const void* pConstants,
                                                    Uint32      FirstConstant,
                                                    Uint32      NumConstants)
{
    const ResourceAttribs&         Attribs   = m_pSignature->GetResourceAttribs(ResIndex);
    const ResourceCacheContentType CacheType = m_ResourceCache.GetContentType();
    const Uint32                   RootIndex = Attribs.RootIndex(CacheType);

#ifdef DILIGENT_DEVELOPMENT
    const PipelineResourceDesc& ResDesc = m_pSignature->GetResourceDesc(ResIndex);
    VerifyInlineConstants(ResDesc, pConstants, FirstConstant, NumConstants);
#endif

    m_ResourceCache.SetInlineConstants(RootIndex, pConstants, FirstConstant, NumConstants);
}
```

**File**: `ShaderResourceCacheD3D12.cpp`

```cpp
void ShaderResourceCacheD3D12::SetInlineConstants(Uint32      RootIndex,
                                                  const void* pConstants,
                                                  Uint32      FirstConstant,
                                                  Uint32      NumConstants)
{
    RootTable& Tbl = GetRootTable(RootIndex);
    Resource&  Res = Tbl.GetResource(0);
    
    VERIFY_EXPR(Res.Type == SHADER_RESOURCE_TYPE_CONSTANT_BUFFER);
    VERIFY(Res.CPUDescriptorHandle.ptr != 0, "Invalid pointer to inline constants storage");
    
    Uint32* pDstConstants = reinterpret_cast<Uint32*>(Res.CPUDescriptorHandle.ptr);
    memcpy(pDstConstants + FirstConstant, pConstants, NumConstants * sizeof(Uint32));
}
```

### Step 4: Committing to GPU

During `CommitShaderResources`, inline constants are submitted to the D3D12 command list:

**File**: `PipelineResourceSignatureD3D12Impl.cpp`

```cpp
void PipelineResourceSignatureD3D12Impl::CommitRootConstants(
    const CommitCacheResourcesAttribs& CommitAttribs,
    Uint64                             ConstantsMask) const
{
    while (ConstantsMask != 0)
    {
        const Uint64 RootIndBit = ExtractLSB(ConstantsMask);
        const Uint32 RootInd    = PlatformMisc::GetLSB(RootIndBit);
        
        const ShaderResourceCacheD3D12::RootTable& CacheTbl = 
            CommitAttribs.pResourceCache->GetRootTable(RootInd);
        const ShaderResourceCacheD3D12::Resource& Res = CacheTbl.GetResource(0);
        
        const Uint32* pConstants  = reinterpret_cast<const Uint32*>(Res.CPUDescriptorHandle.ptr);
        const Uint32  NumConstants = static_cast<Uint32>(Res.BufferRangeSize);
        
        ID3D12GraphicsCommandList* pd3d12CmdList = CommitAttribs.CmdCtx.GetCommandList();
        if (CommitAttribs.IsCompute)
        {
            pd3d12CmdList->SetComputeRoot32BitConstants(
                CommitAttribs.BaseRootIndex + RootInd, NumConstants, pConstants, 0);
        }
        else
        {
            pd3d12CmdList->SetGraphicsRoot32BitConstants(
                CommitAttribs.BaseRootIndex + RootInd, NumConstants, pConstants, 0);
        }
    }
}
```

### Multiple Inline Constants Support

D3D12 backend fully supports multiple inline constant buffers. Each inline constant resource:

- Gets its own `D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS` root parameter
- Has a unique `RootIndex`
- Has separate memory storage in the resource cache
- Is committed independently via `SetGraphicsRoot32BitConstants`

**Example**:

```cpp
PipelineResourceDesc Resources[] = {
    {"TransformCB", SHADER_TYPE_VERTEX, 16, SHADER_RESOURCE_TYPE_CONSTANT_BUFFER,
     SHADER_RESOURCE_VARIABLE_TYPE_STATIC, PIPELINE_RESOURCE_FLAG_INLINE_CONSTANTS},
    {"MaterialCB", SHADER_TYPE_PIXEL, 8, SHADER_RESOURCE_TYPE_CONSTANT_BUFFER,
     SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC, PIPELINE_RESOURCE_FLAG_INLINE_CONSTANTS},
};
```

This creates two separate root constant parameters in the D3D12 root signature.

---

## D3D11 Backend Implementation

### Architecture Overview

In D3D11, inline constants are emulated using **dynamic constant buffers** (`USAGE_DYNAMIC` with `D3D11_MAP_WRITE_DISCARD`). Unlike D3D12's direct root constants, D3D11 requires creating actual buffer objects and updating them via `Map`/`Unmap`.

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                        User Application                                      │
├─────────────────────────────────────────────────────────────────────────────┤
│  pVariable->SetInlineConstants(pData, FirstConstant, NumConstants)          │
└──────────────────────────────────┬──────────────────────────────────────────┘
                                   │
                                   ▼
┌─────────────────────────────────────────────────────────────────────────────┐
│                    ShaderVariableManagerD3D11                                │
│  ┌───────────────────────────────────────────────────────────────────────┐  │
│  │ ConstBuffBindInfo::SetConstants():                                     │  │
│  │   1. Validate parameters (DEV build only)                             │  │
│  │   2. Call m_ResourceCache.SetInlineConstants(...)                     │  │
│  └───────────────────────────────────────────────────────────────────────┘  │
└──────────────────────────────────┬──────────────────────────────────────────┘
                                   │
                                   ▼
┌─────────────────────────────────────────────────────────────────────────────┐
│                     ShaderResourceCacheD3D11                                 │
│  ┌───────────────────────────────────────────────────────────────────────┐  │
│  │ SetInlineConstants():                                                  │  │
│  │   1. Get CachedCB by BindPoints                                       │  │
│  │   2. memcpy to pInlineConstantData (CPU-side staging buffer)          │  │
│  │   3. Mark as dirty (implicit - data changed)                          │  │
│  └───────────────────────────────────────────────────────────────────────┘  │
│                                                                              │
│  CachedCB Structure:                                                         │
│  ┌─────────────────────────────────────────────────────────────────────┐    │
│  │ pBuff (ID3D11Buffer*)                                                │    │
│  │ pInlineConstantData (void*) ──> CPU staging buffer                   │    │
│  │ RangeSize, BaseOffset, DynamicOffset                                 │    │
│  └─────────────────────────────────────────────────────────────────────┘    │
└──────────────────────────────────┬──────────────────────────────────────────┘
                                   │
                                   ▼ (During Draw/Dispatch)
┌─────────────────────────────────────────────────────────────────────────────┐
│               PipelineResourceSignatureD3D11Impl                             │
│  ┌───────────────────────────────────────────────────────────────────────┐  │
│  │ UpdateInlineConstantBuffers():                                         │  │
│  │   for each inline constant buffer {                                   │  │
│  │     pd3d11Ctx->Map(pd3d11CB, 0, D3D11_MAP_WRITE_DISCARD, 0, &Mapped);│  │
│  │     memcpy(Mapped.pData, InlineCB.pInlineConstantData, Size);        │  │
│  │     pd3d11Ctx->Unmap(pd3d11CB, 0);                                    │  │
│  │   }                                                                    │  │
│  └───────────────────────────────────────────────────────────────────────┘  │
└─────────────────────────────────────────────────────────────────────────────┘
```

### Key Differences from D3D12

| Aspect                  | D3D12                          | D3D11                             |
|-------------------------|--------------------------------|-----------------------------------|
| Implementation          | Root constants (direct)        | Dynamic constant buffers (emulated)|
| Buffer creation         | Not needed                     | `USAGE_DYNAMIC` buffer required   |
| Update mechanism        | `SetGraphicsRoot32BitConstants`| `Map` + `memcpy` + `Unmap`        |
| Performance             | Very fast (no buffer)          | Slower (buffer map overhead)      |
| Memory                  | Inline in command buffer       | Separate buffer object            |

### Step 1: Resource Signature Creation

When creating a `PipelineResourceSignature` with `PIPELINE_RESOURCE_FLAG_INLINE_CONSTANTS`:

**File**: `PipelineResourceSignatureD3D11Impl.cpp`

```cpp
if (ResDesc.Flags & PIPELINE_RESOURCE_FLAG_INLINE_CONSTANTS)
{
    // Inline constant buffers are handled mostly like regular constant buffers.
    // The only difference is that the buffer is created internally and is not expected to be bound.
    // It is updated by UpdateInlineConstantBuffers() method.
    
    VERIFY(ResDesc.ResourceType == SHADER_RESOURCE_TYPE_CONSTANT_BUFFER, 
           "Only constant buffers can have INLINE_CONSTANTS flag");
    
    InlineConstantBufferAttribsD3D11& InlineCBAttribs = m_InlineConstantBuffers[InlineConstantBufferIdx++];
    InlineCBAttribs.BindPoints   = BindPoints;
    InlineCBAttribs.NumConstants = ResDesc.ArraySize;
    
    // Create a USAGE_DYNAMIC constant buffer
    BufferDesc CBDesc;
    CBDesc.Name           = m_Desc.Name + " - " + ResDesc.Name;
    CBDesc.Size           = ResDesc.ArraySize * sizeof(Uint32);
    CBDesc.Usage          = USAGE_DYNAMIC;
    CBDesc.BindFlags      = BIND_UNIFORM_BUFFER;
    CBDesc.CPUAccessFlags = CPU_ACCESS_WRITE;
    
    m_pDevice->CreateBuffer(CBDesc, nullptr, &InlineCBAttribs.pBuffer);
}
```

**Key Points**:
- Each inline constant resource creates a `USAGE_DYNAMIC` buffer
- Buffer is shared across all SRBs created from this signature
- CPU-side staging buffer (`pInlineConstantData`) is allocated separately

### Step 2: Resource Cache Initialization

**File**: `ShaderResourceCacheD3D11.cpp`

```cpp
void ShaderResourceCacheD3D11::Initialize(
    const D3D11ShaderResourceCounters&        ResCount,
    IMemoryAllocator&                         MemAllocator,
    const std::array<Uint16, NumShaderTypes>* pDynamicCBSlotsMask,
    const InlineConstantBufferAttribsD3D11*   pInlineCBs,
    Uint32                                    NumInlineCBs)
{
    // Calculate total memory including inline constant data storage
    Uint32 TotalInlineConstantDataSize = 0;
    for (Uint32 i = 0; i < NumInlineCBs; ++i)
    {
        TotalInlineConstantDataSize += pInlineCBs[i].NumConstants * sizeof(Uint32);
    }
    
    // Allocate memory: Resources + d3d11 resources + inline constant data
    size_t TotalSize = /* resource arrays */ + TotalInlineConstantDataSize;
    m_pResourceData = ALLOCATE_RAW(MemAllocator, "Memory for cache data", TotalSize);
    
    // Initialize inline constant buffers
    Uint8* pInlineConstDataStorage = /* end of resource arrays */;
    for (Uint32 i = 0; i < NumInlineCBs; ++i)
    {
        const InlineConstantBufferAttribsD3D11& InlineCBAttr = pInlineCBs[i];
        
        InitInlineConstantBuffer(
            InlineCBAttr.BindPoints,
            InlineCBAttr.pBuffer,
            InlineCBAttr.NumConstants,
            pInlineConstDataStorage  // CPU-side staging buffer
        );
        
        pInlineConstDataStorage += InlineCBAttr.NumConstants * sizeof(Uint32);
    }
}
```

**Memory Layout**:
```
┌──────────────────────────────────────────────────────────────────────────┐
│ CBV Resources | SRV Resources | Sampler Resources | UAV Resources |      │
│               |               |                   |               |      │
│ d3d11 CBVs    | d3d11 SRVs    | d3d11 Samplers    | d3d11 UAVs    |      │
│                                                                           │
│ Inline Constant Data Storage (CPU-side staging buffers)                  │
│ ┌──────────────┬──────────────┬──────────────┐                           │
│ │ InlineCB[0]  │ InlineCB[1]  │ InlineCB[2]  │ ...                       │
│ └──────────────┴──────────────┴──────────────┘                           │
└──────────────────────────────────────────────────────────────────────────┘
```

### Step 3: SetInlineConstants Implementation

**File**: `ShaderVariableManagerD3D11.cpp`

```cpp
void ShaderVariableManagerD3D11::ConstBuffBindInfo::SetConstants(
    const void* pConstants, 
    Uint32      FirstConstant, 
    Uint32      NumConstants)
{
    const PipelineResourceAttribsD3D11& Attr = GetAttribs();
    const PipelineResourceDesc&         Desc = GetDesc();
    
#ifdef DILIGENT_DEVELOPMENT
    VerifyInlineConstants(Desc, pConstants, FirstConstant, NumConstants);
#endif
    
    m_ParentManager.m_ResourceCache.SetInlineConstants(
        Attr.BindPoints, pConstants, FirstConstant, NumConstants);
}
```

**File**: `ShaderResourceCacheD3D11.hpp` (inline implementation)

```cpp
__forceinline void ShaderResourceCacheD3D11::SetInlineConstants(
    const D3D11ResourceBindPoints& BindPoints,
    const void*                    pConstants,
    Uint32                         FirstConstant,
    Uint32                         NumConstants)
{
    // Since all shader stages share the same inline constant data, 
    // we can just set it for one stage
    SHADER_TYPE ActiveStages = BindPoints.GetActiveStages();
    VERIFY_EXPR(ActiveStages != SHADER_TYPE_UNKNOWN);
    
    const Uint32 ShaderInd0 = ExtractFirstShaderStageIndex(ActiveStages);
    const Uint32 Binding0   = BindPoints[ShaderInd0];
    
    const auto ResArrays0 = GetResourceArrays<D3D11_RESOURCE_RANGE_CBV>(ShaderInd0);
    ResArrays0.first[Binding0].SetInlineConstants(pConstants, FirstConstant, NumConstants);
}
```

**File**: `ShaderResourceCacheD3D11.hpp` (CachedCB member)

```cpp
void CachedCB::SetInlineConstants(const void* pSrcConstants, 
                                  Uint32      FirstConstant, 
                                  Uint32      NumConstants)
{
    VERIFY(pSrcConstants != nullptr, "Source constant data pointer is null");
    VERIFY(FirstConstant + NumConstants <= RangeSize / sizeof(Uint32),
           "Too many constants for the allocated space");
    VERIFY(pInlineConstantData != nullptr, "Inline constant data pointer is null");
    
    // Copy to CPU-side staging buffer
    memcpy(reinterpret_cast<Uint8*>(pInlineConstantData) + FirstConstant * sizeof(Uint32),
           pSrcConstants,
           NumConstants * sizeof(Uint32));
}
```

### Step 4: Updating GPU Buffer

During draw/dispatch commands, inline constant buffers are updated:

**File**: `DeviceContextD3D11Impl.cpp`

```cpp
void DeviceContextD3D11Impl::CommitShaderResources(...)
{
    // For each bound SRB
    for (Uint32 SignIdx = 0; SignIdx < m_NumActiveSignatures; ++SignIdx)
    {
        if ((m_BindInfo.InlineConstantsSRBMask & (1u << SignIdx)) != 0)
        {
            // Always update inline constant buffers if the SRB is stale
            // or inline constants are not intact
            if (SRBStale || !InlineConstantsIntact)
            {
                PipelineResourceSignatureD3D11Impl* pSign = 
                    m_pPipelineState->GetResourceSignature(SignIdx);
                
                pSign->UpdateInlineConstantBuffers(*pResourceCache, m_pd3d11DeviceContext);
            }
        }
    }
}
```

**File**: `PipelineResourceSignatureD3D11Impl.cpp`

```cpp
void PipelineResourceSignatureD3D11Impl::UpdateInlineConstantBuffers(
    const ShaderResourceCacheD3D11& ResourceCache, 
    ID3D11DeviceContext*            pd3d11Ctx) const
{
    for (Uint32 i = 0; i < m_NumInlineConstantBuffers; ++i)
    {
        const InlineConstantBufferAttribsD3D11& InlineCBAttr = m_InlineConstantBuffers[i];
        
        // Get the cached constant buffer
        ID3D11Buffer*                             pd3d11CB = nullptr;
        const ShaderResourceCacheD3D11::CachedCB& InlineCB = 
            ResourceCache.GetResource<D3D11_RESOURCE_RANGE_CBV>(InlineCBAttr.BindPoints, &pd3d11CB);
        
        VERIFY(InlineCBAttr.NumConstants * sizeof(Uint32) == InlineCB.RangeSize, 
               "Inline constant buffer size mismatch");
        VERIFY(InlineCB.pInlineConstantData != nullptr, 
               "Inline constant data pointer is null");
        
        // Map the D3D11 buffer
        D3D11_MAPPED_SUBRESOURCE MappedData{};
        if (SUCCEEDED(pd3d11Ctx->Map(pd3d11CB, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedData)))
        {
            // Copy from CPU staging buffer to GPU buffer
            memcpy(MappedData.pData, 
                   InlineCB.pInlineConstantData, 
                   InlineCBAttr.NumConstants * sizeof(Uint32));
            
            pd3d11Ctx->Unmap(pd3d11CB, 0);
        }
    }
}
```

### Multiple Inline Constants Support

D3D11 backend fully supports multiple inline constant buffers, similar to D3D12:

- Each inline constant resource creates its own `USAGE_DYNAMIC` buffer
- Each has separate CPU-side staging buffer (`pInlineConstantData`)
- All inline constant buffers are updated together during `CommitShaderResources`

**Example**:

```cpp
PipelineResourceDesc Resources[] = {
    {"TransformCB", SHADER_TYPE_VERTEX, 16, SHADER_RESOURCE_TYPE_CONSTANT_BUFFER,
     SHADER_RESOURCE_VARIABLE_TYPE_STATIC, PIPELINE_RESOURCE_FLAG_INLINE_CONSTANTS},
    {"MaterialCB", SHADER_TYPE_PIXEL, 8, SHADER_RESOURCE_TYPE_CONSTANT_BUFFER,
     SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC, PIPELINE_RESOURCE_FLAG_INLINE_CONSTANTS},
};
```

This creates two separate dynamic constant buffers.

### Performance Considerations

**D3D11 Inline Constants Performance**:

1. **Overhead**: Higher than D3D12 due to `Map`/`Unmap` operations
2. **Update frequency**: Best for per-draw updates (not per-constant updates)
3. **Buffer sharing**: All SRBs share the same buffer, reducing memory but requiring updates
4. **Optimization flags**: 
   - Use `DRAW_FLAG_DYNAMIC_RESOURCE_BUFFERS_INTACT` if constants unchanged
   - This skips the `Map`/`Unmap` overhead

---

## Vulkan Backend Implementation

### Architecture Overview

The Vulkan backend implements inline constants using **dynamic uniform buffers** for emulation, similar to the D3D11 backend. While Vulkan natively supports push constants, the current implementation uses uniform buffer emulation to support multiple inline constant resources and broader compatibility.

**Design Decision**: 
- True Vulkan push constants are reserved for SPIRV blocks explicitly marked with `layout(push_constant)` (future implementation)
- All inline constant resources currently use dynamic uniform buffer emulation
- This approach allows multiple inline constant resources (Vulkan only supports one push constant block per pipeline)

### Implementation Architecture

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                        User Application                                      │
├─────────────────────────────────────────────────────────────────────────────┤
│  pVariable->SetInlineConstants(pData, FirstConstant, NumConstants)          │
└──────────────────────────────────────────────────────────────────────────────┘
                                   │
                                   ▼
┌─────────────────────────────────────────────────────────────────────────────┐
│                      ShaderVariableManagerVk                                 │
│  ┌───────────────────────────────────────────────────────────────────────┐  │
│  │ SetInlineConstants():                                                  │  │
│  │   1. Get resource offset in merged push constant block                │  │
│  │   2. Copy data to push constant storage                               │  │
│  │   3. Mark push constants as dirty                                     │  │
│  └───────────────────────────────────────────────────────────────────────┘  │
└──────────────────────────────────┬──────────────────────────────────────────┘
       1                            │
                                   ▼ (During CommitShaderResources)
┌─────────────────────────────────────────────────────────────────────────────┐
│                         DeviceContextVkImpl                                  │
│  ┌───────────────────────────────────────────────────────────────────────┐  │
│  │ CommitPushConstants():                                                 │  │
│  │   vkCmdPushConstants(cmdBuffer, pipelineLayout,                       │  │
│  │                      stageFlags, offset, size, pData);                │  │
│  └───────────────────────────────────────────────────────────────────────┘  │
└─────────────────────────────────────────────────────────────────────────────┘
```

### Step 1: Resource Signature Creation

When creating a `PipelineResourceSignature` with `PIPELINE_RESOURCE_FLAG_INLINE_CONSTANTS`:

**File**: `PipelineResourceSignatureVkImpl.cpp` - `CreateSetLayouts()`

```cpp
if (ResDesc.Flags & PIPELINE_RESOURCE_FLAG_INLINE_CONSTANTS)
{
    VERIFY(ResDesc.ResourceType == SHADER_RESOURCE_TYPE_CONSTANT_BUFFER,
           "Only constant buffers can have INLINE_CONSTANTS flag");
    
    // Create internal dynamic uniform buffer for emulation
    InlineConstantBufferAttribsVk& InlineCBAttr = m_InlineConstantBuffers[InlineCBIdx++];
    InlineCBAttr.DescrSet     = DescrSetId;
    InlineCBAttr.BindingIndex = vkBinding;
    InlineCBAttr.NumConstants = ResDesc.ArraySize;  // Number of 32-bit values
    
    // Create a USAGE_DYNAMIC uniform buffer
    BufferDesc BuffDesc;
    BuffDesc.Name           = std::string(m_Desc.Name) + " - " + ResDesc.Name;
    BuffDesc.Size           = ResDesc.ArraySize * sizeof(Uint32);
    BuffDesc.Usage          = USAGE_DYNAMIC;
    BuffDesc.BindFlags      = BIND_UNIFORM_BUFFER;
    BuffDesc.CPUAccessFlags = CPU_ACCESS_WRITE;
    
    RefCntAutoPtr<IBuffer> pBuffer;
    GetDevice()->CreateBuffer(BuffDesc, nullptr, &pBuffer);
    InlineCBAttr.pBuffer = RefCntAutoPtr<BufferVkImpl>(pBuffer, IID_BufferVk);
}
```

**Key Points**:
- Each inline constant resource creates a `USAGE_DYNAMIC` uniform buffer
- Buffer is stored in `InlineConstantBufferAttribsVk` structure
- Buffer is shared across all SRBs created from this signature

### Step 2: Resource Cache Initialization

#### For Static Inline Constants

**File**: `PipelineResourceSignatureVkImpl.cpp` - `CreateSetLayouts()`

Static inline constants require CPU-side staging buffers that are allocated once during signature creation and freed in the destructor.

```cpp
// Calculate total memory size for all static inline constants
Uint32 TotalStaticInlineConstantSize = 0;
for (each static inline constant)
{
    TotalStaticInlineConstantSize += NumConstants * sizeof(Uint32);
}

// Allocate single memory block for all static inline constants
m_pStaticInlineConstantData = GetRawAllocator().Allocate(
    TotalStaticInlineConstantSize, "Static inline constant data", __FILE__, __LINE__);
memset(m_pStaticInlineConstantData, 0, TotalStaticInlineConstantSize);

// Assign memory to each static inline constant
Uint8* pCurrentDataPtr = static_cast<Uint8*>(m_pStaticInlineConstantData);
for (each static inline constant)
{
    m_pStaticResCache->InitializeInlineConstantBuffer(
        0, CacheOffset, NumConstants, pCurrentDataPtr);
    pCurrentDataPtr += DataSize;
}
```

#### For Mutable/Dynamic Inline Constants (SRB)

**File**: `PipelineResourceSignatureVkImpl.cpp` - `InitSRBResourceCache()`

```cpp
void PipelineResourceSignatureVkImpl::InitSRBResourceCache(ShaderResourceCacheVk& ResourceCache)
{
    // Initialize inline constant buffers
    if (m_NumInlineConstantBuffers > 0)
    {
        // Calculate total memory size
        Uint32 TotalInlineConstantSize = 0;
        for (Uint32 i = 0; i < m_NumInlineConstantBuffers; ++i)
        {
            TotalInlineConstantSize += m_InlineConstantBuffers[i].NumConstants * sizeof(Uint32);
        }
        
        // Allocate single memory block
        void* pInlineConstantMemory = CacheMemAllocator.Allocate(
            TotalInlineConstantSize, "Inline constant data", __FILE__, __LINE__);
        memset(pInlineConstantMemory, 0, TotalInlineConstantSize);
        
        // Pass ownership to resource cache for proper cleanup
        ResourceCache.SetInlineConstantMemory(CacheMemAllocator, pInlineConstantMemory);
        
        // Assign memory to each inline constant buffer
        Uint8* pCurrentDataPtr = static_cast<Uint8*>(pInlineConstantMemory);
        for (Uint32 i = 0; i < m_NumInlineConstantBuffers; ++i)
        {
            ResourceCache.InitializeInlineConstantBuffer(
                DescrSet, CacheOffset, NumConstants, pCurrentDataPtr);
            pCurrentDataPtr += InlineCBAttr.NumConstants * sizeof(Uint32);
        }
    }
    
    // Bind internal uniform buffers to the resource cache
    for (Uint32 i = 0; i < m_NumInlineConstantBuffers; ++i)
    {
        ResourceCache.SetResource(
            pLogicalDevice,  // nullptr for dynamic variables
            DescrSet, CacheOffset,
            {
                BindingIndex, 0, // ArrayIndex
                RefCntAutoPtr<IDeviceObject>{InlineCBAttr.pBuffer},
                0, // BufferBaseOffset
                InlineCBAttr.NumConstants * sizeof(Uint32) // BufferRangeSize
            });
    }
}
```

**Memory Management**:
- Static inline constants: Memory allocated via `GetRawAllocator()`, freed in `Destruct()`
- SRB inline constants: Memory allocated via `CacheMemAllocator`, owned by `ShaderResourceCacheVk` (freed in destructor via `m_pInlineConstantMemory`)

### Step 3: SetInlineConstants Implementation

**File**: `ShaderVariableManagerVk.cpp`

```cpp
void ShaderVariableManagerVk::SetInlineConstants(Uint32      ResIndex,
                                                 const void* pConstants,
                                                 Uint32      FirstConstant,
                                                 Uint32      NumConstants)
{
    const PipelineResourceAttribsVk& Attribs = m_pSignature->GetResourceAttribs(ResIndex);
    const ResourceCacheContentType   CacheType = m_ResourceCache.GetContentType();
    
#ifdef DILIGENT_DEVELOPMENT
    const PipelineResourceDesc& ResDesc = m_pSignature->GetResourceDesc(ResIndex);
    VerifyInlineConstants(ResDesc, pConstants, FirstConstant, NumConstants);
#endif
    
    m_ResourceCache.SetInlineConstants(
        Attribs.DescrSet,
        Attribs.CacheOffset(CacheType),
        pConstants,
        FirstConstant,
        NumConstants);
}
```

**File**: `ShaderResourceCacheVk.cpp`

```cpp
void ShaderResourceCacheVk::SetInlineConstants(Uint32      DescrSetIndex,
                                               Uint32      CacheOffset,
                                               const void* pConstants,
                                               Uint32      FirstConstant,
                                               Uint32      NumConstants)
{
    DescriptorSet& DescrSet = GetDescriptorSet(DescrSetIndex);
    Resource&      DstRes   = DescrSet.GetResource(CacheOffset);
    
    VERIFY(DstRes.pInlineConstantData != nullptr,
           "Inline constant data pointer is null.");
    
    // Copy to CPU-side staging buffer
    Uint32* pDstConstants = reinterpret_cast<Uint32*>(DstRes.pInlineConstantData);
    memcpy(pDstConstants + FirstConstant, pConstants, NumConstants * sizeof(Uint32));
    
    UpdateRevision();  // Mark cache as dirty
}
```

### Step 4: Updating GPU Buffer

During draw/dispatch commands, inline constant buffers are updated:

**File**: `DeviceContextVkImpl.cpp`

```cpp
void DeviceContextVkImpl::PrepareForDraw(DRAW_FLAGS Flags)
{
    // Update inline constant buffers before committing descriptor sets
    UpdateInlineConstantBuffers(BindInfo);
    
    // Commit descriptor sets
    CommitDescriptorSets(BindInfo, /* ... */);
}

void DeviceContextVkImpl::UpdateInlineConstantBuffers(ResourceBindInfo& BindInfo)
{
    for (Uint32 SignIdx = 0; SignIdx < BindInfo.ActiveSRBCount; ++SignIdx)
    {
        const auto& SignInfo = BindInfo.pSignatures[SignIdx];
        if (SignInfo.pSignature->GetNumInlineConstantBuffers() > 0)
        {
            SignInfo.pSignature->UpdateInlineConstantBuffers(
                *SignInfo.pResourceCache, *this);
        }
    }
}
```

**File**: `PipelineResourceSignatureVkImpl.cpp`

```cpp
void PipelineResourceSignatureVkImpl::UpdateInlineConstantBuffers(
    const ShaderResourceCacheVk& ResourceCache,
    DeviceContextVkImpl&         Ctx) const
{
    for (Uint32 i = 0; i < m_NumInlineConstantBuffers; ++i)
    {
        const InlineConstantBufferAttribsVk& InlineCBAttr = m_InlineConstantBuffers[i];
        
        // Get the inline constant data from the resource cache
        const void* pInlineConstantData = ResourceCache.GetInlineConstantData(
            InlineCBAttr.DescrSet, InlineCBAttr.BindingIndex);
        
        if (pInlineConstantData == nullptr)
            continue;
        
        // Map the buffer and copy the data
        void* pMappedData = nullptr;
        Ctx.MapBuffer(InlineCBAttr.pBuffer, MAP_WRITE, MAP_FLAG_DISCARD, pMappedData);
        if (pMappedData != nullptr)
        {
            memcpy(pMappedData, pInlineConstantData,
                   InlineCBAttr.NumConstants * sizeof(Uint32));
            Ctx.UnmapBuffer(InlineCBAttr.pBuffer, MAP_WRITE);
        }
    }
}
```

### Step 5: Handling ArraySize for ShaderResourceVariableDesc

When inline constants are specified via `ShaderResourceVariableDesc` (not `PipelineResourceDesc`), the `ArraySize` must be determined from SPIR-V:

**File**: `PipelineStateVkImpl.cpp` - `GetDefaultResourceSignatureDesc()`

```cpp
// For inline constants, ArraySize specifies the number of 32-bit constants,
// not the array dimension. Calculate it from the buffer size.
Uint32 ArraySize = Attribs.ArraySize;
if ((Flags & PIPELINE_RESOURCE_FLAG_INLINE_CONSTANTS) != 0)
{
    // For inline constants specified via ShaderResourceVariableDesc,
    // the SPIRV ArraySize is 1 (buffer is not an array), but we need
    // the number of 32-bit constants which is BufferStaticSize / sizeof(Uint32)
    if (Attribs.BufferStaticSize > 0)
    {
        ArraySize = Attribs.BufferStaticSize / sizeof(Uint32);
    }
    else
    {
        LOG_ERROR_AND_THROW("Resource '", Attribs.Name, "' is marked as inline constants, "
                            "but has zero buffer size.");
    }
}
```

### Multiple Inline Constants Support

Vulkan backend fully supports multiple inline constant buffers:

- Each inline constant resource creates its own `USAGE_DYNAMIC` uniform buffer
- Each has separate CPU-side staging buffer (`pInlineConstantData`)
- All inline constant buffers are updated together during `PrepareForDraw`/`PrepareForDispatch`

**Example**:

```cpp
PipelineResourceDesc Resources[] = {
    {"TransformCB", SHADER_TYPE_VERTEX, 16, SHADER_RESOURCE_TYPE_CONSTANT_BUFFER,
     SHADER_RESOURCE_VARIABLE_TYPE_STATIC, PIPELINE_RESOURCE_FLAG_INLINE_CONSTANTS},
    {"MaterialCB", SHADER_TYPE_PIXEL, 8, SHADER_RESOURCE_TYPE_CONSTANT_BUFFER,
     SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC, PIPELINE_RESOURCE_FLAG_INLINE_CONSTANTS},
};
```

This creates two separate dynamic uniform buffers.

### Memory Leak Prevention

The implementation carefully manages memory to prevent leaks:

1. **Static inline constants**: 
   - Single allocation in `CreateSetLayouts()` stored in `m_pStaticInlineConstantData`
   - Freed in `PipelineResourceSignatureVkImpl::Destruct()`

2. **SRB inline constants**:
   - Single allocation in `InitSRBResourceCache()` 
   - Ownership transferred to `ShaderResourceCacheVk::m_pInlineConstantMemory`
   - Automatically freed by `std::unique_ptr` in `~ShaderResourceCacheVk()`

### Performance Considerations

**Vulkan Inline Constants Performance**:

1. **Overhead**: Similar to D3D11 due to `Map`/`Unmap` operations
2. **Update frequency**: Best for per-draw updates
3. **Buffer sharing**: All SRBs share the same buffer, reducing memory
4. **Future optimization**: True push constants for `layout(push_constant)` blocks (planned)

---

## Static vs Mutable/Dynamic Variables

### Important Note on Static Variables

When using **static** inline constants, there's a critical distinction between Signature cache and SRB cache:

```cpp
// This updates the SIGNATURE's cache (for initialization)
pPRS->GetStaticVariableByName(SHADER_TYPE_VERTEX, "Constants")
    ->SetInlineConstants(pData, 0, 16);

// SRB creation copies from Signature cache to SRB cache
pPRS->CreateShaderResourceBinding(&pSRB, true);

// For per-frame updates, use the SRB's variable
pSRB->GetVariableByName(SHADER_TYPE_VERTEX, "Constants")
    ->SetInlineConstants(pNewData, 0, 16);
```

**Key Points**:
- Static variable bindings are copied from Signature to SRB during `CreateShaderResourceBinding(pSRB, true)`
- Updates to Signature's static variables after SRB creation do **not** propagate to existing SRBs
- For per-frame updates, always use the SRB's variable, not the Signature's

### Recommended Variable Types for Inline Constants

| Update Frequency | Recommended Variable Type |
|------------------|---------------------------|
| Once at init     | `STATIC`                  |
| Once per SRB     | `MUTABLE`                 |
| Per-frame/draw   | `DYNAMIC` or `MUTABLE`    |

---

## Performance Considerations

### D3D12

- Root constants are stored directly in the root signature, no indirection
- Each `SetGraphicsRoot32BitConstants` call updates constants immediately
- Optimal for small, frequently-updated data (< 64 constants / 256 bytes)
- Performanc: Fastest

### D3D11

- Emulated using `USAGE_DYNAMIC` constant buffers
- Requires `Map` + `memcpy` + `Unmap` for each update
- Buffer shared across all SRBs (reduces memory, increases update frequency)
- Use `DRAW_FLAG_DYNAMIC_RESOURCE_BUFFERS_INTACT` to skip updates when unchanged
- Performance: same as Constant Buffers, Moderate overhead due to Map/Unmap

### Vulkan

- Currently emulated using `USAGE_DYNAMIC` uniform buffers
- Requires `MapBuffer` + `memcpy` + `UnmapBuffer` for each update
- Buffer shared across all SRBs (reduces memory, increases update frequency)
- Performance: Similar to D3D11, moderate overhead due to Map/Unmap
- Future: True push constants for `layout(push_constant)` blocks (planned)

### Performance Comparison

| Backend | Implementation      | Update Cost | Best Use Case                    |
|---------|---------------------|-------------|----------------------------------|
| D3D12   | Root constants      | Very Low    | Per-draw, high-frequency updates |
| D3D11   | Dynamic CB + Map    | Moderate    | Per-draw, moderate frequency     |
| Vulkan  | Dynamic UB + Map    | Moderate    | Per-draw, moderate frequency     |

### General Guidelines

1. **Size**: Keep inline constants small (typically ≤ 128 bytes)
2. **Frequency**: Use for per-draw or per-dispatch data
3. **Fallback**: For larger data, use dynamic uniform buffers
4. **Multiple resources**: 
   - D3D12: Full support for multiple inline constants
   - D3D11: Full support for multiple inline constants (each as separate dynamic buffer)
   - Vulkan: Full support for multiple inline constants (each as separate dynamic uniform buffer)
5. **D3D11 specific**: Use `DRAW_FLAG_DYNAMIC_RESOURCE_BUFFERS_INTACT` when constants unchanged between draws

---

## References

- [D3D12 Root Signatures](https://docs.microsoft.com/en-us/windows/win32/direct3d12/root-signatures)
- [Vulkan Push Constants](https://docs.vulkan.org/guide/latest/push_constants.html)
- [GLSL Vulkan Extension (GL_KHR_vulkan_glsl)](https://github.com/KhronosGroup/GLSL/blob/master/extensions/khr/GL_KHR_vulkan_glsl.txt)

