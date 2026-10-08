#pragma once

#include <array>
#include <cstdint>
#include <memory>
#include <optional>
#include <stdexcept>
#include <utility>
#include <vector>

class Node {
public:
    Node(uint8_t val, bool leaf) : value(val), is_leaf(leaf) {
    }

    uint8_t value;
    std::unique_ptr<Node> left;
    std::unique_ptr<Node> right;
    bool is_leaf;
};

class Huffman {
public:
    Huffman(uint8_t type, uint8_t id, std::array<uint8_t, 16>&& len_count,
            std::vector<uint8_t>&& values)
        : type_{type}, id_{id}, len_count_{std::move(len_count)}, values_{std::move(values)} {
        MakeTree();
    }

    class BinaryTree {
    public:
        explicit BinaryTree(std::unique_ptr<Node> node) : root{std::move(node)} {
        }

        bool Insert(Node* node, uint8_t value, uint8_t length);

        std::unique_ptr<Node> root;
    };

    uint8_t GetType() const noexcept {
        return type_;
    }

    uint8_t GetId() const noexcept {
        return id_;
    }

    const BinaryTree& GetTree() const {
        if (!tree_) {
            throw std::runtime_error("Huffman tree is not initialized");
        }
        return *tree_;
    }

private:
    uint8_t type_;
    uint8_t id_;
    std::array<uint8_t, 16> len_count_;
    std::vector<uint8_t> values_;
    std::optional<BinaryTree> tree_;

    void MakeTree();
};
