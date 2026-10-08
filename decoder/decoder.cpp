#include "decoder2.h"
#include "../decoder.h"

#include "bitreader.h"
#include "color_conversion.h"
#include "huffman-dec.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <iterator>
#include <limits>
#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <fftw3.h>

namespace {
constexpr std::size_t kMaxDecodedSamples = 200'000'000;

int CeilDiv(int value, int divisor) {
    if (value < 0 || divisor <= 0) {
        throw std::runtime_error("Invalid division while computing JPEG geometry");
    }
    return (value + divisor - 1) / divisor;
}
}  // namespace

Image Decode(const std::filesystem::path& path) {
    std::ifstream stream(path, std::ios_base::binary);

    if (!stream.is_open()) {
        throw std::runtime_error("File opening error");
    }

    Decoder decoder(stream);
    return decoder.Decode();
}

Image Decoder::Decode() {
    if (ReadMarker() != 0xFFD8) {
        throw std::runtime_error("SOI marker expected");
    }

    bool frame_seen = false;
    bool eoi_seen = false;

    while (!eoi_seen) {
        const uint16_t marker = ReadMarker();

        switch (marker) {
            case 0xFFC0:  // SOF0
                if (frame_seen) {
                    throw std::runtime_error("Duplicate SOF marker");
                }
                ParseSof0();
                frame_seen = true;
                break;

            case 0xFFC2:  // SOF2
                throw std::runtime_error("Progressive JPEG is not supported");

            case 0xFFC4:  // DHT
                ParseDht();
                break;

            case 0xFFDB:  // DQT
                ParseDqt();
                break;

            case 0xFFDA:  // SOS
                if (!frame_seen) {
                    throw std::runtime_error("SOS before SOF");
                }
                DecodeScan();
                scan_seen_ = true;
                break;

            case 0xFFDD:  // DRI
                ParseDri();
                break;

            case 0xFFFE:  // COM
                ParseComment();
                break;

            case 0xFFD9:  // EOI
                eoi_seen = true;
                break;

            case 0xFFD8:
                throw std::runtime_error("Unexpected SOI marker");

            default:
                if (marker >= 0xFFE0 && marker <= 0xFFEF) {
                    SkipSegment();
                } else {
                    throw std::runtime_error("Unsupported JPEG marker: " + std::to_string(marker));
                }
        }
    }

    if (!frame_seen) {
        throw std::runtime_error("SOF0 marker was not found");
    }
    if (!scan_seen_) {
        throw std::runtime_error("SOS marker was not found");
    }

    return BuildImage();
}

uint16_t Decoder::ReadMarker() {
    uint8_t byte = reader_.ReadOneByte();
    if (byte != 0xFF) {
        throw std::runtime_error("JPEG marker prefix FF expected");
    }

    do {
        byte = reader_.ReadOneByte();
    } while (byte == 0xFF);

    if (byte == 0x00) {
        throw std::runtime_error("Unexpected stuffed byte outside entropy data");
    }

    return static_cast<uint16_t>(0xFF00u | byte);
}

void Decoder::SkipSegment() {
    const uint16_t length = reader_.ReadTwoBytes();
    if (length < 2) {
        throw std::runtime_error("Invalid segment length");
    }
    reader_.SkipExact(length - 2);
}

