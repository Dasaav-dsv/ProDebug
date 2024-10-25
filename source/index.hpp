#pragma once

#include <algorithm>
#include <array>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <memory>
#include <optional>
#include <ranges>
#include <stdexcept>
#include <utility>
#include <vector>

#include "pattern.hpp"

namespace Index {
namespace Detail {
struct Bytes {
    using ByteRange = std::span<const uint8_t>;
    using SortedPattern = std::vector<std::pair<uint16_t, uint8_t>>;

    explicit Bytes(ByteRange range);

    std::vector<uint32_t> findAll(ByteRange range, SortedPattern&& sorted) const;
    std::optional<uint32_t> findFirst(ByteRange range, SortedPattern&& sorted) const;

    uint32_t count(uint8_t byte) const noexcept {
        auto pos = &idx[byte];
        return pos[1] - pos[0] - ext;
    }

    size_t bytes() const noexcept {
        size_t dataSize = size_t{ total() } * sizeof(uint16_t);
        return sizeof(Bytes) - sizeof(uint16_t) + dataSize;
    }

    uint32_t total() const noexcept {
        return idx[256];
    }

    uint16_t* data() noexcept {
        return (_data) + 0;
    }

    const uint16_t* data() const noexcept {
        return (_data) + 0;
    }

    std::pair<const uint16_t*, const uint16_t*> ipair(uint8_t byte) const noexcept {
        auto pos = &idx[byte];
        return { &_data[pos[0]], &_data[pos[1]] };
    }

    static constexpr uint32_t getBlockSize(std::ranges::sized_range auto&& r) noexcept {
        uint32_t size = static_cast<uint32_t>(std::ranges::size(std::forward<decltype(r)>(r)));
        return size / 0xFFFF + static_cast<bool>(size % 0xFFFF); // TODO: what if it's 0
    }
    
    static constexpr size_t getTotalSize(std::ranges::sized_range auto&& r) noexcept {
        size_t overhead = getBlockSize(std::forward<decltype(r)>(r)) * 256 * sizeof(uint16_t);
        size_t dataSize = std::ranges::size(std::forward<decltype(r)>(r)) * sizeof(uint16_t);
        return sizeof(Bytes) - sizeof(uint16_t) + dataSize + overhead;
    }

    template <size_t N>
    SortedPattern sortCPattern(const CPattern<N>& pat) const {
        SortedPattern sorted;
        for (uint16_t i = 0; i < pat.len; ++i) {
            sorted.insert(std::upper_bound(sorted.begin(), sorted.end(), pat.bt[i], [&](uint8_t c, auto&& p) {
                return count(c) < count(p.first);
            }), { pat.pos[i], pat.bt[i] });
        }
        return sorted;
    }

    template <typename It>
    static constexpr uint32_t next(It& first, It last, uint32_t& hi) noexcept {
        for (uint16_t lo = 0; ++first != last;) {
            lo = *first;
            if (lo != 0xFFFF) [[likely]]
                return hi + lo;
            hi += 0xFFFF;
        }
        return 0;
    };

    const uint32_t ext;
    std::array<uint32_t, 257> idx;
    uint16_t _data[1];
};

template <typename Alloc>
concept BytesAllocator = requires {
    typename std::allocator_traits<Alloc>;
    std::same_as<uint32_t, typename std::allocator_traits<Alloc>::value_type>;
};

template <typename Alloc>
concept BytesAllocatorFrom = requires {
    typename std::allocator_traits<Alloc>;
    typename std::allocator_traits<Alloc>::template rebind_alloc<uint32_t>;
    std::constructible_from<typename std::allocator_traits<Alloc>::template rebind_alloc<uint32_t>, const Alloc&>;
};

template <BytesAllocator Alloc>
struct BytesBase : private Alloc {
    using Altraits = std::allocator_traits<Alloc>;
    using ByteRange = typename Bytes::ByteRange;

    BytesBase(ByteRange range, const Alloc& al) : Alloc(al), range(range),
        size(Bytes::getTotalSize(range) / sizeof(uint32_t) + 1) {}

    template <std::convertible_to<Alloc> Other>
    BytesBase(const BytesBase<Other>& other) : Alloc(other.alloc()),
        range(other.range), size(other.size) {}

    Bytes* makeInstance() {
        uint32_t* memory = Altraits::allocate(alloc(), size);
        if (memory == nullptr)
            throw std::bad_alloc();
        Bytes* instance = std::launder(reinterpret_cast<Detail::Bytes*>(memory));
        return std::construct_at(instance, range);
    }

    void operator()(Bytes* ptr) const noexcept {
        if (ptr != nullptr) {
            static_assert(std::is_trivially_destructible_v<Bytes>);
            Altraits::deallocate(alloc(), reinterpret_cast<uint32_t*>(ptr), size);
        }
    }

    Alloc& alloc() const noexcept {
        return static_cast<Alloc&>(const_cast<BytesBase&>(*this));
    }

    const ByteRange range;
    const size_t size;
};
} // namespace Detail

template <Detail::BytesAllocator Alloc>
class BytesAdapter : private Detail::BytesBase<Alloc> {
    using Base = Detail::BytesBase<Alloc>;
    using ByteRange = typename Detail::Bytes::ByteRange;

    template <typename AllocFrom>
    using RebindAlloc = typename std::allocator_traits<AllocFrom>::template rebind_alloc<uint32_t>;

public:
    template <std::convertible_to<ByteRange> Range, Detail::BytesAllocatorFrom AllocFrom = Alloc>
    explicit BytesAdapter(Range&& r, const AllocFrom& al = {}) : Base(ByteRange(r),
        RebindAlloc<AllocFrom>(al)), impl(Base::makeInstance(), static_cast<Base&>(*this)) {}

    template <size_t N>
    std::vector<uint32_t> findAll(const CPattern<N>& pat) const {
        return impl->findAll(Base::range, impl->sortCPattern(pat));
    }

    template <size_t N>
    std::optional<uint32_t> findFirst(const CPattern<N>& pat) const {
        return impl->findFirst(Base::range, impl->sortCPattern(pat));
    }

    uint32_t count(uint8_t byte) const noexcept {
        return impl->count(byte);
    }

    size_t bytes() const noexcept {
        return sizeof(BytesAdapter) + impl->bytes();
    }

private:
    std::unique_ptr<Detail::Bytes, Base&> impl;
};

using Bytes = BytesAdapter<std::allocator<uint32_t>>;
} // namespace Index
