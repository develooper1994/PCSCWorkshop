#ifndef PCSC_WORKSHOP1_CIPHER_CNGAESGCMUTIL_H
#define PCSC_WORKSHOP1_CIPHER_CNGAESGCMUTIL_H

#include "CngAESGcm.h"
#include <vector>

// Utility functions for AES-GCM output format used by CngAESGcm
// Format: [nonce (12 bytes)] [ciphertext (len bytes)] [tag (16 bytes)]

constexpr size_t CngAESGcm_NonceSize = 12u;
constexpr size_t CngAESGcm_TagSize = 16u;

inline void splitGcmOutput(const std::vector<unsigned char>& input,
                           std::vector<unsigned char>& nonce,
                           std::vector<unsigned char>& ciphertext,
                           std::vector<unsigned char>& tag) {
    nonce.clear(); ciphertext.clear(); tag.clear();
    if (input.size() < (CngAESGcm_NonceSize + CngAESGcm_TagSize)) return;
    size_t total = input.size();
    nonce.insert(nonce.end(), input.begin(), input.begin() + CngAESGcm_NonceSize);
    tag.insert(tag.end(), input.end() - CngAESGcm_TagSize, input.end());
    ciphertext.insert(ciphertext.end(), input.begin() + CngAESGcm_NonceSize, input.end() - CngAESGcm_TagSize);
}

inline std::vector<unsigned char> joinGcmOutput(const std::vector<unsigned char>& nonce,
                                                const std::vector<unsigned char>& ciphertext,
                                                const std::vector<unsigned char>& tag) {
    std::vector<unsigned char> out;
    out.reserve(nonce.size() + ciphertext.size() + tag.size());
    out.insert(out.end(), nonce.begin(), nonce.end());
    out.insert(out.end(), ciphertext.begin(), ciphertext.end());
    out.insert(out.end(), tag.begin(), tag.end());
    return out;
}

#endif // PCSC_WORKSHOP1_CIPHER_CNGAESGCMUTIL_H
