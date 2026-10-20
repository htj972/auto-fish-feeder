#pragma once
#include <cstring>
#include <map>
#include <string>
#include <vector>

// 在内存中模拟NVS，用于执行真实参数存储代码的保存与重启读取流程。
class Preferences {
  public:
    inline static std::map<std::string, std::vector<uint8_t>> data;

    bool begin(const char *, bool) {
        return true;
    }

    size_t getBytesLength(const char *key) {
        return data[key].size();
    }

    size_t getBytes(const char *key, void *out, size_t size) {
        if (data[key].size() != size) {
            return 0;
        }
        std::memcpy(out, data[key].data(), size);
        return size;
    }

    size_t putBytes(const char *key, const void *value, size_t size) {
        const auto *bytes = static_cast<const uint8_t *>(value);
        data[key] = {bytes, bytes + size};
        return size;
    }

    uint8_t getUChar(const char *key, uint8_t fallback) {
        return data[key].size() == 1 ? data[key][0] : fallback;
    }

    size_t putUChar(const char *key, uint8_t value) {
        return putBytes(key, &value, 1);
    }

    int32_t getInt(const char *key, int32_t fallback) {
        int32_t value;
        return getBytes(key, &value, sizeof(value)) ? value : fallback;
    }

    size_t putInt(const char *key, int32_t value) {
        return putBytes(key, &value, sizeof(value));
    }

    String getString(const char *key) {
        return data[key].empty() ? "" : reinterpret_cast<const char *>(data[key].data());
    }

    size_t putString(const char *key, const String &value) {
        putBytes(key, value.c_str(), value.length() + 1);
        return value.length();
    }
};
