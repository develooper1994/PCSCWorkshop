#ifndef PCSC_WORKSHOP1_CIPHER_CNGBLOCKCIP_H
#define PCSC_WORKSHOP1_CIPHER_CNGBLOCKCIP_H

#include "Types.h"
#include <vector>
#include <cstddef>

// ════════════════════════════════════════════════════════════════════════════════
// CngBlockCipher — Raw AES/3DES CBC (no padding)
// ════════════════════════════════════════════════════════════════════════════════
//
// DESFire mutual auth ve secure messaging için gerekli olan
// "tam blok" CBC şifreleme/deşifreleme.
//
// Farklar (mevcut CngAES/Cng3DES'ten):
//   - Padding yok (BCRYPT_BLOCK_PADDING kullanılmaz)
//   - Input uzunluğu blok boyutunun katı olmalı (16 AES, 8 3DES)
//   - IV her çağrıda bağımsız kopyalanır (in-place modifikasyon engellenir)
//   - Stateless: her çağrı kendi key handle'ını açar/kapar
//
// Kullanım:
//   BYTEV key = {0x00,...,0x00};   // 16 byte AES-128
//   BYTEV iv(16, 0);               // zero IV
//   BYTEV enc = CngBlockCipher::encryptAES(key, iv, plaintext);
//   BYTEV dec = CngBlockCipher::decryptAES(key, iv, ciphertext);
//
// ════════════════════════════════════════════════════════════════════════════════

class CngBlockCipher {
public:
    // ── AES-128 CBC (no padding) ────────────────────────────────────────────
    //   key: 16 bytes, iv: 16 bytes, data: N*16 bytes

    static BYTEV encryptAES(const BYTEV& key, const BYTEV& iv,
                            const BYTE* data, size_t len);
    static BYTEV decryptAES(const BYTEV& key, const BYTEV& iv,
                            const BYTE* data, size_t len);

    // Convenience
    static BYTEV encryptAES(const BYTEV& key, const BYTEV& iv, const BYTEV& data);
    static BYTEV decryptAES(const BYTEV& key, const BYTEV& iv, const BYTEV& data);

    // ── 2K3DES CBC (no padding) ─────────────────────────────────────────────
    //   key: 16 bytes (2K3DES), iv: 8 bytes, data: N*8 bytes

    static BYTEV encrypt2K3DES(const BYTEV& key, const BYTEV& iv,
                               const BYTE* data, size_t len);
    static BYTEV decrypt2K3DES(const BYTEV& key, const BYTEV& iv,
                               const BYTE* data, size_t len);

    static BYTEV encrypt2K3DES(const BYTEV& key, const BYTEV& iv, const BYTEV& data);
    static BYTEV decrypt2K3DES(const BYTEV& key, const BYTEV& iv, const BYTEV& data);

    // ── 3K3DES CBC (no padding) ─────────────────────────────────────────────
    //   key: 24 bytes (3K3DES), iv: 8 bytes, data: N*8 bytes

    static BYTEV encrypt3K3DES(const BYTEV& key, const BYTEV& iv,
                               const BYTE* data, size_t len);
    static BYTEV decrypt3K3DES(const BYTEV& key, const BYTEV& iv,
                               const BYTE* data, size_t len);

    static BYTEV encrypt3K3DES(const BYTEV& key, const BYTEV& iv, const BYTEV& data);
    static BYTEV decrypt3K3DES(const BYTEV& key, const BYTEV& iv, const BYTEV& data);

    // ── AES CMAC (OMAC1) ────────────────────────────────────────────────────
    //   key: 16 bytes, data: arbitrary length
    //   Returns 16-byte MAC

    static BYTEV cmacAES(const BYTEV& key, const BYTE* data, size_t len);
    static BYTEV cmacAES(const BYTEV& key, const BYTEV& data);

    // ── Block sizes ─────────────────────────────────────────────────────────

    static constexpr size_t AES_BLOCK  = 16;
    static constexpr size_t DES_BLOCK  = 8;

private:
    CngBlockCipher() = delete;  // static-only class
};

#endif // PCSC_WORKSHOP1_CIPHER_CNGBLOCKCIP_H