void Decoder::ParseSof0() {
    const uint16_t len = reader_.ReadTwoBytes();
    const uint8_t prec = reader_.ReadOneByte();
    const uint16_t hght = reader_.ReadTwoBytes();
    const uint16_t wdth = reader_.ReadTwoBytes();
    const uint8_t cnt = reader_.ReadOneByte();

    if (cnt == 0 || cnt > 4) {
        throw std::runtime_error("Invalid number of components");
    }
    if (len != static_cast<uint16_t>(8 + 3 * cnt)) {
        throw std::runtime_error("Invalid SOF0 length");
    }
    if (prec != 8) {
        throw std::runtime_error("Only 8-bit baseline JPEG is supported");
    }
    if (wdth == 0 || hght == 0) {
        throw std::runtime_error("Invalid image dimensions");
    }
    if (static_cast<std::size_t>(wdth) * hght > kMaxDecodedSamples) {
        throw std::runtime_error("Image is too large");
    }

    DCT dct(prec, hght, wdth, cnt);
    int blocks_in_mcu = 0;

    for (int i = 0; i < cnt; ++i) {
        const uint8_t id = reader_.ReadOneByte();
        const uint8_t byte = reader_.ReadOneByte();
        const uint8_t hor_sub = byte >> 4;
        const uint8_t ver_sub = byte & 0x0F;
        const uint8_t id_quant = reader_.ReadOneByte();

        if (hor_sub == 0 || hor_sub > 4 || ver_sub == 0 || ver_sub > 4) {
            throw std::runtime_error("Invalid component sampling factors");
        }
        if (id_quant > 3) {
            throw std::runtime_error("Invalid quantization table id");
        }

        blocks_in_mcu += hor_sub * ver_sub;
        dct.AddCanal(Canal(id, hor_sub, ver_sub, id_quant));
    }

    if (cnt > 1 && blocks_in_mcu > 10) {
        throw std::runtime_error("Too many data units in one MCU");
    }

    dct_ = std::move(dct);
}

void Decoder::ParseDht() {
    int len = reader_.ReadTwoBytes();
    if (len < 2) {
        throw std::runtime_error("Invalid Huffman tables length");
    }
    len -= 2;

    while (len > 0) {
        if (len < 17) {
            throw std::runtime_error("Truncated Huffman table");
        }

        const uint8_t byte = reader_.ReadOneByte();
        --len;

        const uint8_t type = byte >> 4;
        const uint8_t id = byte & 0x0F;
        if (type != 0 && type != 1) {
            throw std::runtime_error("Invalid type of Huffman table");
        }
        if (id > 3) {
            throw std::runtime_error("Invalid Huffman table id");
        }

        std::array<uint8_t, 16> lens{};
        int vals_cnt = 0;
        for (int i = 0; i < 16; ++i) {
            lens[i] = reader_.ReadOneByte();
            vals_cnt += lens[i];
            --len;
        }

        if (vals_cnt > 256 || len < vals_cnt) {
            throw std::runtime_error("Invalid Huffman table value count");
        }

        std::vector<uint8_t> vals;
        vals.reserve(static_cast<std::size_t>(vals_cnt));
        for (int i = 0; i < vals_cnt; ++i) {
            vals.push_back(reader_.ReadOneByte());
            --len;
        }

        huff_tables_.erase(std::remove_if(huff_tables_.begin(), huff_tables_.end(),
                                          [type, id](const Huffman& table) {
                                              return table.GetType() == type && table.GetId() == id;
                                          }),
                           huff_tables_.end());
        huff_tables_.emplace_back(type, id, std::move(lens), std::move(vals));
    }

    if (len != 0) {
        throw std::runtime_error("Invalid DHT section length");
    }
}

void Decoder::ParseDqt() {
    static constexpr std::array<int, 64> kZigzag = {
        0,  1,  8,  16, 9,  2,  3,  10, 17, 24, 32, 25, 18, 11, 4,  5,  12, 19, 26, 33, 40, 48,
        41, 34, 27, 20, 13, 6,  7,  14, 21, 28, 35, 42, 49, 56, 57, 50, 43, 36, 29, 22, 15, 23,
        30, 37, 44, 51, 58, 59, 52, 45, 38, 31, 39, 46, 53, 60, 61, 54, 47, 55, 62, 63};

    int len = reader_.ReadTwoBytes();
    if (len < 2) {
        throw std::runtime_error("Invalid DQT tables length");
    }
    len -= 2;

    while (len > 0) {
        if (len < 1) {
            throw std::runtime_error("Truncated DQT table");
        }

        const uint8_t byte = reader_.ReadOneByte();
        --len;
        const uint8_t value_len = byte >> 4;
        const uint8_t id = byte & 0x0F;

        if (value_len != 0 && value_len != 1) {
            throw std::runtime_error("Invalid quantization value precision");
        }
        if (id > 3) {
            throw std::runtime_error("Invalid quantization table id");
        }

        const int bytes_per_value = value_len == 0 ? 1 : 2;
        const int required = 64 * bytes_per_value;
        if (len < required) {
            throw std::runtime_error("Truncated DQT table data");
        }

        std::array<uint16_t, 64> data{};
        for (int i = 0; i < 64; ++i) {
            const uint16_t value = value_len == 0 ? reader_.ReadOneByte() : reader_.ReadTwoBytes();
            if (value == 0) {
                throw std::runtime_error("Quantization table contains zero");
            }
            data[kZigzag[i]] = value;
            len -= bytes_per_value;
        }

        dqt_tables_.erase(std::remove_if(dqt_tables_.begin(), dqt_tables_.end(),
                                         [id](const DQT& table) { return table.GetId() == id; }),
                          dqt_tables_.end());
        dqt_tables_.emplace_back(id, std::move(data));
    }

    if (len != 0) {
        throw std::runtime_error("Invalid DQT section length");
    }
}

