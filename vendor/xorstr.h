#pragma once

// Compile-time XOR string encryption
// Usage: XS("my secret string") -> returns decrypted const char* on stack
// IDA will only see encrypted bytes in .rdata, never plaintext

namespace xorstr_detail {

constexpr unsigned int seed() {
    unsigned int h = 0;
    const char* t = __TIME__;
    for (int i = 0; t[i]; ++i)
        h = h * 131 + t[i];
    return h;
}

constexpr unsigned int key() {
    unsigned int s = seed();
    s ^= s << 13;
    s ^= s >> 17;
    s ^= s << 5;
    return s;
}

template <unsigned int N, unsigned int K>
struct Encrypted {
    char data[N];

    constexpr Encrypted(const char (&str)[N]) : data{} {
        for (unsigned int i = 0; i < N; ++i)
            data[i] = str[i] ^ static_cast<char>((K >> ((i % 4) * 8)) & 0xFF);
    }
};

template <unsigned int N, unsigned int K>
class Decryptor {
    char buf_[N];
public:
    Decryptor(const Encrypted<N, K>& enc) {
        for (unsigned int i = 0; i < N; ++i)
            buf_[i] = enc.data[i] ^ static_cast<char>((K >> ((i % 4) * 8)) & 0xFF);
    }
    const char* c_str() const { return buf_; }
    operator const char*() const { return buf_; }
};

} // namespace xorstr_detail

// Each translation unit gets a different key due to __TIME__ variance.
// For stronger protection, use __LINE__ mixing per-call site:

#define XS_KEY_MIX(line) (xorstr_detail::key() ^ static_cast<unsigned int>(line) * 2654435761u)

#define XS(str)                                                                 \
    ([]() -> const char* {                                                      \
        constexpr auto k = XS_KEY_MIX(__LINE__);                                \
        static constexpr xorstr_detail::Encrypted<sizeof(str), k> enc(str);     \
        static thread_local xorstr_detail::Decryptor<sizeof(str), k> dec(enc);  \
        return dec.c_str();                                                     \
    }())
