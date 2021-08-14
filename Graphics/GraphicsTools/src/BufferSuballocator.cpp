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

#include "BufferSuballocator.h"

#include <mutex>
#include <atomic>

#include "DebugUtilities.hpp"
#include "ObjectBase.hpp"
#include "RefCntAutoPtr.hpp"
#include "DynamicBuffer.hpp"
#include "VariableSizeAllocationsManager.hpp"
#include "Align.hpp"
#include "DefaultRawMemoryAllocator.hpp"
#include "FixedBlockMemoryAllocator.hpp"

namespace Diligent
{

class BufferSuballocatorImpl;

class BufferSuballocationImpl final : public ObjectBase<IBufferSuballocation>
{
public:
    using TBase = ObjectBase<IBufferSuballocation>;
    BufferSuballocationImpl(IReferenceCounters*                          pRefCounters,
                            BufferSuballocatorImpl*                      pParentAllocator,
                            Uint32                                       Offset,
                            Uint32                                       Size,
                            VariableSizeAllocationsManager::Allocation&& Subregion) :
        // clang-format off
        TBase             {pRefCounters},
        m_pParentAllocator{pParentAllocator},
        m_Subregion       {std::move(Subregion)},
        m_Offset          {Offset},
        m_Size            {Size}
    // clang-format on
    {
        VERIFY_EXPR(m_pParentAllocator);
        VERIFY_EXPR(m_Subregion.IsValid());
    }

    ~BufferSuballocationImpl();

    IMPLEMENT_QUERY_INTERFACE_IN_PLACE(IID_BufferSuballocation, TBase)

    virtual Atomics::Long DILIGENT_CALL_TYPE Release() override final
    {
        RefCntAutoPtr<BufferSuballocatorImpl> pParent;
        return TBase::Release(
            [&]() //
            {
                // We must keep parent alive while this object is being destroyed because
                // the parent keeps the memory allocator for the object.
                pParent = m_pParentAllocator;
            });
    }

    virtual Uint32 GetOffset() const override final
    {
        return m_Offset;
    }

    virtual Uint32 GetSize() const override final
    {
        return m_Size;
    }

    virtual IBufferSuballocator* GetAllocator() override final;

    virtual void SetUserData(IObject* pUserData) override final
    {
        m_pUserData = pUserData;
    }

    virtual IObject* GetUserData() const override final
    {
        return m_pUserData.RawPtr<IObject>();
    }

private:
    RefCntAutoPtr<BufferSuballocatorImpl> m_pParentAllocator;

    VariableSizeAllocationsManager::Allocation m_Subregion;

    const Uint32 m_Offset;
    const Uint32 m_Size;

    RefCntAutoPtr<IObject> m_pUserData;
};

class BufferSuballocatorImpl final : public ObjectBase<IBufferSuballocator>
{
public:
    using TBase = ObjectBase<IBufferSuballocator>;

    IMPLEMENT_QUERY_INTERFACE_IN_PLACE(IID_BufferSuballocator, TBase)

    BufferSuballocatorImpl(IReferenceCounters*                 pRefCounters,
                           IRenderDevice*                      pDevice,
                           const BufferSuballocatorCreateInfo& CreateInfo) :
        // clang-format off
        TBase                    {pRefCounters},
        m_Mgr                    {CreateInfo.Desc.uiSizeInBytes, DefaultRawMemoryAllocator::GetAllocator()},
        m_Buffer                 {pDevice, CreateInfo.Desc},
        m_ExpansionSize          {CreateInfo.ExpansionSize},
        m_SuballocationsAllocator
        {
            DefaultRawMemoryAllocator::GetAllocator(),
            sizeof(BufferSuballocationImpl),
            CreateInfo.SuballocationObjAllocationGranularity
        }
    // clang-format on
    {}

    ~BufferSuballocatorImpl()
    {
        VERIFY_EXPR(m_AllocationCount.load() == 0);
    }

