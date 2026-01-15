//
// Created by ragnarokk on 06.01.2026.
//
// эту штуку я только начал делать, поэтому тут мб не доделано

#ifndef COLUMNAR_ENGINE_CODEC_H
#define COLUMNAR_ENGINE_CODEC_H

#include <cstdint>
#include <cstring>
#include <unordered_map>
#include <vector>

enum class CodecType : uint8_t {
    NONE,
    RLE,
    DICTIONARY,
    DELTA,
    BITPACKING
};

template <typename T>
class Codec {
public:
    virtual ~Codec() = default;

    virtual CodecType GetType() const = 0;

    virtual std::vector<char> Encode(const std::vector<T>& data) = 0;

    virtual std::vector<T> Decode(const std::vector<char>& encoded_data) = 0;
};

template <typename T>
class RLECodec : public Codec<T> {
public:
    CodecType GetType() const override {
        return CodecType::RLE;
    }

    std::vector<char> Encode(const std::vector<T>& data) override {
        std::vector<char> result;
        if (data.empty()) {
            return result;
        }
        size_t count = 1;
        T current = data[0];
        for (size_t i = 1; i < data.size(); ++i) {
            if (data[i] == current) {
                ++count;
            } else {
                AppendEncoded(result, count, current);
                current = data[i];
                count = 1;
            }
        }
        AppendEncoded(result, count, current);
        return result;
    }

    std::vector<T> Decode(const std::vector<char>& encoded_data) override {
        std::vector<T> result;
        size_t offset = 0;
        while (offset < encoded_data.size()) {
            size_t count;
            T value;
            std::memcpy(&count, encoded_data.data() + offset, sizeof(count));
            offset += sizeof(count);
            std::memcpy(&value, encoded_data.data() + offset, sizeof(value));
            offset += sizeof(value);
            result.insert(result.end(), count, value);
        }
        return result;
    }

private:
    void AppendEncoded(std::vector<char>& result, size_t count, const T& value) {
        size_t old_size = result.size();
        result.resize(old_size + sizeof(value) + sizeof(count));
        std::memcpy(result.data() + old_size, &count, sizeof(count));
        std::memcpy(result.data() + old_size + sizeof(count), &value, sizeof(value));
    }
};

template <typename T>
class CodecDictionary : public Codec<T> {
public:
    CodecType GetType() const override {
        return CodecType::DICTIONARY;
    }

    std::vector<char> Encode(const std::vector<T>& data) override {
        std::vector<char> result;
        std::unordered_map<T, uint32_t> dictionary;
        std::vector<uint32_t> indices;
        uint32_t dict_index = 0;

        for (const auto& item : data) {
            if (dictionary.find(item) == dictionary.end()) {
                dictionary[item] = dict_index++;
            }
            indices.push_back(dictionary[item]);
        }

        size_t dict_size = dictionary.size();
        result.resize(sizeof(dict_size));
        std::memcpy(result.data(), &dict_size, sizeof(dict_size));

        for (const auto& pair : dictionary) {
            size_t old_size = result.size();
            result.resize(old_size + sizeof(pair.first) + sizeof(pair.second));
            std::memcpy(result.data() + old_size, &pair.first, sizeof(pair.first));
            std::memcpy(result.data() + old_size + sizeof(pair.first), &pair.second,
                        sizeof(pair.second));
        }

        for (const auto& index : indices) {
            size_t old_size = result.size();
            result.resize(old_size + sizeof(index));
            std::memcpy(result.data() + old_size, &index, sizeof(index));
        }

        return result;
    }
};

#endif  // COLUMNAR_ENGINE_CODEC_H
