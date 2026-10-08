#pragma once

#include <string>
#include <utility>

class Comment {
public:
    explicit Comment(std::string data) : data_(std::move(data)) {
    }

    const std::string& GetData() const noexcept {
        return data_;
    }

private:
    std::string data_;
};
