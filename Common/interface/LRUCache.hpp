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
#include <vector>

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

    /// Finds the data in the cache and returns it. If the data is not found, it is atomically created using the provided initializer.
    template <typename InitDataType>
    DataType Get(const KeyType& Key, InitDataType&& InitData) noexcept(false) // InitData may throw
    {
        // Get the data wrapper. Since this is a shared pointer, it may not be
        // destroyed while we keep one, even if it is popped from the cache
        // by other thread.
        auto pDataWrpr = GetDataWrapper(Key);
        VERIFY_EXPR(pDataWrpr);

        // Get data by value. It will be atomically initialized if necessary,
        // while the main cache mutex is not locked.
        bool IsNewObject = false;
        // InitData may throw, which will leave the wrapper in the cache in the 'InitFailure' state.
        // It will be removed from the cache later when the LRU queue is processed.
        auto Data = pDataWrpr->GetData(std::forward<InitDataType>(InitData), IsNewObject);

        // Process the release queue
        std::vector<std::shared_ptr<DataWrapper>> DeleteList;
        {
            std::lock_guard<std::mutex> Lock{m_Mtx};

            if (IsNewObject)
            {
                VERIFY_EXPR(pDataWrpr->GetState() == DataWrapper::DataState::InitializedUnaccounted);

                // NB: since we released the cache mutex, there is no guarantee that
                //     pDataWrpr is still in the cache as it could have been
                //     removed by other thread!
                auto it = m_Cache.find(Key);
                if (it != m_Cache.end())
                {
                    // Check that the object wrapper is the same.
                    if (it->second == pDataWrpr)
                    {
                        // Only a single thread can initialize accounted size as only a single thread can
                        // initialize the object and obtain IsNewObject == true.
                        pDataWrpr->InitAccountedSize();

                        // The wrapper has indeed been added to the cache - update the cache size.
                        m_CurrSize += pDataWrpr->GetAccountedSize();
                        // Note that since we hold the mutex, no other thread can access the
                        // LRUQueue and remove this wrapper from the cache.
                    }
                }
            }

            for (int idx = static_cast<int>(m_LRUQueue.size()) - 1; idx >= 0; --idx)
            {
                if (m_CurrSize <= m_MaxSize)
                    break;

                VERIFY_EXPR(!m_LRUQueue.empty());

                const auto& cache_it = m_LRUQueue[idx];
                const auto  State    = cache_it->second->GetState();
                if (State == DataWrapper::DataState::Default)
                {
                    // The object is being initialized in another thread.
                    continue;
                }
                if (State == DataWrapper::DataState::InitializedUnaccounted)
                {
                    // Object has been initialized in another thread, but has not been accounted for
                    // in the cache yet as this thread acquired the mutex first.
                    continue;
                }

                VERIFY_EXPR(State == DataWrapper::DataState::InitializedAccounted ||
                            State == DataWrapper::DataState::InitFailure);
                // Note that for data wrappers in InitFailure state, the accounted size
                // can't be updated since we hold the mutex. Even if other thread successfully
                // initializes the wrapper, it will not be in the cache by that time,
                // and the wrapper will be discarded.
                const auto AccountedSize = cache_it->second->GetAccountedSize();
                DeleteList.emplace_back(std::move(cache_it->second));
                m_Cache.erase(cache_it);
                m_LRUQueue.erase(m_LRUQueue.begin() + idx);
                VERIFY_EXPR(m_CurrSize >= AccountedSize);
                m_CurrSize -= AccountedSize;
            }
            VERIFY_EXPR(m_Cache.size() == m_LRUQueue.size());
        }

        // Delete objects after releasing the cache mutex
        DeleteList.clear();

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
            DbgSize += last_it->second->GetAccountedSize();
            m_Cache.erase(last_it);
        }
        VERIFY_EXPR(m_Cache.empty());
        VERIFY_EXPR(DbgSize == m_CurrSize);
#endif
    }

private:
    class DataWrapper
    {
    public:
        enum class DataState
        {
            InitFailure = -1,
            Default,
            InitializedUnaccounted,
            InitializedAccounted
        };

        template <typename InitDataType>
        const DataType& GetData(InitDataType&& InitData, bool& IsNewObject) noexcept(false)
        {
            std::lock_guard<std::mutex> Lock{m_InitDataMtx};
            if (m_DataSize == 0)
            {
                m_State.store(DataState::Default);
                try
                {
                    size_t DataSize = 0;
                    InitData(m_Data, DataSize); // May throw
                    VERIFY_EXPR(DataSize > 0);
                    m_DataSize.store((std::max)(DataSize, size_t{1}));
                    m_State.store(DataState::InitializedUnaccounted);
                    IsNewObject = true;
                }
                catch (...)
                {
                    m_Data = {};
                    m_State.store(DataState::InitFailure);
                    throw;
                }
            }
            else
            {
                VERIFY_EXPR(m_State == DataState::InitializedUnaccounted || m_State == DataState::InitializedAccounted);
            }
            return m_Data;
        }

        void InitAccountedSize()
        {
            VERIFY(m_State == DataState::InitializedUnaccounted, "Initializing accounted size for an object that is not initialized.");
            VERIFY(m_AccountedSize == 0, "Accounted size has already been initialized.");
            m_AccountedSize.store(m_DataSize.load());
            m_State.store(DataState::InitializedAccounted);
        }

        size_t GetAccountedSize() const
        {
            VERIFY_EXPR((m_State == DataState::InitFailure && m_AccountedSize == 0) ||
                        (m_State == DataState::InitializedAccounted && m_AccountedSize != 0));
            return m_AccountedSize.load();
        }

        DataState GetState() const { return m_State; }

    private:
        std::mutex m_InitDataMtx;
        DataType   m_Data;

        std::atomic<DataState> m_State{DataState::Default};

        std::atomic<size_t> m_DataSize{0};
        // The size that was accounted in the cache
        std::atomic<size_t> m_AccountedSize{0};
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
