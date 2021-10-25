#include <iostream>

#include "ram.h"

RAM::RAM() : data(65535, 0x0)
{
}

const std::vector<uint8_t> RAM::read(uint16_t segment, uint16_t offset,
                                uint16_t size) const
{
    auto index = index_from_segment_and_offset(segment, offset);
    std::cout << "Read from 0x" << std::hex << index << "\n";
    auto begin = data.begin() + index;
    auto end = begin + size;
    std::vector<uint8_t> result(begin, end);
    return result;
}

void RAM::write(uint16_t segment, uint16_t offset,
                const std::vector<uint8_t>& buf)
{
    auto index = index_from_segment_and_offset(segment, offset);
    std::cout << "Write to 0x" << std::hex << index << "\n";
    std::copy(buf.begin(), buf.end(), data.begin() + index);
}

size_t RAM::index_from_segment_and_offset(uint16_t segment,
                                          uint16_t offset) const
{
    return segment * 16 + offset;
}