void Decoder::ParseComment() {
    const uint16_t len = reader_.ReadTwoBytes();
    if (len < 2) {
        throw std::runtime_error("Invalid comment length");
    }

    std::string str(len - 2, '\0');
    reader_.ReadExact(str.data(), str.size());
    comment_ = Comment(std::move(str));
}

void Decoder::ParseDri() {
    const uint16_t len = reader_.ReadTwoBytes();
    if (len != 4) {
        throw std::runtime_error("Invalid DRI segment length");
    }
    restart_interval_ = reader_.ReadTwoBytes();
}

std::vector<uint8_t> Decoder::ParseInfo() {
    if (!dct_) {
        throw std::runtime_error("SOS before SOF");
    }

    const uint16_t len = reader_.ReadTwoBytes();
    const uint8_t count = reader_.ReadOneByte();

    if (count == 0 || count > dct_->GetCount()) {
        throw std::runtime_error("Invalid SOS component count");
    }
    if (len != static_cast<uint16_t>(6 + 2 * count)) {
        throw std::runtime_error("Invalid SOS section length");
    }

    std::vector<uint8_t> ids;
    ids.reserve(count);
    std::set<uint8_t> unique_ids;

    for (int i = 0; i < count; ++i) {
        const uint8_t id = reader_.ReadOneByte();
        if (!unique_ids.insert(id).second) {
            throw std::runtime_error("Duplicate component id in SOS");
        }
        if (decoded_components_.count(id) != 0) {
            throw std::runtime_error("Component is decoded more than once in baseline JPEG");
        }

        const uint8_t byte = reader_.ReadOneByte();
        const uint8_t id_huff_dc = byte >> 4;
        const uint8_t id_huff_ac = byte & 0x0F;
        if (id_huff_dc > 3 || id_huff_ac > 3) {
            throw std::runtime_error("Invalid Huffman table id in SOS");
        }

        Canal& canal = dct_.value()[id];
        canal.SetIdDc(id_huff_dc);
        canal.SetIdAc(id_huff_ac);

        GetHuffTree(0, id_huff_dc);
        GetHuffTree(1, id_huff_ac);
        FindDqt(canal.GetIdDqt());

        ids.push_back(id);
    }

    const uint8_t spectral_start = reader_.ReadOneByte();
    const uint8_t spectral_end = reader_.ReadOneByte();
    const uint8_t approximation = reader_.ReadOneByte();

    if (spectral_start != 0 || spectral_end != 63 || approximation != 0) {
        throw std::runtime_error("Only baseline sequential scans are supported");
    }

    return ids;
}

