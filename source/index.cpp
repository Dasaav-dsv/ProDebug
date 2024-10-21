#include "index.hpp"

using namespace Index;

Detail::Bytes::Bytes(ByteRange range, Allocator& al) : idx{}, overhead(0), range(range), data(nullptr) {
    if (range.size() + (range.size() >> 8) + 256 > std::numeric_limits<uint32_t>::max())
        throw std::length_error("input range too big");
    auto indexed = getIndexedBytes();
    uint16_t* memory = al.allocate(total());
    if (memory == nullptr)
        throw std::bad_alloc();
    for (size_t i = 0; i < 256; ++i) {
        std::memcpy(static_cast<void*>(memory + idx[i]), static_cast<const void*>(indexed[i].data()),
            indexed[i].size() * sizeof(uint16_t));
        indexed[i].clear();
    }
    data = memory;
}

std::array<std::vector<uint16_t>, 256> Detail::Bytes::getIndexedBytes() {
    std::array<std::vector<uint16_t>, 256> out{};
    const size_t size = range.size();
    for (size_t i = 0; i < 256; ++i) {
        out[i].reserve(size / 0x10000 + size / 0x100 + 1);
        out[i].push_back(0);
    }
    uint32_t i = 0;
    std::array<uint32_t, 256> sizes{};
    while (i < (size & 0xFFFF'0000)) {
        do {
            out[range[i]].push_back(static_cast<uint16_t>(i));
        } while (++i & 0xFFFF);
        for (size_t j = 0; j < 256; ++j) {
            uint32_t first = sizes[j];
            uint32_t last = static_cast<uint32_t>(out[j].size());
            out[j][first] = static_cast<uint16_t>(last - first - 1);
            sizes[j] = last;
            out[j].push_back(0);
        }
    }
    for (; i < size; ++i) {
        out[range[i]].push_back(static_cast<uint16_t>(i));
    }
    uint32_t index = 0;
    for (size_t j = 0; j < 256; ++j) {
        uint32_t first = sizes[j];
        uint32_t last = static_cast<uint32_t>(out[j].size());
        out[j][first] = static_cast<uint16_t>(last - first - 1);
        sizes[j] = last;
        idx[j] = index;
        index += last;
    }
    idx[256] = index;
    overhead = (size >> 16) + 1;
    return out;
}

std::vector<uint32_t> Detail::Bytes::findAll(SortedPattern&& sorted) const {
    auto first = sorted.begin();
    auto last = sorted.end();
    std::vector<uint32_t> result;
    if (last - first >= 2) {
        auto [lfirst, llast] = ipair(first[0].second);
        auto [rfirst, rlast] = ipair(first[1].second);
        uint32_t lhi = first[1].first;
        uint32_t rhi = first[0].first;
        while (++lfirst != llast && lfirst[-1] == 0)
            lhi += 0x1'0000;
        while (++rfirst != rlast && rfirst[-1] == 0)
            rhi += 0x1'0000;
        auto lblock = lfirst - 1;
        auto rblock = rfirst - 1;
        while (lfirst != llast && rfirst != rlast) {
            uint32_t lhs = lhi + *lfirst;
            uint32_t rhs = rhi + *rfirst;
            if (lhs < rhs)
                step(lfirst, llast, lblock, lhi);
            else if (rhs < lhs)
                step(rfirst, rlast, rblock, rhi);
            else {
                step(lfirst, llast, lblock, lhi);
                step(rfirst, rlast, rblock, rhi);
                uint32_t index = lhs - first[0].first - first[1].first;
                auto it = first + 2;
                while (it != last && range[index + it->first] == it->second)
                    ++it;
                if (it == last)
                    result.push_back(index);
            }
        }
    }
    return result;
}

std::optional<uint32_t> Detail::Bytes::findFirst(SortedPattern&& sorted) const {
    auto first = sorted.begin();
    auto last = sorted.end();
    if (last - first >= 2) {
        auto [lfirst, llast] = ipair(first[0].second);
        auto [rfirst, rlast] = ipair(first[1].second);
        uint32_t lhi = first[1].first;
        uint32_t rhi = first[0].first;
        while (++lfirst != llast && lfirst[-1] == 0)
            lhi += 0x1'0000;
        while (++rfirst != rlast && rfirst[-1] == 0)
            rhi += 0x1'0000;
        auto lblock = lfirst - 1;
        auto rblock = rfirst - 1;
        while (lfirst != llast && rfirst != rlast) {
            uint32_t lhs = lhi + *lfirst;
            uint32_t rhs = rhi + *rfirst;
            if (lhs < rhs)
                step(lfirst, llast, lblock, lhi);
            else if (rhs < lhs)
                step(rfirst, rlast, rblock, rhi);
            else {
                step(lfirst, llast, lblock, lhi);
                step(rfirst, rlast, rblock, rhi);
                uint32_t index = lhs - first[0].first - first[1].first;
                auto it = first + 2;
                while (it != last && range[index + it->first] == it->second)
                    ++it;
                if (it == last)
                    return index;
            }
        }
    }
    return std::nullopt;
}
