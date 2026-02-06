/*  Copyright 2026 Diligent Graphics LLC

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

/// \file Defines a multi-producer single-consumer queue.

#include <atomic>
#include <utility>
#include <mutex>
#include <new>
#include <algorithm>
#include <type_traits>
#include <utility>

namespace Diligent
{

/// Multi-Producer Single-Consumer (MPSC) queue.
///
/// The queue enables multiple producers to enqueue items concurrently, while a single consumer can dequeue items.
/// Dequeue operations are lock-free.
///
/// \tparam T The type of items stored in the queue. Must be default-constructible, move-constructible, and move-assignable.
template <typename T>
class MPSCQueue
{
public:
    static_assert(std::is_move_assignable_v<T>, "T must be move-assignable");
    static_assert(std::is_move_constructible_v<T>, "T must be move-constructible");
    static_assert(std::is_default_constructible_v<T>, "T must be default-constructible");

    /// Constructs an empty MPSCQueue.
    MPSCQueue() :
        m_Head{new Node},
        m_Tail{m_Head}
    {
        m_Head->pNext.store(nullptr, std::memory_order_relaxed);
    }

    // clang-format off
    MPSCQueue           (const MPSCQueue&) = delete;
    MPSCQueue& operator=(const MPSCQueue&) = delete;
    MPSCQueue           (MPSCQueue&&)      = delete;
    MPSCQueue& operator=(MPSCQueue&&)      = delete;
    // clang-format on

    /// Destructs the MPSCQueue and releases all resources.
    ///
    /// \warning Not thread-safe. All producers must be stopped/joined before destruction.
    ~MPSCQueue()
    {
        // Drain the queue and delete head node
        {
            T value{};
            while (Dequeue(value)) {}
            delete m_Head;
        }

        // Delete all nodes in the free list
        for (Node* node = m_FreeHead.exchange(nullptr, std::memory_order_acq_rel); node != nullptr;)
        {
            Node* next = node->pNext.load(std::memory_order_relaxed);
            delete node;
            node = next;
        }
    }

    /// Enqueues a value into the queue.
    /// The method is thread-safe and can be called concurrently by multiple producers.
    /// \param value The value to enqueue. It will be moved into the queue.
    void Enqueue(T value)
    {
        Node* node = AllocateNode(std::move(value));
        node->pNext.store(nullptr, std::memory_order_relaxed);

        // Standard Lock-Free Enqueue
        Node* prev = m_Tail.exchange(node, std::memory_order_acq_rel);
        prev->pNext.store(node, std::memory_order_release);
    }

    /// Dequeues a value from the queue.
    /// The method is thread-safe and can be called in parallel with producers, but only one consumer should call it.
    /// \param result A reference to store the dequeued value. It will be moved from the queue.
    /// \return true if a value was successfully dequeued; false if the queue was empty.
    bool Dequeue(T& result)
    {
        Node* head = m_Head;
        // Acquire ensures we see the initialization of the node's data
        Node* next = head->pNext.load(std::memory_order_acquire);

        if (next == nullptr)
        {
            return false;
        }

        m_Head = next;
        result = std::move(next->Value);

        // Current head became the old dummy node, recycle it
        RecycleNode(head);

        return true;
    }

private:
    struct Node
    {
        T Value{};

        std::atomic<Node*> pNext{nullptr};

        Node() = default;
        Node(T&& v) :
            Value{std::move(v)}
        {}
    };

    // Get node from pool or create new
    Node* AllocateNode(T&& value)
    {
        Node* node = nullptr;

        // Lock prevents multiple producers from fighting for the same free node.
        // It effectively serializes freelist pops (only one popper at a time).
        // Since only the consumer pushes to the freelist, this avoids Treiber-pop ABA corruption.
        std::lock_guard<std::mutex> lock{m_FreeHeadMtx};

        // We use acquire to see the data written by the thread that recycled this node
        node = m_FreeHead.load(std::memory_order_acquire);
        while (node != nullptr)
        {
            Node* next = node->pNext.load(std::memory_order_relaxed);

            // Even though we hold a lock, we must CAS because the Consumer (RecycleNode)
            // does NOT hold the lock and might push a new node onto the head.
            if (m_FreeHead.compare_exchange_weak(
                    node, next,
                    std::memory_order_acq_rel,
                    std::memory_order_acquire))
            {
                node->Value = std::move(value);
                node->pNext.store(nullptr, std::memory_order_relaxed);
                return node;
            }
            // If CAS fails, 'node' is automatically updated to the new m_FreeHead
        }

        // Pool is empty
        return new Node{std::move(value)};
    }

    void RecycleNode(Node* node)
    {
        Node* old = m_FreeHead.load(std::memory_order_relaxed);
        do
        {
            node->pNext.store(old, std::memory_order_relaxed);
        } while (!m_FreeHead.compare_exchange_weak(
            old, node,
            std::memory_order_release, // Release our data to the popper
            std::memory_order_relaxed));
    }

private:
#ifdef __cpp_lib_hardware_interference_size
    static constexpr size_t CacheLineSize = std::max(std::hardware_destructive_interference_size, size_t{64});
#else
    static constexpr size_t CacheLineSize = 64;
#endif

#ifdef _MSC_VER
#    pragma warning(push)
#    pragma warning(disable : 4324) // structure was padded due to alignment specifier
#endif

    // Consumer Data (Hot)
    alignas(CacheLineSize) Node* m_Head = nullptr;

    // Free List (Shared - Moderate Contention)
    // The lock protects the "Pop" side from other Producers
    std::mutex         m_FreeHeadMtx;
    std::atomic<Node*> m_FreeHead{nullptr};

    // Producer Data (Hot)
    alignas(CacheLineSize) std::atomic<Node*> m_Tail{nullptr};

#ifdef _MSC_VER
#    pragma warning(pop)
#endif
};

} // namespace Diligent
