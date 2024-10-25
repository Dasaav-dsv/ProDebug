#include "index.hpp"

using namespace Index::Detail;

Bytes::Bytes(ByteRange range) : ext(getBlockSize(range)), idx{} {
    if (range.size() + size_t{ ext } * 256 > std::numeric_limits<uint32_t>::max())
        throw std::length_error("input range too big");
    const auto base = &idx[1];
    for (uint8_t byte : range)
        ++base[byte];
    for (int i = 0; i < 256; ++i)
        base[i] += base[i - 1] + ext;
    const auto size = static_cast<uint32_t>(range.size());
    std::array<uint16_t*, 256> ptrs{};
    for (uint32_t i = 0; i < 256; ++i)
        ptrs[i] = &data()[idx[i]];
    uint32_t first = 0;
    for (uint32_t first = 0; first < size;) {
        uint32_t last = std::min(first + 0xFFFF, size);
        while (first != last) {
            *(ptrs[range[first]]++) = static_cast<uint16_t>(first % 0xFFFF);
            ++first;
        }
        for (auto& ptr : ptrs)
            *(ptr++) = 0xFFFF;
    }
}

std::vector<uint32_t> Bytes::findAll(ByteRange range, SortedPattern&& sorted) const {
    auto first = sorted.begin();
    auto last = sorted.end();
    std::vector<uint32_t> result;
    if (std::distance(first, last) >= 2) {
        auto [first1, last1] = ipair(first[0].second);
        auto [first2, last2] = ipair(first[1].second);
        uint32_t hi1 = first[1].first;
        uint32_t hi2 = first[0].first;
        while (first1 != last1 && *first1 == 0xFFFF) {
            hi1 += 0xFFFF;
            ++first1;
        }
        while (first2 != last2 && *first2 == 0xFFFF) {
            hi2 += 0xFFFF;
            ++first2;
        }
        if (first1 == last1 || first2 == last2)
            return result;
        uint32_t i1 = hi1 + *first1;
        uint32_t i2 = hi2 + *first2;
        while (first1 != last1 && first2 != last2) {
            if (i1 < i2)
                i1 = next(first1, last1, hi1);
            else if (i2 < i1)
                i2 = next(first2, last2, hi2);
            else {
                auto it = first + 2;
                uint32_t index = i1 - first[0].first - first[1].first;
                while (it != last && range[index + it->first] == it->second)
                    ++it;
                if (it == last)
                    result.push_back(index);
                i1 = next(first1, last1, hi1);
                i2 = next(first2, last2, hi2);
            }
        }
    }
    return result;
}

std::optional<uint32_t> Bytes::findFirst(ByteRange range, SortedPattern&& sorted) const {
    auto first = sorted.begin();
    auto last = sorted.end();
    if (std::distance(first, last) >= 2) {
        auto [first1, last1] = ipair(first[0].second);
        auto [first2, last2] = ipair(first[1].second);
        uint32_t hi1 = first[1].first;
        uint32_t hi2 = first[0].first;
        while (first1 != last1 && *first1 == 0xFFFF) {
            hi1 += 0xFFFF;
            ++first1;
        }
        while (first2 != last2 && *first2 == 0xFFFF) {
            hi2 += 0xFFFF;
            ++first2;
        }
        if (first1 == last1 || first2 == last2)
            return std::nullopt;
        uint32_t i1 = hi1 + *first1;
        uint32_t i2 = hi2 + *first2;
        while (first1 != last1 && first2 != last2) {
            if (i1 < i2)
                i1 = next(first1, last1, hi1);
            else if (i2 < i1)
                i2 = next(first2, last2, hi2);
            else {
                auto it = first + 2;
                uint32_t index = i1 - first[0].first - first[1].first;
                while (it != last && range[index + it->first] == it->second)
                    ++it;
                if (it == last)
                    return index;
                i1 = next(first1, last1, hi1);
                i2 = next(first2, last2, hi2);
            }
        }
    }
    return std::nullopt;
}
