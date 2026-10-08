#include "huffman.h"

#include <cstddef>

void Huffman::MakeTree() {
    std::size_t expected_values = 0;
    for (uint8_t count : len_count_) {
        expected_values += count;
    }

    if (expected_values != values_.size()) {
        throw std::runtime_error("Invalid Huffman table value count");
    }

    BinaryTree tree(std::make_unique<Node>(0, false));

    std::size_t index = 0;
    for (int i = 0; i < 16; ++i) {
        for (int j = 0; j < len_count_[i]; ++j) {
            if (!tree.Insert(tree.root.get(), values_.at(index++), static_cast<uint8_t>(i + 1))) {
                throw std::runtime_error("Oversubscribed Huffman table");
            }
        }
    }

    tree_ = std::move(tree);
}

bool Huffman::BinaryTree::Insert(Node* node, uint8_t value, uint8_t length) {
    if (node == nullptr || node->is_leaf) {
        return false;
    }

    if (length == 0) {
        if (node->left != nullptr || node->right != nullptr) {
            return false;
        }
        node->value = value;
        node->is_leaf = true;
        return true;
    }

    if (node->left == nullptr) {
        node->left = std::make_unique<Node>(0, false);
    }
    if (!node->left->is_leaf && Insert(node->left.get(), value, static_cast<uint8_t>(length - 1))) {
        return true;
    }

    if (node->right == nullptr) {
        node->right = std::make_unique<Node>(0, false);
    }
    if (!node->right->is_leaf &&
        Insert(node->right.get(), value, static_cast<uint8_t>(length - 1))) {
        return true;
    }

    return false;
}
