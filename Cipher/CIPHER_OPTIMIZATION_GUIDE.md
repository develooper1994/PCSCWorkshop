# Cipher Optimization Guide

Bu doküman `AesCbcCipher` üzerinde yapılan optimizasyonları ve diğer cipher sınıflarına nasıl uygulanacağını açıklar.

## Uygulanan Optimizasyonlar

### 1. Validate-Before-Allocate
**Sorun:** Constructor'da `make_unique<Impl>` çağrılıp ardından validasyon yapılıyordu. Geçersiz parametre geldiğinde Impl boşuna allocate/destruct ediliyordu.

**Çözüm:** Önce validasyon, sonra `make_unique`.
```cpp
// ÖNCE
MyClass(const BYTEV& key, const BYTEV& iv)
    : pImpl(std::make_unique<Impl>(key, iv)) {
    if (iv.size() != 16) throw ...;  // Impl zaten allocate oldu
}

// SONRA
MyClass(const BYTEV& key, const BYTEV& iv)
    : pImpl(nullptr) {
    if (iv.size() != 16) throw ...;  // Ucuz — sadece karşılaştırma
    pImpl = std::make_unique<Impl>(key, iv);
}
```

### 2. Cache Cipher Pointer
**Sorun:** `EVP_aes_128_cbc()` gibi fonksiyonlar OpenSSL 3.x'te internal lookup yapıyor. Her encrypt/decrypt'te tekrarlanıyordu.

**Çözüm:** `Impl` struct'ında `const EVP_CIPHER* cipher` olarak bir kez resolve edilir.
```cpp
struct Impl {
    const EVP_CIPHER* cipher;
    Impl(const BYTEV& k) : cipher(pickAes(k.size())) {}
};
```

### 3. Stack-Based IV Copy
**Sorun:** `BYTEV ivCopy = iv` her çağrıda heap allocation yapıyordu (vector kopyalama).

**Çözüm:** IV boyutu sabit olduğundan `BYTE ivBuf[BLOCK_SIZE]` + `memcpy` kullanılır.
```cpp
// Impl'de:
BYTE iv[BLOCK_SIZE];   // BYTEV yerine fixed array

// Metotlarda:
BYTE ivBuf[BLOCK_SIZE];
std::memcpy(ivBuf, pImpl->iv, BLOCK_SIZE);
```

### 4. Thread-Local EVP_CIPHER_CTX
**Sorun:** Her encrypt/decrypt'te `EVP_CIPHER_CTX_new()` + `EVP_CIPHER_CTX_free()` çağrılıyordu (heap alloc).

**Çözüm:** `thread_local` holder ile her thread bir kez allocate eder.
```cpp
static EVP_CIPHER_CTX* acquireThreadLocalCtx() {
    thread_local struct CtxHolder {
        EVP_CIPHER_CTX* ctx;
        CtxHolder() : ctx(EVP_CIPHER_CTX_new()) {}
        ~CtxHolder() { if (ctx) EVP_CIPHER_CTX_free(ctx); }
        CtxHolder(const CtxHolder&) = delete;
        CtxHolder& operator=(const CtxHolder&) = delete;
    } holder;
    return holder.ctx;
}
```
> **Dikkat:** `EVP_EncryptInit_ex`/`EVP_DecryptInit_ex` her çağrıda CTX'i full reset eder — güvenlidir.

### 5. encryptIntoSized / decryptIntoSized Override
**Sorun:** ICipher base'de default implementasyon `encrypt()` → temp vector → copy yapıyordu.

**Çözüm:** Asıl işi `IntoSized`'da yapmak, `encrypt()`/`decrypt()` onu çağırır.
```cpp
BYTEV encrypt(const BYTE* data, size_t len) const override {
    BYTEV out(len + BLOCK_SIZE);
    size_t written = encryptIntoSized(data, len, out.data(), out.size());
    out.resize(written);
    return out;
}
```
> **Not:** Yalnızca output boyutu önceden bilinebilen cipher'lara uygulanabilir (CBC, CTR). GCM gibi nonce||ct||tag üreten modlarda zorlayıcı olabilir.

### 6. Doğru Output Buffer Boyutları
| Mod | Encrypt buffer | Decrypt buffer |
|-----|---------------|---------------|
| CBC (padded) | `len + BLOCK_SIZE` | `len` |
| CTR (stream) | `len` | `len` |
| GCM | `NONCE + len + TAG` | `len - NONCE - TAG` |
| 3DES-CBC | `len + DES_BLOCK` | `len` |

## Uygulanabilirlik Matrisi

| Optimizasyon | AesCbcCipher | AesCtrCipher | AesGcmCipher | TripleDesCbcCipher |
|---|---|---|---|---|
| Validate-before-allocate | ✅ | ✅ | ✅ | ✅ |
| Cache cipher pointer | ✅ | ✅ | ✅ | ✅ (tek cipher) |
| Stack IV copy | ✅ (16B) | ✅ (16B) | N/A (random nonce) | ✅ (8B) |
| Thread-local CTX | ✅ | ✅ | ✅ | ✅ |
| IntoSized override | ✅ | ✅ | ❌ (nonce||ct||tag layout) | ✅ |
| Buffer boyutu düzeltme | ✅ | ✅ | ✅ | ✅ |

## Exception Safety Notu

`Result<T>` yapısı PCSC protokol katmanına özel (`StatusWord`, `ErrorCode`). Cipher katmanında
exception doğru yaklaşımdır çünkü:
- Hata durumları programlama hatası (yanlış key boyutu) veya veri bozulması (bad padding)
- Tüm caller'lar exception bekliyor
- `CipherError` zaten `std::runtime_error`'dan türüyor
