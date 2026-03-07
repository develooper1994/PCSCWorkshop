#include "Kdf.h"
#include "Exceptions/GenericExceptions.h"
#include <openssl/evp.h>
#include <openssl/kdf.h>
#include <openssl/core_names.h>
#include <openssl/params.h>

namespace crypto {

BYTEV pbkdf2Sha256(const std::string& password, const BYTEV& salt,
                   int iterations, size_t keyLen) {
	BYTEV out(keyLen);
	if (PKCS5_PBKDF2_HMAC(password.c_str(), static_cast<int>(password.size()),
	                       salt.data(), static_cast<int>(salt.size()),
	                       iterations, EVP_sha256(),
	                       static_cast<int>(keyLen), out.data()) != 1)
		throw pcsc::CipherError("PKCS5_PBKDF2_HMAC failed");
	return out;
}

BYTEV hkdf(const BYTEV& ikm, const BYTEV& salt, const BYTEV& info,
           size_t outLen) {
	EVP_KDF* kdf = EVP_KDF_fetch(nullptr, "HKDF", nullptr);
	if (!kdf) throw pcsc::CipherError("EVP_KDF_fetch(HKDF) failed");

	EVP_KDF_CTX* ctx = EVP_KDF_CTX_new(kdf);
	if (!ctx) { EVP_KDF_free(kdf); throw pcsc::CipherError("EVP_KDF_CTX_new failed"); }

	OSSL_PARAM params[5];
	int idx = 0;
	params[idx++] = OSSL_PARAM_construct_utf8_string(OSSL_KDF_PARAM_DIGEST, const_cast<char*>("SHA256"), 0);
	params[idx++] = OSSL_PARAM_construct_octet_string(OSSL_KDF_PARAM_KEY, const_cast<BYTE*>(ikm.data()), ikm.size());
	if (!salt.empty())
		params[idx++] = OSSL_PARAM_construct_octet_string(OSSL_KDF_PARAM_SALT, const_cast<BYTE*>(salt.data()), salt.size());
	if (!info.empty())
		params[idx++] = OSSL_PARAM_construct_octet_string(OSSL_KDF_PARAM_INFO, const_cast<BYTE*>(info.data()), info.size());
	params[idx] = OSSL_PARAM_construct_end();

	BYTEV out(outLen);
	int rc = EVP_KDF_derive(ctx, out.data(), outLen, params);

	EVP_KDF_CTX_free(ctx);
	EVP_KDF_free(kdf);

	if (rc != 1) throw pcsc::CipherError("EVP_KDF_derive(HKDF) failed");
	return out;
}

} // namespace crypto
