#include "huffman-dec.h"
#include <stdexcept>

uint8_t HuffmanDecoder::Decode() {
    const Node* node = tree_.root.get();

    while (node != nullptr && !node->is_leaf) {
        node = reader_.ReadBit() == 0 ? node->left.get() : node->right.get();
    }

    if (node == nullptr) {
        throw std::runtime_error("Invalid Huffman code in scan data");
    }

    return node->value;
}