void Decoder::PrepareComponentPlanes() {
    if (!dct_) {
        throw std::runtime_error("Cannot prepare planes without SOF0");
    }
    if (!component_planes_.empty()) {
        return;
    }

    uint8_t h_max = 0;
    uint8_t v_max = 0;
    dct_->MaxSubsampling(h_max, v_max);

    const int mcu_x = CeilDiv(dct_->GetWidth(), 8 * h_max);
    const int mcu_y = CeilDiv(dct_->GetHeight(), 8 * v_max);

    std::size_t total_samples = 0;
    for (const Canal& canal : dct_->GetCanals()) {
        ComponentPlane plane;
        plane.width = mcu_x * canal.GetHorSubsampling() * 8;
        plane.height = mcu_y * canal.GetVerSubsampling() * 8;

        const std::size_t sample_count =
            static_cast<std::size_t>(plane.width) * static_cast<std::size_t>(plane.height);
        if (sample_count > kMaxDecodedSamples ||
            total_samples > kMaxDecodedSamples - sample_count) {
            throw std::runtime_error("Decoded component planes are too large");
        }
        total_samples += sample_count;
        plane.data.assign(sample_count, 0.0);

        const auto [unused, inserted] = component_planes_.emplace(canal.GetId(), std::move(plane));
        if (!inserted) {
            throw std::runtime_error("Duplicate component plane id");
        }
    }
}

void Decoder::PutBlock(uint8_t id, int block_x, int block_y, const std::array<double, 64>& block) {
    ComponentPlane& plane = component_planes_.at(id);
    const int origin_x = block_x * 8;
    const int origin_y = block_y * 8;

    if (block_x < 0 || block_y < 0 || origin_x + 8 > plane.width || origin_y + 8 > plane.height) {
        throw std::runtime_error("Decoded block is outside its component plane");
    }

    for (int y = 0; y < 8; ++y) {
        for (int x = 0; x < 8; ++x) {
            const std::size_t destination =
                static_cast<std::size_t>(origin_y + y) * plane.width + (origin_x + x);
            plane.data.at(destination) = block.at(static_cast<std::size_t>(y * 8 + x));
        }
    }
}

double Decoder::GetElement(uint8_t id, int x, int y) const {
    const ComponentPlane& plane = component_planes_.at(id);
    if (x < 0 || y < 0 || x >= plane.width || y >= plane.height) {
        throw std::runtime_error("Component sample coordinates are out of range");
    }
    return plane.data.at(static_cast<std::size_t>(y) * plane.width + x);
}

void Decoder::ReadRestartMarker(uint8_t expected_index) {
    uint8_t byte = reader_.ReadOneByte();
    if (byte != 0xFF) {
        throw std::runtime_error("Restart marker prefix FF expected");
    }
    do {
        byte = reader_.ReadOneByte();
    } while (byte == 0xFF);

    const uint8_t expected = static_cast<uint8_t>(0xD0 + expected_index);
    if (byte != expected) {
        throw std::runtime_error("Unexpected restart marker");
    }
}

