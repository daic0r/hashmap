#ifndef HASHMAP_H
#define HASHMAP_H

#include <functional>
#include <memory>
#include <type_traits>
#include <cstring>
#include <utility>
#include <optional>
#include <concepts>
#include <span>
#ifdef HASHMAP_DEBUG_
#include <iostream>
#endif

template<typename Key, typename Value, typename Hash=std::hash<Key>> 
class hashmap {
    struct node_t;
    using bucket_t = node_t*;

public:
    struct sentinel{};
    class iterator {
        friend class hashmap;
    public:
        using iterator_concept = std::bidirectional_iterator_tag;
        using difference_type = std::ptrdiff_t;
        using value_type = std::remove_cvref_t<Value>;
        using pointer = value_type*;
        using reference = value_type&;

        constexpr iterator() = default;
        constexpr bool operator==(const iterator& other) const noexcept {
            return m_pHashmap == other.m_pHashmap && m_pNode == other.m_pNode;
        }
        constexpr bool operator!=(const iterator& other) const noexcept {
            return not (*this == other);
        }

        constexpr reference operator *() const noexcept {
            return m_pNode->kvp.second;
        }

        constexpr reference operator->() const noexcept {
            return *(*this);
        }
        constexpr iterator& operator++() noexcept {
            m_pNode = m_pNode->pNext;
            if (m_pNode)
                return *this;
            ++m_pBucket;
            ++m_nBucketIndex;
            while (m_nBucketIndex < m_pHashmap->m_nCapacity && not *m_pBucket) {
                ++m_pBucket;
                ++m_nBucketIndex;
            }
            m_pNode = *m_pBucket;
            return *this;
        }
        constexpr iterator operator++(int) const noexcept {
            iterator copy{ *this };
            ++(*this);
            return copy;
        }
        constexpr iterator& operator--() noexcept {
            m_pNode = m_pNode->pPrev;
            if (m_pNode)
                return *this;
            if (m_nBucketIndex == 0)
                return *this;
            --m_pBucket;
            --m_nBucketIndex;
            while (m_nBucketIndex > 0 && not *m_pBucket) {
                --m_pBucket;
                --m_nBucketIndex;
            }
            m_pNode = *m_pBucket;
            return *this;
        }
        constexpr iterator operator--(int) const noexcept {
            iterator copy{ *this };
            --(*this);
            return copy;
        }


        constexpr bool operator==(sentinel) const noexcept {
            return m_nBucketIndex >= m_pHashmap->m_nCapacity;
        }
        constexpr bool operator!=(sentinel) const noexcept {
            return not (*this == sentinel{});
        }

    private:
        constexpr iterator(hashmap& hm, bucket_t* pBucket, node_t* pNode, std::optional<std::size_t> nBucketIdx) : 
            m_pHashmap{&hm},
            m_pBucket{pBucket ? pBucket : hm.m_arBuffer},
            m_nBucketIndex{pBucket ? nBucketIdx.value() : 0},
            m_pNode{pNode}
        {
            if (not pNode) {
                while (not *m_pBucket && m_nBucketIndex < hm.m_nCapacity) {
                    ++m_pBucket;
                    ++m_nBucketIndex;
                }
                if (*m_pBucket)
                    m_pNode = *m_pBucket;
            }
        }
        constexpr iterator(hashmap& hm, sentinel) :
            m_pHashmap{&hm}
        {
            m_nBucketIndex = hm.m_nCapacity;
        }

    private:
        hashmap* m_pHashmap{};
        bucket_t* m_pBucket{};
        node_t* m_pNode{};
        std::size_t m_nBucketIndex{};
    };

public:
    static_assert(std::bidirectional_iterator<iterator>);

    constexpr hashmap() : m_arBuffer{ 
        m_nCapacity > 0 ? new bucket_t[m_nCapacity] : nullptr }
    {}
    constexpr hashmap(hashmap const& rhs) : m_nCapacity{ rhs.m_nCapacity }, m_nCapacityLeft{ rhs.m_nCapacityLeft } {
        m_arBuffer = operator new[](rhs.m_nCapacity * sizeof(bucket_t));
        std::memset(&m_arBuffer[0], 0, rhs.m_nCapacity * sizeof(bucket_t));
        copy_buckets<false>(m_arBuffer, rhs.m_arBuffer, rhs.m_nCapacity);
    }
    constexpr hashmap& operator=(hashmap const& rhs) {
        hashmap copy{ rhs };
        copy.swap(*this);
        return *this;
    }
    constexpr hashmap(hashmap&& rhs) noexcept
        : m_nCapacity{ std::exchange(rhs.m_nCapacity, 0) },
        m_nCapacityLeft{ std::exchange(rhs.m_nCapacityLeft, 0) },
        m_arBuffer{ std::exchange(rhs.m_arBuffer, nullptr) }
    {}
    constexpr hashmap& operator=(hashmap&& rhs) noexcept {
        hashmap copy{ std::move(rhs) };
        copy.swap(*this);
        return *this;
    }
    ~hashmap() { 
        free_buckets(m_arBuffer, m_nCapacity);
    }

