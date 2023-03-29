/*  Copyright 2019-2023 Diligent Graphics LLC
 
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

#include <unordered_map>
#include <deque>
#include <mutex>
#include <memory>
#include <algorithm>
#include <atomic>

#include "../../../DiligentCore/Platforms/Basic/interface/DebugUtilities.hpp"

namespace Diligent
{

/// This class implements a thread-safe LRU cache.
///
/// Usage example:
///
///     struct CacheData
///     {
///         RefCntAutoPtr<IDataBlob> pData;
///     };
///     LRUCache<std::string, CacheData> Cache;
///     Cache.SetMaxSize(32768);
///     auto Data = Cache.Get("DataKey",
///                           [&](CacheData& Data, size_t& Size) //
///                           {
///                               Data.pData = ...;
///                               Size       = 1024;
///                           });
///
/// \note   The Get() method returns the data by value, as the copy kept by the cache
///         may be released immediately after the method finishes.
///
///         If the data is not found, it is atomically initialized by the provided initializer function.
///         If the data is found, the initializer function is not called.
template <typename KeyType, typename DataType, typename KeyHasher = std::hash<KeyType>>
class LRUCache
{
public:
    LRUCache() noexcept
    {}

    explicit LRUCache(size_t MaxSize) noexcept :
        m_MaxSize{MaxSize}
    {}

    /// Finds the data in the cache and returns it. If the data is not found, it is created using the provided initializer.
    template <typename InitDataType>
    DataType Get(const KeyType& Key, InitDataType InitData)
    {
        // Get the data wrapper. Since this is a shared pointer, it may not be
        // destroyed while we keep one, even if it is popped from the cache
        // by other thread.
        auto pDataWrpr = GetDataWrapper(Key);
        VERIFY_EXPR(pDataWrpr);

        // Get data by value. It will be atomically initialized if necessary,
        // while the main cache mutex will not be locked.
        bool     IsNewObject = false;
        DataType Data        = pDataWrpr->Get(InitData, IsNewObject);

        // Process release queue
        {
            std::lock_guard<std::mutex> Lock{m_Mtx};

            if (IsNewObject)
            {
                // NB: since we released the mutex, there is no guarantee that
                //     pDataWrpr is still in the cache as it could have been
                //     removed by other thread!
                auto it = m_Cache.find(Key);
                if (it != m_Cache.end())
                {
                    // Check that the object is the same (though another object should
                    // never be found in the cache as only objects with AccountedSize > 0 can
                    // be removed).
                    if (it->second == pDataWrpr)
                    {
                        // The wrapper has indeed been added to the cache - update its size.
                        m_CurrSize += pDataWrpr->DataSize;
                        // Only a single thread can write to AccountedSize as only a single thread can
                        // initialize the object and obtain IsNewObject = true.
                        VERIFY_EXPR(pDataWrpr->AccountedSize.load() == 0);
                        pDataWrpr->AccountedSize.store(pDataWrpr->DataSize);
                        // Note also that we write to AccountedSize while holding the cache
                        // mutex, so no other thread can read it, and the LRUQueue can not be
                        // processed at this time by another thread.
                    }
                }
            }

            while (m_CurrSize > m_MaxSize)
            {
                VERIFY_EXPR(!m_LRUQueue.empty());

                auto last_it = m_LRUQueue.back();

                const auto AccountedSize = last_it->second->AccountedSize.load();
                if (AccountedSize == 0)
                {
                    // The size of this wrapper has not been accounted for yet, which
                    // means another thread is waiting to update the cache size.
                    // Stop the loop - the queue will be processed again when the
                    // waiting thread gets unlocked.
                    break;
                }

                // Pop the last object from the queue
                m_LRUQueue.pop_back();
                VERIFY_EXPR(m_CurrSize >= AccountedSize);
                m_CurrSize -= AccountedSize;
                m_Cache.erase(last_it);
            }
            VERIFY_EXPR(m_Cache.size() == m_LRUQueue.size());
        }

        return Data;
    }

    /// Sets the maximum cache size.
    void SetMaxSize(size_t MaxSize)
    {
        m_MaxSize = MaxSize;
    }

    /// Returns the current cache size.
    size_t GetCurrSize() const
    {
        return m_CurrSize;
    }

    ~LRUCache()
    {
#ifdef DILIGENT_DEBUG
        size_t DbgSize = 0;
        VERIFY_EXPR(m_Cache.size() == m_LRUQueue.size());
        while (!m_LRUQueue.empty())
        {
            auto last_it = m_LRUQueue.back();
            m_LRUQueue.pop_back();
            VERIFY_EXPR(last_it->second->DataSize == last_it->second->AccountedSize);
            DbgSize += last_it->second->DataSize;
            m_Cache.erase(last_it);
        }
        VERIFY_EXPR(m_Cache.empty());
        VERIFY_EXPR(DbgSize == m_CurrSize);
#endif
    }

private:
    struct DataWrapper
    {
        template <typename InitDataType>
        const DataType& Get(InitDataType InitData, bool& IsNewObject)
        {
            std::lock_guard<std::mutex> Lock{InitDataMtx};
            IsNewObject = (DataSize == 0);
            if (IsNewObject)
            {
                InitData(Data, DataSize);
                VERIFY_EXPR(DataSize > 0);
                DataSize = (std::max)(DataSize, size_t{1});
            }
            return Data;
        }

        std::mutex InitDataMtx;
        DataType   Data;
        // Actual data size
        size_t DataSize = 0;
        // The size that was accounted for in the cache
        std::atomic<size_t> AccountedSize{0};
    };

    std::shared_ptr<DataWrapper> GetDataWrapper(const KeyType& Key)
    {
        std::lock_guard<std::mutex> Lock{m_Mtx};

        auto it = m_Cache.find(Key);
        if (it == m_Cache.end())
        {
            it = m_Cache.emplace(Key, std::make_shared<DataWrapper>()).first;
        }
        else
        {
            // Pop the wrapper iterator from the queue
            auto queue_it = std::find(m_LRUQueue.begin(), m_LRUQueue.end(), it);
            VERIFY_EXPR(queue_it != m_LRUQueue.end());
            m_LRUQueue.erase(queue_it);
        }

        // Move iterator to the front of the queue
        m_LRUQueue.push_front(it);
        VERIFY_EXPR(m_Cache.size() == m_LRUQueue.size());

        return it->second;
    }


    using CacheType = std::unordered_map<KeyType, std::shared_ptr<DataWrapper>, KeyHasher>;
    CacheType m_Cache;

    std::deque<typename CacheType::iterator> m_LRUQueue;

    std::mutex m_Mtx;

    std::atomic<size_t> m_CurrSize{0};
    std::atomic<size_t> m_MaxSize{0};
};

} // namespace Diligent
