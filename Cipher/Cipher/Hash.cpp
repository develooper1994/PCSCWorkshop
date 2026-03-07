#include "Hash.h"
#include "Exceptions/GenericExceptions.h"
#include <openssl/evp.h>

namespace crypto {

static BYTEV digest(const EVP_MD* md, const BYTE* data, size_t len) {
	unsigned int outLen = 0;
	BYTEV out(static_cast<size_t>(EVP_MD_get_size(md)));
	if (EVP_Digest(data, len, out.data(), &outLen, md, nullptr) != 1)
		throw pcsc::CipherError("EVP_Digest failed");
	out.resize(outLen);
	return out;
}

BYTEV sha256(const BYTE* data, size_t len) { return digest(EVP_sha256(), data, len); }
BYTEV sha384(const BYTE* data, size_t len) { return digest(EVP_sha384(), data, len); }
BYTEV sha512(const BYTE* data, size_t len) { return digest(EVP_sha512(), data, len); }

} // namespace crypto
