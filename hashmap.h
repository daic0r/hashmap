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
    struct node_t {
        Key key;
        Value value;
        node_t* pNext{};
    };

    using bucket_t = node_t*;

    std::size_t m_nCapacityLeft{};
    std::size_t m_nCapacity{};
    bucket_t* m_arBuffer{};

public:
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
    constexpr bool emplace(T&& key, U&& val) {
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
            *pNewNode = new node_t{ std::forward<T>(key), std::forward<U>(val), nullptr };
        }
        catch (...) {
            return false;
        }
        if (pParent)
            pParent->pNext = *pNewNode;
        if (pNewNode == pBucket)
            --m_nCapacityLeft;
        return true;
    }

    template<std::convertible_to<Key> T>
    constexpr std::optional<std::reference_wrapper<Value>> find(T&& key) const noexcept {
        auto pNode = m_arBuffer[Hash{}(std::forward<T>(key)) % m_nCapacity];
        while (pNode && (std::forward<T>(key) != pNode->key)) {
            pNode = pNode->pNext;
        }
        if (not pNode)
            return std::nullopt;
        return pNode->value;
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
            copy_buckets<true>(temp.m_arBuffer, m_arBuffer, m_nCapacity);
            temp.swap(*this);
        }
        catch (...) {
            return false;
        }
        return true;
    }

};

#endif