    constexpr void swap(hashmap& rhs) noexcept {
        using std::swap;
        swap(m_nCapacityLeft, rhs.m_nCapacityLeft);
        swap(m_nCapacity, rhs.m_nCapacity);
        swap(m_arBuffer, rhs.m_arBuffer);
    }

    constexpr friend void swap(hashmap& lhs, hashmap& rhs) noexcept {
        lhs.swap(rhs);
    }

    constexpr auto size() const noexcept { return m_nCapacity - m_nCapacityLeft; }

    template<std::convertible_to<Key> T, std::convertible_to<Value> U>
    constexpr std::pair<iterator, bool> emplace(T&& key, U&& val) {
        if (m_nCapacityLeft == 0) {
            grow();    
        }
        const auto pBucket = &m_arBuffer[Hash{}(std::forward<T>(key)) % m_nCapacity];
        auto pNewNode = pBucket;
        node_t* pParent{};
        while (*pNewNode) {
            if (not (*pNewNode)->pNext)
                pParent = *pNewNode;
            pNewNode = &(*pNewNode)->pNext;
        }
        try {
            *pNewNode = new node_t{ std::make_pair(std::forward<T>(key), std::forward<U>(val)), nullptr, nullptr };
        }
        catch (...) {
            return std::make_pair(iterator{ *this, sentinel{} }, false);
        }
        if (pParent) {
            pParent->pNext = *pNewNode;
            (*pNewNode)->pPrev = pParent;
        }
        if (pNewNode == pBucket)
            --m_nCapacityLeft;
        const std::size_t nBucktIdx = pBucket - &m_arBuffer[0];
        return std::make_pair(iterator{ *this, pBucket, *pNewNode, nBucktIdx }, true);
    }

    template<std::convertible_to<Key> T>
    constexpr iterator find(T&& key) const noexcept {
        auto pNode = m_arBuffer[Hash{}(std::forward<T>(key)) % m_nCapacity];
        const auto pBucket = &pNode;
        while (pNode && (std::forward<T>(key) != pNode->kvp.first)) {
            pNode = pNode->pNext;
        }
        if (not pNode)
            return iterator{ *this, sentinel{} };
        return iterator{ *this, pBucket, pNode, pBucket - &m_arBuffer[0] };
    }

    constexpr auto begin() noexcept {
        return iterator{ *this, nullptr, nullptr, std::nullopt };
    }

    constexpr auto end() noexcept {
        return sentinel{};
    }

private:
    template<bool Move>
    static constexpr void copy_buckets(bucket_t* pDst, bucket_t* pSrc, std::size_t nNumElements) {
        for (std::size_t i = 0; i < nNumElements; ++i) {
            auto pCurNodePtr = pSrc[i];
            auto pCurDestPtr = pDst[i];
            while (pCurNodePtr) {
                auto& pTarget = (pCurNodePtr == pSrc[i]) ? pDst[i] : pCurDestPtr->pNext;
                if constexpr (Move && std::is_nothrow_move_constructible_v<node_t>)
                    pTarget = new node_t(std::move(*pCurNodePtr));
                else
                    pTarget = new node_t(*pCurNodePtr);
                pCurDestPtr = pTarget;
                pCurNodePtr = pCurNodePtr->pNext;
            }
        }
    }

    static constexpr void free_buckets(bucket_t* pBuckets, std::size_t nNumElements) {
        for (std::size_t i = 0; i < nNumElements; ++i) {
            auto pCurNodePtr = pBuckets[i];
            while (pCurNodePtr) {
                auto pNext = pCurNodePtr->pNext;
                delete pCurNodePtr;
                pCurNodePtr = pNext;
            }
        }
        delete[] pBuckets;
    }
    constexpr bool grow() noexcept {
        try {
            hashmap temp{};
            if (m_nCapacity > 0) {
                temp.m_nCapacityLeft = m_nCapacity;
                temp.m_nCapacity = m_nCapacity * 2;
            } else {
                temp.m_nCapacity = 2;
                temp.m_nCapacityLeft = 2;
            }
#ifdef HASHMAP_DEBUG_
            std::cout << "Debug: Growing to capacity of " << temp.m_nCapacity << "\n";
#endif
            temp.m_arBuffer = reinterpret_cast<bucket_t*>(operator new[](temp.m_nCapacity * sizeof(bucket_t)));
            std::memset(temp.m_arBuffer, 0, temp.m_nCapacity * sizeof(bucket_t));
            if (m_arBuffer)
                copy_buckets<true>(temp.m_arBuffer, m_arBuffer, m_nCapacity);
            temp.swap(*this);
        }
        catch (...) {
            return false;
        }
        return true;
    }

private:
    struct node_t {
        std::pair<const Key, Value> kvp;
        node_t* pNext{};
        node_t* pPrev{};
    };

    std::size_t m_nCapacityLeft{};
    std::size_t m_nCapacity{};
    bucket_t* m_arBuffer{};
};

#endif