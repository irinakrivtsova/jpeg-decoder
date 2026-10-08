#pragma once

#include <cstdint>
#include <istream>

class BitReader {
public:
    explicit BitReader(std::istream& stream);

    int ReadBit();
    int ReadBits(int n);
    void AlignToByte() noexcept;

private:
    void FillBuffer();

    std::istream& stream_;
    uint8_t buffer_;
    int bits_left_;
};
