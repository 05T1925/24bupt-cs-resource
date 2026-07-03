#include "HashUtil.h"

#include <array>
#include <iomanip>
#include <sstream>
#include <vector>

uint32_t HashUtil::rotateRight(uint32_t value, uint32_t bits) {
    return (value >> bits) | (value << (32U - bits));
}

uint32_t HashUtil::choose(uint32_t x, uint32_t y, uint32_t z) {
    return (x & y) ^ (~x & z);
}

uint32_t HashUtil::majority(uint32_t x, uint32_t y, uint32_t z) {
    return (x & y) ^ (x & z) ^ (y & z);
}

uint32_t HashUtil::bigSigma0(uint32_t x) {
    return rotateRight(x, 2) ^ rotateRight(x, 13) ^ rotateRight(x, 22);
}

uint32_t HashUtil::bigSigma1(uint32_t x) {
    return rotateRight(x, 6) ^ rotateRight(x, 11) ^ rotateRight(x, 25);
}

uint32_t HashUtil::smallSigma0(uint32_t x) {
    return rotateRight(x, 7) ^ rotateRight(x, 18) ^ (x >> 3);
}

uint32_t HashUtil::smallSigma1(uint32_t x) {
    return rotateRight(x, 17) ^ rotateRight(x, 19) ^ (x >> 10);
}

std::string HashUtil::sha256(const std::string& input) {
    // 64 个轮常量由 SHA-256 标准定义，不能随业务配置改变。
    static const std::array<uint32_t, 64> k = {
        0x428a2f98U, 0x71374491U, 0xb5c0fbcfU, 0xe9b5dba5U, 0x3956c25bU, 0x59f111f1U,
        0x923f82a4U, 0xab1c5ed5U, 0xd807aa98U, 0x12835b01U, 0x243185beU, 0x550c7dc3U,
        0x72be5d74U, 0x80deb1feU, 0x9bdc06a7U, 0xc19bf174U, 0xe49b69c1U, 0xefbe4786U,
        0x0fc19dc6U, 0x240ca1ccU, 0x2de92c6fU, 0x4a7484aaU, 0x5cb0a9dcU, 0x76f988daU,
        0x983e5152U, 0xa831c66dU, 0xb00327c8U, 0xbf597fc7U, 0xc6e00bf3U, 0xd5a79147U,
        0x06ca6351U, 0x14292967U, 0x27b70a85U, 0x2e1b2138U, 0x4d2c6dfcU, 0x53380d13U,
        0x650a7354U, 0x766a0abbU, 0x81c2c92eU, 0x92722c85U, 0xa2bfe8a1U, 0xa81a664bU,
        0xc24b8b70U, 0xc76c51a3U, 0xd192e819U, 0xd6990624U, 0xf40e3585U, 0x106aa070U,
        0x19a4c116U, 0x1e376c08U, 0x2748774cU, 0x34b0bcb5U, 0x391c0cb3U, 0x4ed8aa4aU,
        0x5b9cca4fU, 0x682e6ff3U, 0x748f82eeU, 0x78a5636fU, 0x84c87814U, 0x8cc70208U,
        0x90befffaU, 0xa4506cebU, 0xbef9a3f7U, 0xc67178f2U
    };

    std::vector<unsigned char> data(input.begin(), input.end());
    const uint64_t bitLength = static_cast<uint64_t>(data.size()) * 8U;
    // SHA-256 填充：追加 1 位、补 0 至模 512 等于 448，再追加原始 64 位长度。
    data.push_back(0x80U);
    while ((data.size() % 64U) != 56U) {
        data.push_back(0U);
    }
    for (int i = 7; i >= 0; --i) {
        data.push_back(static_cast<unsigned char>((bitLength >> (i * 8)) & 0xffU));
    }

    // 八个初始哈希字同样来自标准定义。
    std::array<uint32_t, 8> h = {
        0x6a09e667U, 0xbb67ae85U, 0x3c6ef372U, 0xa54ff53aU,
        0x510e527fU, 0x9b05688cU, 0x1f83d9abU, 0x5be0cd19U
    };

    // 每个 512 位分组扩展为 64 个消息字并执行 64 轮压缩。
    for (std::size_t offset = 0; offset < data.size(); offset += 64U) {
        std::array<uint32_t, 64> w{};
        for (std::size_t i = 0; i < 16U; ++i) {
            const std::size_t j = offset + i * 4U;
            w[i] = (static_cast<uint32_t>(data[j]) << 24U) |
                   (static_cast<uint32_t>(data[j + 1]) << 16U) |
                   (static_cast<uint32_t>(data[j + 2]) << 8U) |
                   static_cast<uint32_t>(data[j + 3]);
        }
        for (std::size_t i = 16U; i < 64U; ++i) {
            w[i] = smallSigma1(w[i - 2]) + w[i - 7] + smallSigma0(w[i - 15]) + w[i - 16];
        }

        uint32_t a = h[0];
        uint32_t b = h[1];
        uint32_t c = h[2];
        uint32_t d = h[3];
        uint32_t e = h[4];
        uint32_t f = h[5];
        uint32_t g = h[6];
        uint32_t current = h[7];

        // current 对应规范中的工作变量 h，避免与外层哈希状态数组重名。
        for (std::size_t i = 0; i < 64U; ++i) {
            const uint32_t t1 = current + bigSigma1(e) + choose(e, f, g) + k[i] + w[i];
            const uint32_t t2 = bigSigma0(a) + majority(a, b, c);
            current = g;
            g = f;
            f = e;
            e = d + t1;
            d = c;
            c = b;
            b = a;
            a = t1 + t2;
        }

        h[0] += a;
        h[1] += b;
        h[2] += c;
        h[3] += d;
        h[4] += e;
        h[5] += f;
        h[6] += g;
        h[7] += current;
    }

    std::ostringstream stream;
    // 每个 32 位状态固定输出 8 个十六进制字符，保留前导零。
    stream << std::hex << std::setfill('0');
    for (uint32_t value : h) {
        stream << std::setw(8) << value;
    }
    return stream.str();
}
