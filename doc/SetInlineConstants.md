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

The Vulkan backend now supports **two** inline constant paths:

- `PIPELINE_RESOURCE_FLAG_INLINE_CONSTANTS` + `PIPELINE_RESOURCE_FLAG_VULKAN_PUSH_CONSTANT` → true push constants backed by `vkCmdPushConstants`.
- `PIPELINE_RESOURCE_FLAG_INLINE_CONSTANTS` without the push-constant flag → emulated via internal `USAGE_DYNAMIC` uniform buffers (same behavior as D3D11).

This split lets us mix SPIR-V `layout(push_constant)` blocks with regular inline constant buffers in a single signature while still supporting multiple inline constant resources per pipeline.

### Implementation Architecture

```
┌────────────────────────────────────────────────────────────────────────────┐
│                    ShaderVariableManagerVk                                 │
│  SetInlineConstants():                                                     │
│    if resource has PIPELINE_RESOURCE_FLAG_VULKAN_PUSH_CONSTANT             │
│        ├─ STATIC var  -> copy into InlineCBAttr.pPushConstantData          │
│        └─ SRB cache   -> write to ShaderResourceCacheVk push constant slot │
│    else                                                                    │
│        └─ ShaderResourceCacheVk::SetInlineConstants(...) (CPU staging)     │
└────────────────────────────────────────────────────────────────────────────┘
                │                                               │
                │                          ┌────────────────────┘
                ▼ (Bind SRB)               ▼ (Bind SRB with emulation)
┌────────────────────────────────────────────────────────────────────────────┐
│            DeviceContextVkImpl::UpdateInlineConstantBuffers                │
│    for each InlineConstantBufferAttribsVk                                  │
│        if IsPushConstant                                                   │
│            -> ctx.SetPushConstants(pData, 0, Size)                         │
│        else                                                                │
│            -> Map internal uniform buffer, memcpy, Unmap                   │
└────────────────────────────────────────────────────────────────────────────┘
                │
                ▼
┌────────────────────────────────────────────────────────────────────────────┐
│            DeviceContextVkImpl::CommitPushConstants                        │
│    vkCmdPushConstants(vkPipelineLayout, StageFlags, 0, Size, pCpuData)     │
└────────────────────────────────────────────────────────────────────────────┘
```

### Step 1: Resource Signature Creation

**SPIR-V reflection → default signature**  
`PipelineStateVkImpl::GetDefaultResourceSignatureDesc()` converts SPIR-V resources into `PipelineResourceDesc`. When an attribute reports inline constants, the code derives the **number of 32-bit values** from `BufferStaticSize` instead of the logical array size, and SPIR-V `push_constant` resources propagate `PIPELINE_RESOURCE_FLAG_VULKAN_PUSH_CONSTANT` (see `Graphics/GraphicsEngineVulkan/src/PipelineStateVkImpl.cpp:592-666`).

**Push constant metadata for the pipeline layout**  
`PipelineStateVkImpl::InitPushConstantInfo()` walks every shader and records the max push constant size and the union of stage flags. `PipelineLayoutVk::Create()` stores that information so that `vkCmdPushConstants` can be issued later (`Graphics/GraphicsEngineVulkan/src/PipelineLayoutVk.cpp:62-164`).

**InlineConstantBufferAttribsVk initialization**  
Inside `PipelineResourceSignatureVkImpl::CreateSetLayouts()` every inline constant resource gets an `InlineConstantBufferAttribsVk` entry (`Graphics/GraphicsEngineVulkan/src/PipelineResourceSignatureVkImpl.cpp:200-441`):

- `IsPushConstant` mirrors `PIPELINE_RESOURCE_FLAG_VULKAN_PUSH_CONSTANT`.
- Push constants skip descriptor-set bookkeeping altogether; emulated inline constants allocate a `USAGE_DYNAMIC` uniform buffer that is shared by all SRBs originating from the signature.
- Static push constants allocate CPU-side storage (`pPushConstantData`) inside `m_pStaticInlineConstantData`.

### Step 2: Resource Cache Initialization

