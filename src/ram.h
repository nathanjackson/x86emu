#ifndef X86EMU_RAM_H
#define X86EMU_RAM_H

#include <cstddef>
#include <cstdint>
#include <vector>

class RAM
{
public:
    RAM();

    const std::vector<uint8_t> read(uint16_t segment, uint16_t offset,
                                    uint16_t size) const;

    void write(uint16_t segment, uint16_t offset,
               const std::vector<uint8_t>& buf);

private:
    size_t index_from_segment_and_offset(uint16_t segment,
                                         uint16_t offset) const;

    std::vector<uint8_t> data;
};

#endif // X86EMU_RAM_H
