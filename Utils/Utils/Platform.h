#ifndef PCSC_PLATFORM_H
#define PCSC_PLATFORM_H

// ════════════════════════════════════════════════════════════════════════════════
// Platform.h — Windows / Linux PCSC tip ve include soyutlaması
// ════════════════════════════════════════════════════════════════════════════════
//
// Windows : <windows.h> + <winscard.h>   (winscard.lib)
// Linux   : <PCSC/winscard.h>            (libpcsclite)
// macOS   : <PCSC/winscard.h>            (PCSC.framework)
//
// Bu başlık dahil edildikten sonra şu tipler her platformda mevcuttur:
//   SCARDCONTEXT, SCARDHANDLE, DWORD, LONG, BYTE, LPWSTR, SCARD_IO_REQUEST,
//   SCARD_S_SUCCESS, SCARD_SHARE_SHARED, SCARD_PROTOCOL_T0/T1, ...
//
// ════════════════════════════════════════════════════════════════════════════════

#ifdef _WIN32
    // ── Windows ─────────────────────────────────────────────────────────────
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #include <windows.h>
    #include <winscard.h>

    #if defined(_MSC_VER)
        #pragma comment(lib, "winscard.lib")
    #endif

#elif defined(__APPLE__)
    // ── macOS ───────────────────────────────────────────────────────────────
    #include <PCSC/winscard.h>
    #include <PCSC/wintypes.h>

#else
    // ── Linux (pcsclite) ────────────────────────────────────────────────────
    #include <PCSC/winscard.h>
    #include <PCSC/wintypes.h>

    // pcsclite LPWSTR ile wchar_t kullanmaz — narrow string ile çalışır.
    // Projede wstring kullanan API'ler (SCardListReadersW, SCardConnectW)
    // Linux'ta narrow varyantlarına düşer: SCardListReaders, SCardConnect.
    //
    // Linux uyumluluğu için PCSC.cpp'de #ifdef _WIN32 guard'ları mevcuttur.

#endif

#endif // PCSC_PLATFORM_H