`PipelineResourceSignatureVkImpl::InitSRBResourceCache()` now sets up both flavors (`Graphics/GraphicsEngineVulkan/src/PipelineResourceSignatureVkImpl.cpp:653-739`):

- A single block of CPU memory is allocated for all inline constants and the pointer is owned by `ShaderResourceCacheVk::m_pInlineConstantMemory`.
- Push-constant resources register their per-SRB staging pointers via `ShaderResourceCacheVk::InitializePushConstantDataPtrs()` / `SetPushConstantDataPtr()` so that each SRB maintains its own copy.
- Emulated inline constants call `InitializeInlineConstantBuffer()` to hook the CPU staging memory into the descriptor-set resource slots and bind the internal dynamic buffer through `SetResource()`.

Static inline constants follow the same pattern during `CreateSetLayouts()` (lines `576-626`), so signatures keep zeroed data for static variables and copy it into SRBs later.

### Step 3: ShaderVariableManagerVk::SetInlineConstants

`ShaderVariableManagerVk::SetInlineConstants()` branches on the new flag (`Graphics/GraphicsEngineVulkan/src/ShaderVariableManagerVk.cpp:654-725`):

```
if (IsPushConstant)
{
    if (CacheType == ResourceCacheContentType::Signature)
    {
        // Static push constants live inside InlineCBAttr.pPushConstantData.
    }
    else
    {
        // SRB cache owns per-instance push constant storage.
        void* pDst = m_ResourceCache.GetPushConstantDataPtr(PushConstantBufferIdx);
    }
    memcpy(...); // copy 32-bit values
    return;
}

const Uint32 CacheOffset = Attribs.CacheOffset(CacheType);
m_ResourceCache.SetInlineConstants(Attribs.DescrSet, CacheOffset, pConstants,
                                   FirstConstant, NumConstants);
```

Emulated inline constants therefore keep using `ShaderResourceCacheVk::SetInlineConstants`, while push constants never touch descriptor sets.

### Step 4: Updating GPU State

`PipelineResourceSignatureVkImpl::UpdateInlineConstantBuffers()` now understands both data paths (`Graphics/GraphicsEngineVulkan/src/PipelineResourceSignatureVkImpl.cpp:1309-1345`):

```
if (InlineCBAttr.IsPushConstant)
{
    const void* pData = ResourceCache.GetPushConstantDataPtr(PushConstantBufferIdx++);
    Ctx.SetPushConstants(pData, 0, DataSize);
}
else if (InlineCBAttr.pBuffer)
{
    void* pMappedData = nullptr;
    Ctx.MapBuffer(InlineCBAttr.pBuffer, MAP_WRITE, MAP_FLAG_DISCARD, pMappedData);
    memcpy(pMappedData, pInlineConstantData, DataSize);
    Ctx.UnmapBuffer(InlineCBAttr.pBuffer, MAP_WRITE);
}
```

- `DeviceContextVkImpl::SetPushConstants()` copies the data into an internal `std::array<Uint8, DILIGENT_MAX_PUSH_CONSTANTS_SIZE>` buffer and marks it dirty (`Graphics/GraphicsEngineVulkan/src/DeviceContextVkImpl.cpp:454-480`).
- After descriptor sets are committed, `CommitPushConstants()` checks whether the current pipeline layout exposes push constants and issues `vkCmdPushConstants` with the stored stage flags (`Graphics/GraphicsEngineVulkan/src/DeviceContextVkImpl.cpp:482-501`). Draw, dispatch, and trace entry points call `CommitPushConstants()` right after `CommitDescriptorSets`.
- SRBs that only contain push constants no longer trigger descriptor-set binding (`CommitDescriptorSets()` filters them out at lines `503-521`).

### Step 5: Example Usage

