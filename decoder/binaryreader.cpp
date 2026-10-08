#include "binaryreader.h"

#include <limits>
#include <stdexcept>

namespace {
std::streamsize ToStreamSize(std::size_t size) {
    if (size > static_cast<std::size_t>(std::numeric_limits<std::streamsize>::max())) {
        throw std::runtime_error("Requested stream operation is too large");
    }
    return static_cast<std::streamsize>(size);
}
}  // namespace

uint8_t BinaryReader::ReadOneByte() {
    const int byte = stream_.get();

    if (byte == std::char_traits<char>::eof()) {
        throw std::runtime_error("Unexpected end of file");
    }

    return static_cast<uint8_t>(byte);
}

uint16_t BinaryReader::ReadTwoBytes() {
    const uint16_t byte1 = ReadOneByte();
    const uint16_t byte2 = ReadOneByte();
    return static_cast<uint16_t>((byte1 << 8) | byte2);
}

void BinaryReader::ReadExact(char* destination, std::size_t size) {
    if (size == 0) {
        return;
    }
    if (destination == nullptr) {
        throw std::invalid_argument("Null destination in BinaryReader::ReadExact");
    }

    const std::streamsize count = ToStreamSize(size);
    stream_.read(destination, count);
    if (stream_.gcount() != count) {
        throw std::runtime_error("Unexpected end of file");
    }
}

void BinaryReader::SkipExact(std::size_t size) {
    const std::streamsize count = ToStreamSize(size);
    stream_.ignore(count);
    if (stream_.gcount() != count) {
        throw std::runtime_error("Unexpected end of file while skipping segment");
    }
}
