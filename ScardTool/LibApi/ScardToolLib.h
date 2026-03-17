/** @file ScardToolLib.h
 * @brief scardtool C API — library entegrasyonu için.
 *
 * Bu header scardtool'u statik ya da dinamik kütüphane olarak kullanan
 * C/C++ projeler için kararlı bir arayüz sunar.
 *
 * Ayrıca Lua ve Tcl extension'larının da kullandığı temel katmandır.
 *
 * @par Derleme Modları (CMake)
 * @code
 * # Standalone binary (her zaman mevcut)
 * cmake -DBUILD_STANDALONE=ON ..
 *
 * # C++ projesi için statik kütüphane
 * cmake -DBUILD_STATIC_LIB=ON ..
 *
 * # C++ projesi için dinamik kütüphane
 * cmake -DBUILD_SHARED_LIB=ON ..
 *
 * # Lua extension (require("scardtool"))
 * cmake -DBUILD_LUA_EXTENSION=ON ..
 *
 * # Tcl extension (package require scardtool)
 * cmake -DBUILD_TCL_EXTENSION=ON ..
 *
 * # Standalone + gömülü Lua
 * cmake -DBUILD_STANDALONE=ON -DEMBED_LUA=ON ..
 *
 * # Standalone + gömülü Tcl
 * cmake -DBUILD_STANDALONE=ON -DEMBED_TCL=ON ..
 * @endcode
 *
 * @par C++ Kullanımı (library mode)
 * @code
 * #include "ScardToolLib.h"
 *
 * auto ctx = scardtool_create();
 * scardtool_set_reader(ctx, 0);
 * scardtool_set_json(ctx, false);
 *
 * char result[4096];
 * int ec = scardtool_exec(ctx, "uid", result, sizeof(result));
 * printf("UID: %s (exit=%d)\n", result, ec);
 *
 * scardtool_destroy(ctx);
 * @endcode
 */
#pragma once

#include <stdint.h>
#include <stddef.h>

// ── Visibility ───────────────────────────────────────────────────────────────
#ifdef _WIN32
#  ifdef SCARDTOOL_BUILD_SHARED
#    define SCARDTOOL_API __declspec(dllexport)
#  elif defined(SCARDTOOL_USE_SHARED)
#    define SCARDTOOL_API __declspec(dllimport)
#  else
#    define SCARDTOOL_API
#  endif
#else
#  if defined(SCARDTOOL_BUILD_SHARED) || defined(SCARDTOOL_BUILD_LUA_EXT) || \
      defined(SCARDTOOL_BUILD_TCL_EXT)
#    define SCARDTOOL_API __attribute__((visibility("default")))
#  else
#    define SCARDTOOL_API
#  endif
#endif

