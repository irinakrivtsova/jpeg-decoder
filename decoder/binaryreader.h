#pragma once

#include <cstddef>
#include <cstdint>
#include <istream>

class BinaryReader {
public:
    explicit BinaryReader(std::istream& stream) : stream_(stream) {
    }

    uint8_t ReadOneByte();
    uint16_t ReadTwoBytes();
    void ReadExact(char* destination, std::size_t size);
    void SkipExact(std::size_t size);

private:
    std::istream& stream_;
};
