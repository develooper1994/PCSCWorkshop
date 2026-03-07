#include "Random.h"
#include "Exceptions/GenericExceptions.h"
#include <openssl/rand.h>

namespace crypto {

BYTEV randomBytes(size_t n) {
	BYTEV buf(n);
	if (n == 0) return buf;
	if (RAND_bytes(buf.data(), static_cast<int>(n)) != 1)
		throw pcsc::CipherError("RAND_bytes failed");
	return buf;
}

} // namespace crypto
