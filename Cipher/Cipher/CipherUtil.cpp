#include "CipherUtil.h"
#include "Exceptions/GenericExceptions.h"
#include <openssl/evp.h>

EVP_CIPHER_CTX* acquireThreadLocalCtx() {
	thread_local struct CtxHolder {
		EVP_CIPHER_CTX* ctx;
		CtxHolder() : ctx(EVP_CIPHER_CTX_new()) {}
		CtxHolder(const CtxHolder&) = delete;
		~CtxHolder() { if (ctx) EVP_CIPHER_CTX_free(ctx); }
		CtxHolder& operator=(const CtxHolder&) = delete;
	} holder;
	return holder.ctx;
}

EVP_CIPHER_CTX* acquireThreadLocalCtxChecked() {
	EVP_CIPHER_CTX* ctx = acquireThreadLocalCtx();
	if (!ctx) throw pcsc::CipherError("EVP_CIPHER_CTX thread-local allocation failed");
	return ctx;
}
