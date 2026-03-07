#ifndef PCSC_CRYPTO_AESGCMUTIL_H
#define PCSC_CRYPTO_AESGCMUTIL_H

#include <vector>
#include <cstddef>

constexpr size_t AesGcm_NonceSize = 12u;
constexpr size_t AesGcm_TagSize = 16u;

inline void splitGcmOutput(const std::vector<unsigned char>& input,
                           std::vector<unsigned char>& nonce,
                           std::vector<unsigned char>& ciphertext,
                           std::vector<unsigned char>& tag) {
	nonce.clear(); ciphertext.clear(); tag.clear();
	if (input.size() < (AesGcm_NonceSize + AesGcm_TagSize)) return;
	nonce.insert(nonce.end(), input.begin(), input.begin() + AesGcm_NonceSize);
	tag.insert(tag.end(), input.end() - AesGcm_TagSize, input.end());
	ciphertext.insert(ciphertext.end(), input.begin() + AesGcm_NonceSize, input.end() - AesGcm_TagSize);
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

#endif // PCSC_CRYPTO_AESGCMUTIL_H
