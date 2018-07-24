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

// Helper class that handles free memory block management to accommodate variable-size allocation requests
// See http://diligentgraphics.com/diligent-engine/architecture/d3d12/variable-size-memory-allocations-manager/

#pragma once

/// \file
/// Implementation of Diligent::ResourceReleaseQueue class

#include <mutex>
#include <deque>
#include <memory>

#include "MemoryAllocator.h"
#include "STDAllocator.h"
#include "DebugUtilities.h"

namespace Diligent
{

/// Helper class that wraps stale resources of different types
class DynamicStaleResourceWrapper
{
public:
    //   ___________________________                                  ___________________________
    //  |DynamicStaleResourceWrapper|                                |DynamicStaleResourceWrapper|
    //  |                           |                                |                           |
    //  |   m_pStaleResource        |                                |   m_pStaleResource        |
    //  |__________|________________|                                |__________|________________|
    //             |                                                            |
    //             |                                                            |
    //             |                                                            |
    //   __________V___________________________________               __________V___________________________________
    //  |SpecificStaleResource<VulkanBufferWrapper>    |             |SpecificStaleResource<VulkanMemoryAllocation> |
    //  |                                              |             |                                              |
    //  |  VulkanBufferWrapper m_SpecificResource;     |             |  VulkanMemoryAllocation m_SpecificResource;  |
    //  |______________________________________________|             |______________________________________________|
    //

    template<typename ResourceType>
    static DynamicStaleResourceWrapper Create(ResourceType&& Resource)
    {
        class SpecificStaleResource : public StaleResourceBase
        {
        public:
            SpecificStaleResource(ResourceType&& SpecificResource) :
                m_SpecificResource(std::move(SpecificResource))
            {}

            SpecificStaleResource             (const SpecificStaleResource&) = delete;
            SpecificStaleResource             (SpecificStaleResource&&)      = delete;
            SpecificStaleResource& operator = (const SpecificStaleResource&) = delete;
            SpecificStaleResource& operator = (SpecificStaleResource&&)      = delete;

        private:
            ResourceType m_SpecificResource;
        };
        return DynamicStaleResourceWrapper{new SpecificStaleResource{std::move(Resource)}};
    }

    DynamicStaleResourceWrapper(DynamicStaleResourceWrapper&& rhs)noexcept :
        m_pStaleResource(std::move(rhs.m_pStaleResource))
    {}

    DynamicStaleResourceWrapper& operator = (DynamicStaleResourceWrapper&& rhs)noexcept
    {
        m_pStaleResource = std::move(rhs.m_pStaleResource);
        return *this;
    }

    DynamicStaleResourceWrapper             (const DynamicStaleResourceWrapper&) = delete;
    DynamicStaleResourceWrapper& operator = (const DynamicStaleResourceWrapper&) = delete;

private:
    class StaleResourceBase
    {
    public:
        virtual ~StaleResourceBase() = 0 {}
    };

    DynamicStaleResourceWrapper(StaleResourceBase *pStaleResource) :
        m_pStaleResource(pStaleResource)
    {}

    std::unique_ptr<StaleResourceBase> m_pStaleResource;
};

/// Helper class that wraps stale resources of the same type
template<typename ResourceType>
class StaticStaleResourceWrapper
{
public:
    static StaticStaleResourceWrapper Create(ResourceType&& Resource)
    {
        return StaticStaleResourceWrapper{std::move(Resource)};
    }

    StaticStaleResourceWrapper(StaticStaleResourceWrapper&& rhs)noexcept :
        m_StaleResource(std::move(rhs.m_StaleResource))
    {}

    StaticStaleResourceWrapper& operator = (StaticStaleResourceWrapper&& rhs)noexcept
    {
        m_StaleResource = std::move(rhs.m_StaleResource);
        return *this;
    }

    StaticStaleResourceWrapper             (const StaticStaleResourceWrapper&) = delete;
    StaticStaleResourceWrapper& operator = (const StaticStaleResourceWrapper&) = delete;


private:
    StaticStaleResourceWrapper(ResourceType&& StaleResource) : 
        m_StaleResource(std::move(StaleResource))
    {}