void Decoder::DecodeScan() {
    const std::vector<uint8_t> ids = ParseInfo();
    PrepareComponentPlanes();

    uint8_t h_max = 0;
    uint8_t v_max = 0;
    dct_->MaxSubsampling(h_max, v_max);

    const bool interleaved = ids.size() > 1;
    int mcu_x = 0;
    int mcu_y = 0;

    if (interleaved) {
        mcu_x = CeilDiv(dct_->GetWidth(), 8 * h_max);
        mcu_y = CeilDiv(dct_->GetHeight(), 8 * v_max);
    } else {
        const Canal& canal = dct_.value()[ids.front()];
        mcu_x = CeilDiv(static_cast<int>(dct_->GetWidth()) * canal.GetHorSubsampling(), 8 * h_max);
        mcu_y = CeilDiv(static_cast<int>(dct_->GetHeight()) * canal.GetVerSubsampling(), 8 * v_max);
    }

    const std::size_t mcus = static_cast<std::size_t>(mcu_x) * mcu_y;
    BitReader bit_reader(stream_);
    std::map<uint8_t, int32_t> pred_dc;
    for (uint8_t id : ids) {
        pred_dc.emplace(id, 0);
    }

    std::size_t processed_mcus = 0;
    uint8_t expected_restart = 0;

    for (int current_mcu_y = 0; current_mcu_y < mcu_y; ++current_mcu_y) {
        for (int current_mcu_x = 0; current_mcu_x < mcu_x; ++current_mcu_x) {
            for (uint8_t id : ids) {
                const Canal& canal = dct_.value()[id];
                const DQT& dqt_table = FindDqt(canal.GetIdDqt());

                const int horizontal_blocks = interleaved ? canal.GetHorSubsampling() : 1;
                const int vertical_blocks = interleaved ? canal.GetVerSubsampling() : 1;

                for (int block_y = 0; block_y < vertical_blocks; ++block_y) {
                    for (int block_x = 0; block_x < horizontal_blocks; ++block_x) {
                        std::array<double, 64> block = ReadBlock(bit_reader, id);

                        block[0] += pred_dc.at(id);
                        pred_dc.at(id) = static_cast<int32_t>(block[0]);

                        for (int k = 0; k < 64; ++k) {
                            block[k] *= dqt_table[static_cast<std::size_t>(k)];
                        }

                        const std::array<double, 64> samples = Idct(std::move(block));
                        const int destination_block_x =
                            interleaved ? current_mcu_x * canal.GetHorSubsampling() + block_x
                                        : current_mcu_x;
                        const int destination_block_y =
                            interleaved ? current_mcu_y * canal.GetVerSubsampling() + block_y
                                        : current_mcu_y;

                        PutBlock(id, destination_block_x, destination_block_y, samples);
                    }
                }
            }

            ++processed_mcus;
            if (restart_interval_ != 0 && processed_mcus % restart_interval_ == 0 &&
                processed_mcus < mcus) {
                bit_reader.AlignToByte();
                ReadRestartMarker(expected_restart);
                expected_restart = static_cast<uint8_t>((expected_restart + 1) & 7);
                for (auto& [id, predictor] : pred_dc) {
                    (void)id;
                    predictor = 0;
                }
            }
        }
    }

    bit_reader.AlignToByte();
    decoded_components_.insert(ids.begin(), ids.end());
}

Image Decoder::BuildImage() const {
    if (!dct_) {
        throw std::runtime_error("Cannot build image without SOF0");
    }

    const std::vector<Canal>& canals = dct_->GetCanals();
    if (canals.size() != 1 && canals.size() != 3) {
        throw std::runtime_error("Only grayscale and three-component YCbCr JPEG are supported");
    }

    for (const Canal& canal : canals) {
        if (decoded_components_.count(canal.GetId()) == 0) {
            throw std::runtime_error("Not all JPEG components were decoded");
        }
    }

    uint8_t h_max = 0;
    uint8_t v_max = 0;
    dct_->MaxSubsampling(h_max, v_max);

    const int width = dct_->GetWidth();
    const int height = dct_->GetHeight();
    Image image(static_cast<std::size_t>(width), static_cast<std::size_t>(height));

    if (canals.size() == 1) {
        const Canal& y_channel = canals[0];
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                const int component_x = x * y_channel.GetHorSubsampling() / h_max;
                const int component_y = y * y_channel.GetVerSubsampling() / v_max;
                const int value = std::clamp(static_cast<int>(std::lround(GetElement(
                                                 y_channel.GetId(), component_x, component_y))),
                                             0, 255);
                image.SetPixel(static_cast<std::size_t>(y), static_cast<std::size_t>(x),
                               RGB{value, value, value});
            }
        }
    } else {
        const Canal& y_channel = canals[0];
        const Canal& cb_channel = canals[1];
        const Canal& cr_channel = canals[2];

        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                const int y_x = x * y_channel.GetHorSubsampling() / h_max;
                const int y_y = y * y_channel.GetVerSubsampling() / v_max;
                const int cb_x = x * cb_channel.GetHorSubsampling() / h_max;
                const int cb_y = y * cb_channel.GetVerSubsampling() / v_max;
                const int cr_x = x * cr_channel.GetHorSubsampling() / h_max;
                const int cr_y = y * cr_channel.GetVerSubsampling() / v_max;

                const RGB rgb =
                    color_conversion::YCbCrToRGB(GetElement(y_channel.GetId(), y_x, y_y),
                                                 GetElement(cb_channel.GetId(), cb_x, cb_y),
                                                 GetElement(cr_channel.GetId(), cr_x, cr_y));

                image.SetPixel(static_cast<std::size_t>(y), static_cast<std::size_t>(x), rgb);
            }
        }
    }

    if (comment_) {
        image.SetComment(comment_->GetData());
    }
    return image;
}

