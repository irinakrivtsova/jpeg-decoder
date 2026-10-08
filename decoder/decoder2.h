#pragma once

#include "../image.h"
#include "binaryreader.h"
#include "comment.h"
#include "dct.h"
#include "dqt.h"
#include "huffman.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <istream>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>

class BitReader;

class Decoder {
public:
    explicit Decoder(std::istream& stream) : stream_{stream}, reader_{stream} {
    }

    Image Decode();

private:
    struct ComponentPlane {
        int width = 0;
        int height = 0;
        std::vector<double> data;
    };

    uint16_t ReadMarker();
    void SkipSegment();
    void ParseSof0();
    void ParseDht();
    void ParseDqt();
    void ParseComment();
    void ParseDri();
    std::vector<uint8_t> ParseInfo();
    void DecodeScan();
    Image BuildImage() const;

    void PrepareComponentPlanes();
    void PutBlock(uint8_t id, int block_x, int block_y, const std::array<double, 64>& block);
    double GetElement(uint8_t id, int x, int y) const;
    void ReadRestartMarker(uint8_t expected_index);

    const Huffman::BinaryTree& GetHuffTree(uint8_t type, uint8_t id) const;
    std::array<double, 64> ReadBlock(BitReader& reader, uint8_t id);
    std::array<double, 64> Idct(std::array<double, 64> block);
    const DQT& FindDqt(uint8_t id) const;

    std::istream& stream_;
    std::optional<Comment> comment_;
    std::vector<DQT> dqt_tables_;
    std::optional<DCT> dct_;
    std::vector<Huffman> huff_tables_;
    BinaryReader reader_;

    std::map<uint8_t, ComponentPlane> component_planes_;
    std::set<uint8_t> decoded_components_;
    uint16_t restart_interval_ = 0;
    bool scan_seen_ = false;
};