    ResourceType m_StaleResource;
};

/// Facilitates safe resource destruction in D3D12 and Vulkan

/// Resource destruction is a two-stage process:
/// * When resource is released, it is moved into the stale objects queue along with the next command list number
/// * When command list is submitted to the command queue, all stale objects associated with this
///   and earlier command lists are moved to the release queue, along with the fence value associated with
///   the command list
/// * Resources are removed and actually destroyed from the queue when fence is signaled and the queue is Purged
///
/// \tparam ResourceWrapperType -  Type of the resource wrapper used by the release queue.
template<typename ResourceWrapperType>
class ResourceReleaseQueue
{
public:
    ResourceReleaseQueue(IMemoryAllocator& Allocator) : 
        m_ReleaseQueue(STD_ALLOCATOR_RAW_MEM(ReleaseQueueElemType, Allocator, "Allocator for deque<ReleaseQueueElemType>")),
        m_StaleResources(STD_ALLOCATOR_RAW_MEM(ReleaseQueueElemType, Allocator, "Allocator for deque<ReleaseQueueElemType>"))
    {}

    ~ResourceReleaseQueue()
    {
        VERIFY(m_StaleResources.empty(), "Not all stale objects were destroyed");
        VERIFY(m_ReleaseQueue.empty(), "Release queue is not empty");
    }

    /// Moves resource to the release queue
    /// \param [in] Resource              - Resource to be released
    /// \param [in] NextCommandListNumber - Number of the command list that will be submitted to the queue next
    template<typename ResourceType>
    void SafeReleaseResource(ResourceType&& Resource, Uint64 NextCommandListNumber)
    {
        std::lock_guard<std::mutex> LockGuard(m_StaleObjectsMutex);
        m_StaleResources.emplace_back(NextCommandListNumber, ResourceWrapperType::Create(std::move(Resource)) );
    }


    /// Moves stale objects to the release queue
    /// \param [in] SubmittedCmdBuffNumber - number of the last submitted command list. 
    ///                                      All resources in the stale object list whose command list number is
    ///                                      less than or equal to this value are moved to the release queue.
    /// \param [in] FenceValue             - Fence value associated with the resources moved to the release queue.
    ///                                      A resource will be destroyed by Purge() method when completed fence value
    ///                                      is greater or equal to the fence value associated with the resource
    void DiscardStaleResources(Uint64 SubmittedCmdBuffNumber, Uint64 FenceValue)
    {
        // Only discard these stale objects that were released before CmdBuffNumber
        // was executed
        std::lock_guard<std::mutex> StaleObjectsLock(m_StaleObjectsMutex);
        std::lock_guard<std::mutex> ReleaseQueueLock(m_ReleaseQueueMutex);
        while (!m_StaleResources.empty() )
        {
            auto &FirstStaleObj = m_StaleResources.front();
            if (FirstStaleObj.first <= SubmittedCmdBuffNumber)
            {
                m_ReleaseQueue.emplace_back(FenceValue, std::move(FirstStaleObj.second));
                m_StaleResources.pop_front();
            }
            else 
                break;
        }
    }


    /// Removes all objects from the release queue whose fence value is
    /// less than or equal to CompletedFenceValue
    /// \param [in] CompletedFenceValue  -  Value of the fence that has been completed by the GPU
    void Purge(Uint64 CompletedFenceValue)
    {
        std::lock_guard<std::mutex> LockGuard(m_ReleaseQueueMutex);

        // Release all objects whose associated fence value is at most CompletedFenceValue
        // See http://diligentgraphics.com/diligent-engine/architecture/d3d12/managing-resource-lifetimes/
        while (!m_ReleaseQueue.empty())
        {
            auto &FirstObj = m_ReleaseQueue.front();
            if (FirstObj.first <= CompletedFenceValue)
                m_ReleaseQueue.pop_front();
            else
                break;
        }
    }
    
    /// Returns the number of stale resources
    size_t GetStaleResourceCount()const
    {
        return m_StaleResources.size();
    }

    /// Returns the number of resources pending release
    size_t GetPendingReleaseResourceCount()const
    {
        return m_ReleaseQueue.size();
    }

private:

    std::mutex m_ReleaseQueueMutex;
    using ReleaseQueueElemType = std::pair<Uint64, ResourceWrapperType>;
    std::deque< ReleaseQueueElemType, STDAllocatorRawMem<ReleaseQueueElemType> > m_ReleaseQueue;

    std::mutex m_StaleObjectsMutex;
    std::deque< ReleaseQueueElemType, STDAllocatorRawMem<ReleaseQueueElemType> > m_StaleResources;
};

}