std::array<double, 64> Decoder::Idct(std::array<double, 64> block) {
    std::array<double, 64> result{};
    const double k_sqrt2 = std::sqrt(2);

    for (int y = 0; y < 8; ++y) {
        for (int x = 0; x < 8; ++x) {
            const int index = y * 8 + x;
            if (x == 0) {
                block[index] *= k_sqrt2;
            }
            if (y == 0) {
                block[index] *= k_sqrt2;
            }
        }
    }

    fftw_plan plan = fftw_plan_r2r_2d(8, 8, block.data(), result.data(), FFTW_REDFT01, FFTW_REDFT01,
                                      FFTW_ESTIMATE);
    if (plan == nullptr) {
        throw std::runtime_error("Failed to create FFTW plan");
    }

    fftw_execute(plan);
    fftw_destroy_plan(plan);

    for (double& value : result) {
        value = std::clamp(value / 16.0 + 128.0, 0.0, 255.0);
    }
    return result;
}

const DQT& Decoder::FindDqt(uint8_t id) const {
    for (const DQT& table : dqt_tables_) {
        if (table.GetId() == id) {
            return table;
        }
    }
    throw std::runtime_error("Quantization table not found: id=" + std::to_string(id));
}

std::array<double, 64> Decoder::ReadBlock(BitReader& reader, uint8_t id) {
    static constexpr std::array<int, 64> kZigzag = {
        0,  1,  8,  16, 9,  2,  3,  10, 17, 24, 32, 25, 18, 11, 4,  5,  12, 19, 26, 33, 40, 48,
        41, 34, 27, 20, 13, 6,  7,  14, 21, 28, 35, 42, 49, 56, 57, 50, 43, 36, 29, 22, 15, 23,
        30, 37, 44, 51, 58, 59, 52, 45, 38, 31, 39, 46, 53, 60, 61, 54, 47, 55, 62, 63};

    std::array<double, 64> block{};
    const Canal& canal = dct_.value()[id];

    auto receive_extend = [&reader](uint8_t length) -> int {
        if (length == 0) {
            return 0;
        }
        const int value = reader.ReadBits(length);
        const int threshold = 1 << (length - 1);
        return value >= threshold ? value : value - (1 << length) + 1;
    };

    HuffmanDecoder huff_decoder_dc(reader, GetHuffTree(0, canal.GetIdHuffDc()));
    const uint8_t dc_length = huff_decoder_dc.Decode();
    if (dc_length > 11) {
        throw std::runtime_error("Invalid baseline DC coefficient category");
    }
    block[0] = receive_extend(dc_length);

    HuffmanDecoder huff_decoder_ac(reader, GetHuffTree(1, canal.GetIdHuffAc()));
    int ind = 1;

    while (ind < 64) {
        const uint8_t value = huff_decoder_ac.Decode();
        if (value == 0x00) {
            break;
        }
        if (value == 0xF0) {
            if (ind + 16 > 64) {
                throw std::runtime_error("ZRL exceeds JPEG block bounds");
            }
            ind += 16;
            continue;
        }

        const uint8_t zeros = value >> 4;
        const uint8_t length = value & 0x0F;
        if (length == 0 || length > 10) {
            throw std::runtime_error("Invalid baseline AC Huffman symbol");
        }
        if (ind + zeros >= 64) {
            throw std::runtime_error("AC coefficient exceeds JPEG block bounds");
        }

        ind += zeros;
        block[kZigzag[ind]] = receive_extend(length);
        ++ind;
    }

    return block;
}

const Huffman::BinaryTree& Decoder::GetHuffTree(uint8_t type, uint8_t id) const {
    for (const Huffman& huff : huff_tables_) {
        if (huff.GetId() == id && huff.GetType() == type) {
            return huff.GetTree();
        }
    }

    throw std::runtime_error("Huffman table not found: type=" + std::to_string(type) +
                             ", id=" + std::to_string(id));
}
