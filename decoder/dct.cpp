#include "dct.h"

#include <algorithm>
#include <stdexcept>
#include <utility>

void DCT::AddCanal(Canal&& canal) {
    for (const Canal& existing : canals_) {
        if (existing.GetId() == canal.GetId()) {
            throw std::runtime_error("Duplicate component id in SOF0");
        }
    }
    canals_.push_back(std::move(canal));
}

Canal& DCT::operator[](int id) {
    for (Canal& canal : canals_) {
        if (canal.GetId() == id) {
            return canal;
        }
    }
    throw std::runtime_error("Non-existent component");
}

const Canal& DCT::operator[](int id) const {
    for (const Canal& canal : canals_) {
        if (canal.GetId() == id) {
            return canal;
        }
    }
    throw std::runtime_error("Non-existent component");
}

void DCT::MaxSubsampling(uint8_t& horizontal, uint8_t& vertical) const {
    if (canals_.empty()) {
        throw std::runtime_error("Frame has no components");
    }

    horizontal = 0;
    vertical = 0;

    for (const Canal& canal : canals_) {
        horizontal = std::max(horizontal, canal.GetHorSubsampling());
        vertical = std::max(vertical, canal.GetVerSubsampling());
    }
}
