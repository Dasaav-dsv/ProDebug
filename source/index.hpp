#pragma once

#include <algorithm>
#include <array>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <execution>
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

    struct Allocator {
        virtual ~Allocator() = default;
        virtual uint16_t* allocate(size_t) = 0;
    };

    Bytes(ByteRange range, Allocator& al);

    std::array<std::vector<uint16_t>, 256> getIndexedBytes();
    std::vector<uint32_t> findAll(SortedPattern&& sorted) const;
    std::optional<uint32_t> findFirst(SortedPattern&& sorted) const;

    uint32_t count(uint8_t byte) const noexcept {
        auto pos = &idx[byte];
        return pos[1] - pos[0] - overhead;
    }

    size_t bytes() const noexcept {
        return sizeof(Bytes) + static_cast<size_t>(total()) * sizeof(uint16_t);
    }

    uint32_t total() const noexcept {
        return idx[256];
    }

    std::pair<const uint16_t*, const uint16_t*> ipair(uint8_t byte) const noexcept {
        auto pos = &idx[byte];
        return { &data[pos[0]], &data[pos[1]] };
    }

    const uint16_t* begin(uint8_t byte) const noexcept {
        return &data[idx[byte]];
    }

    const uint16_t* end(uint8_t byte) const noexcept {
        return &data[idx[static_cast<size_t>(byte) + 1]];
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
    static constexpr void step(It& first, It& last, It& block, uint32_t& hi) noexcept {
        ++first;
        if (*block >= static_cast<uint16_t>(first - block) || first == last)
            return;
        do {
            ++first;
            hi += 0x1'0000;
        } while (first != last && first[-1] == 0);
        block = first - 1;
    };

    std::array<uint32_t, 257> idx;
    uint32_t overhead;
    ByteRange range;
    uint16_t* data;
};

template <typename Alloc>
concept BytesAllocatorInner = requires {
    typename std::allocator_traits<Alloc>;
    std::same_as<uint16_t, typename std::allocator_traits<Alloc>::value_type>;
};

template <typename Alloc>
concept BytesAllocatorToInner = requires {
    typename std::allocator_traits<Alloc>;
    typename std::allocator_traits<Alloc>::template rebind_alloc<uint16_t>;
    std::constructible_from<typename std::allocator_traits<Alloc>::template rebind_alloc<uint16_t>, const Alloc&>;
};

template <typename Alloc>
concept BytesAllocatorOuter = requires {
    typename std::allocator_traits<Alloc>;
    std::same_as<Bytes, typename std::allocator_traits<Alloc>::value_type>;
};

template <typename Alloc>
concept BytesAllocatorToOuter = requires {
    typename std::allocator_traits<Alloc>;
    typename std::allocator_traits<Alloc>::template rebind_alloc<Bytes>;
    std::constructible_from<typename std::allocator_traits<Alloc>::template rebind_alloc<Bytes>, const Alloc&>;
};

template <BytesAllocatorInner Inner, BytesAllocatorOuter Outer>
struct BytesBase : private Inner, private Outer {
    using AltraitsI = std::allocator_traits<Inner>;
    using AltraitsO = std::allocator_traits<Outer>;

    BytesBase(const Inner& i, const Outer& o) : Inner(i), Outer(o) {}

    template <std::convertible_to<Inner> OtherInner,
        std::convertible_to<Outer> OtherOuter>
    BytesBase(const BytesBase<OtherInner, OtherOuter>& other)
        : Inner(other.inner()), Outer(other.outer()) {}

    void operator()(Bytes* ptr) const noexcept {
        if (ptr != nullptr) {
            if (ptr->data != nullptr) {
                uint16_t* data = std::exchange(ptr->data, nullptr);
                AltraitsI::deallocate(inner(), data, ptr->total());
            }
            AltraitsO::destroy(outer(), ptr);
            AltraitsO::deallocate(outer(), ptr, 1);
        }
    }

    Inner& inner() const noexcept {
        return static_cast<Inner&>(const_cast<BytesBase&>(*this));
    }

    Outer& outer() const noexcept {
        return static_cast<Outer&>(const_cast<BytesBase&>(*this));
    }
};
} // namespace Detail

template <Detail::BytesAllocatorOuter Inner, Detail::BytesAllocatorOuter Outer = typename
    std::allocator_traits<Inner>::template rebind_alloc<Detail::Bytes>>
class BytesAdapter : private Detail::BytesBase<Inner, Outer> {
    using Base = Detail::BytesBase<Inner, Outer>;
    using ByteRange = std::span<const uint8_t>;

    template <typename Alloc>
    using RebindToInner = typename std::allocator_traits<Alloc>::template rebind_alloc<uint16_t>;
    template <typename Alloc>
    using RebindToOuter = typename std::allocator_traits<Alloc>::template rebind_alloc<Detail::Bytes>;

    struct AllocatorAdapter : public Detail::Bytes::Allocator {
        AllocatorAdapter(Base& base) noexcept : base(base) {}

        uint16_t* allocate(size_t n) override {
            return Base::AltraitsI::allocate(base.inner(), n);
        }

        Base& base;
    };

public:
    template <std::convertible_to<ByteRange> Range,
        Detail::BytesAllocatorToInner ToInner = Inner, Detail::BytesAllocatorToInner ToOuter = Outer>
    explicit BytesAdapter(Range&& r, const ToInner& i = {}, const ToOuter& o = {})
        : Base(RebindToInner<ToInner>(i), RebindToOuter<ToOuter>(o)), impl(nullptr, base()) {
        Detail::Bytes* instance = std::allocator_traits<Outer>::allocate(base().outer(), 1);
        if (instance == nullptr)
            throw std::bad_alloc();
        auto al = AllocatorAdapter(base());
        std::allocator_traits<Outer>::construct(base().outer(), instance,
            ByteRange(std::forward<decltype(r)>(r)), al);
        impl.reset(instance);
    }

    template <size_t N>
    std::vector<uint32_t> findAll(const CPattern<N>& pat) const {
        return impl->findAll(impl->sortCPattern(pat));
    }

    template <size_t N>
    std::optional<uint32_t> findFirst(const CPattern<N>& pat) const {
        return impl->findFirst(impl->sortCPattern(pat));
    }

    uint32_t count(uint8_t byte) const noexcept {
        return impl->count(byte);
    }

    size_t bytes() const noexcept {
        return sizeof(BytesAdapter) + impl->bytes();
    }

private:
    Base& base() noexcept {
        return static_cast<Base&>(*this);
    }

    std::unique_ptr<Detail::Bytes, Base&> impl;
};

using Bytes = BytesAdapter<std::allocator<uint16_t>>;
} // namespace Index
