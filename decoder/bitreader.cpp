#include "bitreader.h"

#include <stdexcept>

BitReader::BitReader(std::istream& stream) : stream_(stream), buffer_(0), bits_left_(0) {
}

int BitReader::ReadBit() {
    if (bits_left_ == 0) {
        FillBuffer();
    }

    const int bit = (buffer_ >> (bits_left_ - 1)) & 1;
    --bits_left_;
    return bit;
}

int BitReader::ReadBits(int n) {
    if (n < 0 || n > 16) {
        throw std::invalid_argument("Invalid number of bits requested");
    }

    int result = 0;
    for (int i = 0; i < n; ++i) {
        result = (result << 1) | ReadBit();
    }
    return result;
}

void BitReader::AlignToByte() noexcept {
    bits_left_ = 0;
}

void BitReader::FillBuffer() {
    const int byte = stream_.get();
    if (byte == std::char_traits<char>::eof()) {
        throw std::runtime_error("Unexpected EOF in bit stream");
    }

    if (byte == 0xFF) {
        const int next_byte = stream_.get();
        if (next_byte == std::char_traits<char>::eof()) {
            throw std::runtime_error("Unexpected EOF after FF in bit stream");
        }
        if (next_byte != 0x00) {
            throw std::runtime_error("Unexpected marker in bit stream");
        }
    }

    buffer_ = static_cast<uint8_t>(byte);
    bits_left_ = 8;
}
