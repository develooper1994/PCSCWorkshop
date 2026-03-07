#include "Hmac.h"
#include "Exceptions/GenericExceptions.h"
#include <openssl/evp.h>
#include <openssl/core_names.h>
#include <openssl/params.h>
#include <cstring>

namespace crypto {

static BYTEV hmac(const char* digest, const BYTEV& key, const BYTE* data, size_t len) {
	EVP_MAC* mac = EVP_MAC_fetch(nullptr, "HMAC", nullptr);
	if (!mac) throw pcsc::CipherError("EVP_MAC_fetch(HMAC) failed");

	EVP_MAC_CTX* ctx = EVP_MAC_CTX_new(mac);
	if (!ctx) { EVP_MAC_free(mac); throw pcsc::CipherError("EVP_MAC_CTX_new failed"); }

	OSSL_PARAM params[2];
	params[0] = OSSL_PARAM_construct_utf8_string(OSSL_MAC_PARAM_DIGEST,
	                                              const_cast<char*>(digest), 0);
	params[1] = OSSL_PARAM_construct_end();

	int rc = EVP_MAC_init(ctx, key.data(), key.size(), params);
	if (rc != 1) { EVP_MAC_CTX_free(ctx); EVP_MAC_free(mac); throw pcsc::CipherError("EVP_MAC_init failed"); }

	EVP_MAC_update(ctx, data, len);

	size_t outLen = 0;
	EVP_MAC_final(ctx, nullptr, &outLen, 0);
	BYTEV out(outLen);
	EVP_MAC_final(ctx, out.data(), &outLen, out.size());
	out.resize(outLen);

	EVP_MAC_CTX_free(ctx);
	EVP_MAC_free(mac);
	return out;
}

BYTEV hmacSha256(const BYTEV& key, const BYTE* data, size_t len) { return hmac("SHA256", key, data, len); }
BYTEV hmacSha384(const BYTEV& key, const BYTE* data, size_t len) { return hmac("SHA384", key, data, len); }
BYTEV hmacSha512(const BYTEV& key, const BYTE* data, size_t len) { return hmac("SHA512", key, data, len); }

} // namespace crypto
