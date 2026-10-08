#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <utility>

class DQT {
public:
    DQT(uint8_t id, std::array<uint16_t, 64> table) : id_{id}, table_{std::move(table)} {
    }

    uint8_t GetId() const noexcept {
        return id_;
    }

    uint16_t operator[](std::size_t index) const {
        return table_.at(index);
    }

private:
    // uint8_t value_len_;
    uint8_t id_;
    std::array<uint16_t, 64> table_;
};
