#ifndef PCSC_WORKSHOP1_CIPHER_CIPHERS_H
#define PCSC_WORKSHOP1_CIPHER_CIPHERS_H

#include "XorCipher.h"
#include "CaesarCipher.h"
#include "ICipherAAD.h"

#ifdef _WIN32
#include "CngAES.h"
#include "Cng3DES.h"
#include "CngAESGcm.h"
#include "CngAESGcmUtil.h"
#endif

// Convenience header to include all cipher implementations

#endif // PCSC_WORKSHOP1_CIPHER_CIPHERS_H