#ifdef __cplusplus
extern "C" {
#endif

// ── Opaque context ────────────────────────────────────────────────────────────

/** @brief Scardtool oturumu. Thread-safe değildir;
 *  her thread kendi context'ini oluşturmalıdır. */
typedef struct scardtool_ctx scardtool_ctx;

// ── Lifecycle ─────────────────────────────────────────────────────────────────

/** @brief Yeni bir scardtool context oluştur. */
SCARDTOOL_API scardtool_ctx* scardtool_create(void);

/** @brief Context'i serbest bırak, PC/SC bağlantısını kapat. */
SCARDTOOL_API void scardtool_destroy(scardtool_ctx* ctx);

// ── Konfigürasyon ─────────────────────────────────────────────────────────────

/** @brief Reader index veya isim substring'i ayarla. */
SCARDTOOL_API void scardtool_set_reader(scardtool_ctx* ctx, int index);

/** @brief Reader isim substring'i ile ayarla. */
SCARDTOOL_API void scardtool_set_reader_name(scardtool_ctx* ctx, const char* name);

/** @brief JSON çıktı modu. */
SCARDTOOL_API void scardtool_set_json(scardtool_ctx* ctx, int enabled);

/** @brief Verbose log (stderr). */
SCARDTOOL_API void scardtool_set_verbose(scardtool_ctx* ctx, int enabled);

/** @brief Dry-run (donanıma dokunma). */
SCARDTOOL_API void scardtool_set_dry_run(scardtool_ctx* ctx, int enabled);

/** @brief Şifreleme etkinleştir. */
SCARDTOOL_API void scardtool_set_encrypt(scardtool_ctx* ctx, int enabled,
                                          const char* cipher, const char* password);

// ── Komut Çalıştırma ──────────────────────────────────────────────────────────

/**
 * @brief Tek bir scardtool komutu çalıştır.
 *
 * @param ctx      Context
 * @param cmdline  Tam komut satırı: "send-apdu -r 0 -a FFCA000000"
 * @param out_buf  Çıktı tamponu (NULL olabilir)
 * @param out_len  Tampon boyutu
 * @return ExitCode değeri (0 = başarı)
 *
 * @code
 * char buf[256];
 * int ec = scardtool_exec(ctx, "uid", buf, sizeof(buf));
 * @endcode
 */
SCARDTOOL_API int scardtool_exec(scardtool_ctx* ctx,
                                  const char*    cmdline,
                                  char*          out_buf,
                                  size_t         out_len);

/**
 * @brief .scard script çalıştır (dosyadan).
 * @return Son komutun exit code'u
 */
SCARDTOOL_API int scardtool_run_file(scardtool_ctx* ctx, const char* path);

/**
 * @brief .scard script çalıştır (string'den).
 * @return Son komutun exit code'u
 */
SCARDTOOL_API int scardtool_run_string(scardtool_ctx* ctx, const char* script);

// ── APDU Yardımcıları ─────────────────────────────────────────────────────────

/**
 * @brief Ham APDU gönder.
 *
 * @param ctx       Context
 * @param apdu_hex  Hex string: "FF CA 00 00 00" veya "FFCA000000"
 * @param resp_buf  Yanıt tamponu (hex string olarak doldurulur)
 * @param resp_len  Tampon boyutu
 * @return 0 başarı, <0 hata
 */
SCARDTOOL_API int scardtool_send_apdu(scardtool_ctx* ctx,
                                       const char*    apdu_hex,
                                       char*          resp_buf,
                                       size_t         resp_len);

/** @brief Son hata mesajını döndür. */
SCARDTOOL_API const char* scardtool_last_error(scardtool_ctx* ctx);

/** @brief Kütüphane versiyon string'i. */
SCARDTOOL_API const char* scardtool_version(void);

// ── Reader Listeleme ──────────────────────────────────────────────────────────

/**
 * @brief Bağlı reader'ları listele.
 *
 * @param ctx      Context
 * @param names    String dizisi (her biri NULL-terminated)
 * @param max      Dizi kapasitesi
 * @return Bulunan reader sayısı, <0 hata
 */
SCARDTOOL_API int scardtool_list_readers(scardtool_ctx* ctx,
                                          char           names[][256],
                                          int            max);

#ifdef __cplusplus
}  // extern "C"
#endif

// ── C++ Wrapper (isteğe bağlı) ────────────────────────────────────────────────
#ifdef __cplusplus
#include <string>
#include <vector>
#include <stdexcept>

/** @brief C++ RAII wrapper — scardtool_ctx üzerinde. */
class ScardTool {
public:
    ScardTool() : ctx_(scardtool_create()) {
        if (!ctx_) throw std::runtime_error("scardtool_create failed");
    }
    ~ScardTool() { scardtool_destroy(ctx_); }

    ScardTool(const ScardTool&) = delete;
    ScardTool& operator=(const ScardTool&) = delete;

    void setReader(int index)              { scardtool_set_reader(ctx_, index); }
    void setReader(const std::string& n)   { scardtool_set_reader_name(ctx_, n.c_str()); }
    void setJson(bool v)                   { scardtool_set_json(ctx_, v); }
    void setVerbose(bool v)                { scardtool_set_verbose(ctx_, v); }
    void setDryRun(bool v)                 { scardtool_set_dry_run(ctx_, v); }

    /** @brief Komut çalıştır, çıktıyı string olarak döndür. */
    std::string exec(const std::string& cmdline) {
        char buf[8192];
        int ec = scardtool_exec(ctx_, cmdline.c_str(), buf, sizeof(buf));
        if (ec != 0)
            throw std::runtime_error(std::string("scardtool: ") +
                                     scardtool_last_error(ctx_));
        return std::string(buf);
    }

    /** @brief APDU gönder, yanıtı hex string olarak döndür. */
    std::string sendApdu(const std::string& apduHex) {
        char buf[4096];
        int ec = scardtool_send_apdu(ctx_, apduHex.c_str(), buf, sizeof(buf));
        if (ec != 0)
            throw std::runtime_error(std::string("send_apdu: ") +
                                     scardtool_last_error(ctx_));
        return std::string(buf);
    }

    std::vector<std::string> listReaders() {
        char names[16][256];
        int n = scardtool_list_readers(ctx_, names, 16);
        std::vector<std::string> result;
        for (int i = 0; i < n; ++i) result.emplace_back(names[i]);
        return result;
    }

private:
    scardtool_ctx* ctx_;
};
#endif  // __cplusplus