    virtual IBuffer* GetBuffer(IRenderDevice* pDevice, IDeviceContext* pContext) override final
    {
        Uint32 Size = 0;
        {
            std::lock_guard<std::mutex> Lock{m_MgrMtx};
            Size = static_cast<Uint32>(m_Mgr.GetMaxSize());
        }
        if (Size != m_Buffer.GetDesc().uiSizeInBytes)
        {
            m_Buffer.Resize(pDevice, pContext, Size);
        }

        return m_Buffer.GetBuffer(pDevice, pContext);
    }

    virtual void Allocate(Uint32                 Size,
                          Uint32                 Alignment,
                          IBufferSuballocation** ppSuballocation) override final
    {
        if (Size == 0)
        {
            UNEXPECTED("Size must not be zero");
            return;
        }

        if (!IsPowerOfTwo(Alignment))
        {
            UNEXPECTED("Alignment (", Alignment, ") is not a power of two");
            return;
        }

        VariableSizeAllocationsManager::Allocation Subregion;
        {
            std::lock_guard<std::mutex> Lock{m_MgrMtx};
            Subregion = m_Mgr.Allocate(Size, Alignment);

            while (!Subregion.IsValid())
            {
                auto ExtraSize = m_ExpansionSize != 0 ?
                    std::max(m_ExpansionSize, AlignUp(Size, Alignment)) :
                    m_Mgr.GetMaxSize();

                m_Mgr.Extend(ExtraSize);
                Subregion = m_Mgr.Allocate(Size, Alignment);
            }
        }

        // clang-format off
        BufferSuballocationImpl* pSuballocation{
            NEW_RC_OBJ(m_SuballocationsAllocator, "BufferSuballocationImpl instance", BufferSuballocationImpl)
            (
                this,
                AlignUp(static_cast<Uint32>(Subregion.UnalignedOffset), Alignment),
                Size,
                std::move(Subregion)
            )
        };
        // clang-format on

        pSuballocation->QueryInterface(IID_BufferSuballocation, reinterpret_cast<IObject**>(ppSuballocation));
        m_AllocationCount.fetch_add(1);
    }

    void Free(VariableSizeAllocationsManager::Allocation&& Subregion)
    {
        std::lock_guard<std::mutex> Lock{m_MgrMtx};
        m_Mgr.Free(std::move(Subregion));
        m_AllocationCount.fetch_add(-1);
    }

    virtual Uint32 GetVersion() const override final
    {
        return m_Buffer.GetVersion();
    }

    virtual void GetUsageStats(BufferSuballocatorUsageStats& UsageStats) override final
    {
        std::lock_guard<std::mutex> Lock{m_MgrMtx};
        UsageStats.Size             = static_cast<Uint32>(m_Mgr.GetMaxSize());
        UsageStats.UsedSize         = static_cast<Uint32>(m_Mgr.GetUsedSize());
        UsageStats.MaxFreeChunkSize = static_cast<Uint32>(m_Mgr.GetMaxFreeBlockSize());
        UsageStats.AllocationCount  = m_AllocationCount.load();
    }

private:
    std::mutex                     m_MgrMtx;
    VariableSizeAllocationsManager m_Mgr;

    DynamicBuffer m_Buffer;

    const Uint32 m_ExpansionSize;

    std::atomic<Int32> m_AllocationCount{0};

    FixedBlockMemoryAllocator m_SuballocationsAllocator;
};


BufferSuballocationImpl::~BufferSuballocationImpl()
{
    m_pParentAllocator->Free(std::move(m_Subregion));
}

IBufferSuballocator* BufferSuballocationImpl::GetAllocator()
{
    return m_pParentAllocator;
}


void CreateBufferSuballocator(IRenderDevice*                      pDevice,
                              const BufferSuballocatorCreateInfo& CreateInfo,
                              IBufferSuballocator**               ppBufferSuballocator)
{
    try
    {
        auto* pAllocator = MakeNewRCObj<BufferSuballocatorImpl>()(pDevice, CreateInfo);
        pAllocator->QueryInterface(IID_BufferSuballocator, reinterpret_cast<IObject**>(ppBufferSuballocator));
    }
    catch (...)
    {
        LOG_ERROR_MESSAGE("Failed to create buffer suballocator");
    }
}

} // namespace Diligent
