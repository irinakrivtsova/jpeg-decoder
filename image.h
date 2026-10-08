#pragma once

#include <cstddef>
#include <string>
#include <utility>
#include <vector>

struct RGB {
    int r = 0;
    int g = 0;
    int b = 0;
};

class Image {
public:
    Image() = default;

    Image(std::size_t width, std::size_t height) {
        SetSize(width, height);
    }

    void SetSize(std::size_t width, std::size_t height) {
        data_.assign(height, std::vector<RGB>(width));
    }

    std::size_t Width() const noexcept {
        return data_.empty() ? 0 : data_[0].size();
    }

    std::size_t Height() const noexcept {
        return data_.size();
    }

    void SetPixel(std::size_t y, std::size_t x, const RGB& pixel) {
        data_.at(y).at(x) = pixel;
    }

    RGB GetPixel(std::size_t y, std::size_t x) const {
        return data_.at(y).at(x);
    }

    RGB& GetPixel(std::size_t y, std::size_t x) {
        return data_.at(y).at(x);
    }

    void SetComment(std::string comment) {
        comment_ = std::move(comment);
    }

    const std::string& GetComment() const noexcept {
        return comment_;
    }

private:
    std::vector<std::vector<RGB>> data_;
    std::string comment_;
};
