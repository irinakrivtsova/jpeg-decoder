#pragma once
#include "huffman.h"
#include "bitreader.h"

class HuffmanDecoder {
public:
    HuffmanDecoder(BitReader& reader, const Huffman::BinaryTree& tree)
        : reader_{reader}, tree_{tree} {
    }
    uint8_t Decode();

private:
    BitReader& reader_;
    const Huffman::BinaryTree& tree_;
};