```
PipelineResourceDesc Resources[] = {
    // True push constant block declared with layout(push_constant)
    {"PerFramePC", SHADER_TYPE_VERTEX | SHADER_TYPE_PIXEL, 32,
     SHADER_RESOURCE_TYPE_CONSTANT_BUFFER, SHADER_RESOURCE_VARIABLE_TYPE_STATIC,
     PIPELINE_RESOURCE_FLAG_INLINE_CONSTANTS | PIPELINE_RESOURCE_FLAG_VULKAN_PUSH_CONSTANT},

    // Emulated inline constant buffer bound through a uniform buffer
    {"PerDrawInlineCB", SHADER_TYPE_VERTEX, 16,
     SHADER_RESOURCE_TYPE_CONSTANT_BUFFER, SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC,
     PIPELINE_RESOURCE_FLAG_INLINE_CONSTANTS},
};
```

`PerFramePC` is copied directly with `vkCmdPushConstants`, while `PerDrawInlineCB` maps the internal dynamic uniform buffer each time `UpdateInlineConstantBuffers()` runs.

### Memory Management

1. **Static inline constants / push constants**  
   - Single allocation in `CreateSetLayouts()` stored inside `m_pStaticInlineConstantData`.  
   - Freed by `PipelineResourceSignatureVkImpl::Destruct()`.  
   - Push constants also stash the pointer inside `InlineConstantBufferAttribsVk::pPushConstantData`.

2. **SRB inline constants**  
   - Single block per SRB, allocated in `InitSRBResourceCache()` and owned by `ShaderResourceCacheVk::m_pInlineConstantMemory`.  
   - Push constants additionally keep an array of pointers (`m_pPushConstantDataPtrs`) managed by `InitializePushConstantDataPtrs()` / `SetPushConstantDataPtr()`.

### Performance Considerations

- Push constants avoid the `Map`/`Unmap` cost entirely—`DeviceContextVkImpl` simply memcpy's into a small CPU buffer before issuing `vkCmdPushConstants`.
- Emulated inline constants still incur a map/discard write, so they are best suited for larger constant blocks or when more than one inline constant buffer is needed.
- SRBs sharing the internal uniform buffers keeps memory usage small even when every SRB uses emulation.

---


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
- Performance: Fastest

### D3D11

- Emulated using `USAGE_DYNAMIC` constant buffers
- Requires `Map` + `memcpy` + `Unmap` for each update
- Buffer shared across all SRBs (reduces memory, increases update frequency)
- Use `DRAW_FLAG_DYNAMIC_RESOURCE_BUFFERS_INTACT` to skip updates when unchanged
- Performance: same as Constant Buffers, Moderate overhead due to Map/Unmap

### Vulkan

- Resources flagged with `PIPELINE_RESOURCE_FLAG_VULKAN_PUSH_CONSTANT` use `vkCmdPushConstants` (no descriptor binding, no mapping).
- All other inline constants are emulated through internal `USAGE_DYNAMIC` uniform buffers that are shared by every SRB.
- Map/Unmap is only required for the emulated path; push constants merely copy into a small CPU buffer right before `vkCmdPushConstants`.
- Performance: push-constant path is comparable to D3D12 root constants, emulated path matches the D3D11 behavior.

### Performance Comparison

| Backend | Implementation                      | Update Cost    |
|---------|-------------------------------------|----------------|
| D3D12   | Root constants                      | Very Low       |
| D3D11   | Dynamic CB + Map                   | Moderate       |
| Vulkan  | Push constants / Dynamic UB + Map  | Low / Moderate |

### General Guidelines

1. **Size**: Keep inline constants small (typically <= 128 bytes)
2. **Frequency**: Use for per-draw or per-dispatch data
3. **Fallback**: For larger data, use dynamic uniform buffers
4. **Multiple resources**: 
   - D3D12: Full support for multiple inline constants
   - D3D11: Full support for multiple inline constants (emulated with separate dynamic buffer)
   - Vulkan: One push constant block per pipeline + unlimited emulated inline buffers via dynamic uniform buffers

---

## References

- [D3D12 Root Signatures](https://docs.microsoft.com/en-us/windows/win32/direct3d12/root-signatures)
- [Vulkan Push Constants](https://docs.vulkan.org/guide/latest/push_constants.html)
- [GLSL Vulkan Extension (GL_KHR_vulkan_glsl)](https://github.com/KhronosGroup/GLSL/blob/master/extensions/khr/GL_KHR_vulkan_glsl.txt)

