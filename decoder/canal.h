#pragma once

#include <cstdint>

class Canal {
public:
    Canal(uint8_t id, uint8_t horizontal_subsampling, uint8_t vertical_subsampling, uint8_t id_dqt)
        : id_{id},
          horizontal_subsampling_{horizontal_subsampling},
          vertical_subsampling_{vertical_subsampling},
          id_dqt_{id_dqt},
          id_huff_dc_{0},
          id_huff_ac_{0} {
    }

    uint8_t GetId() const noexcept {
        return id_;
    }

    void SetIdDc(uint8_t id) noexcept {
        id_huff_dc_ = id;
    }

    void SetIdAc(uint8_t id) noexcept {
        id_huff_ac_ = id;
    }

    uint8_t GetHorSubsampling() const noexcept {
        return horizontal_subsampling_;
    }

    uint8_t GetVerSubsampling() const noexcept {
        return vertical_subsampling_;
    }

    uint8_t GetIdHuffDc() const noexcept {
        return id_huff_dc_;
    }

    uint8_t GetIdHuffAc() const noexcept {
        return id_huff_ac_;
    }

    uint8_t GetIdDqt() const noexcept {
        return id_dqt_;
    }

private:
    uint8_t id_;
    uint8_t horizontal_subsampling_;
    uint8_t vertical_subsampling_;
    uint8_t id_dqt_;
    uint8_t id_huff_dc_;
    uint8_t id_huff_ac_;
};
