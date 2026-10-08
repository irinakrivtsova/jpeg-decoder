#pragma once

#include "canal.h"

#include <cstdint>
#include <vector>

class DCT {
public:
    DCT(uint8_t prec, uint16_t hght, uint16_t wdth, uint8_t cnt)
        : precision_{prec}, height_{hght}, width_{wdth}, count_{cnt} {
    }

    void AddCanal(Canal&& canal);

    Canal& operator[](int id);
    const Canal& operator[](int id) const;

    void MaxSubsampling(uint8_t& horizontal, uint8_t& vertical) const;

    uint16_t GetHeight() const noexcept {
        return height_;
    }

    uint16_t GetWidth() const noexcept {
        return width_;
    }

    uint8_t GetPrecision() const noexcept {
        return precision_;
    }

    uint8_t GetCount() const noexcept {
        return count_;
    }

    const std::vector<Canal>& GetCanals() const noexcept {
        return canals_;
    }

private:
    // uint16_t length_;
    uint8_t precision_;
    uint16_t height_;
    uint16_t width_;
    uint8_t count_;
    // bool progressive_;
    std::vector<Canal> canals_;
};
