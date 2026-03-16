# scardtool — Geliştirme Planı (v5)

## Proje Özeti
PCSCWorkshop içinde tek binary, çok modlu PC/SC komut aracı.
MCP server, CLI aracı ve interaktif shell tek binary'de birleşiyor.

---

## Mimari

```
PCSCWorkshop/
├── CMakeLists.txt          ← root (Utils+Cipher+Reader+Card+McpServer+ScardTool)
├── CMakePresets.json       ← win-debug|win-release|linux-debug|linux-release|...
├── Utils/                  ← PCSC.h, Result.h, BasicError.h
├── Cipher/                 ← AES-CBC/CTR/GCM, 3DES-CBC, XOR, PBKDF2, HKDF
├── Reader/                 ← Reader.h, PcscCommands.h, ACR1281U
├── Card/                   ← CardInterface, CardMemoryLayout, DESFire, CardDataTypes.h
│     [NOT: Kart simülasyonu için gelecekte cardsim projesi planlanıyor]
├── McpServer/              ← Standalone MCP server (referans)
├── Tests/                  ← LEGACY: Gerçek PC/SC reader gerektirir, build dışı
├── Workshop1/              ← LEGACY: Manuel denemeler, build dışı
└── ScardTool/
    ├── CMakeLists.txt
    ├── PLAN.md
    ├── README.md
    ├── BUILD.md
    ├── Doxyfile
    ├── main.cpp
    ├── ExitCode.h
    ├── EnvVars.h           ← SCARDTOOL_* çevre değişkenleri
    ├── ArgParser/ArgParser.h
    ├── Output/Formatter.h
    ├── Session/SessionFile.h
    ├── LineEditor/ILineEditor.h
    ├── App/App.h
    ├── Card/               ← CardMath, TrailerFormat, AccessBitsHelper
    ├── Catalog/            ← StatusWordCatalog, InsCatalog, ApduMacro, MacroRegistry, ApduCatalog
    ├── Commands/           ← ICommand (öncelik zinciri), Commands.h (23 komut), CommandRegistry.h
    ├── Crypto/             ← CardCrypto, PayloadCipher
    ├── Script/ScriptRunner.h
    ├── Modes/              ← InteractiveMode (history kalıcı), McpMode (23 MCP tool)
    └── Tests/              ← 286 test (GTest, dry-run, PCSC gerektirmez)
```

---

## Parametre Öncelik Zinciri

```
1. Interactive prompt   — kullanıcı o an yazıyor (en yüksek)
2. CLI args            — --reader 0, -r 0
3. Env variable        — SCARDTOOL_READER=0
4. Session file        — .scardtool_session (--session aktifse)
```

---

## Parola Güvenliği (Düşükten Yükseğe)

| Yöntem | Güvenlik | Kullanım |
|--------|----------|---------|
| `-P pass` CLI | ❌ shell history'de görünür | Sadece test |
| Session dosyası | ❌ disk'te düz metin | Uyarı verilir |
| `SCARDTOOL_PASSWORD` env | ⚠️ /proc/pid/environ okunabilir | Script/CI |
| `-W / --password-prompt` | ✅ echo-off, hiçbir yerde görünmez | **Önerilen** |

---

## Modlar

| Mod | Nasıl | Persistent? | History? |
|-----|-------|-------------|---------|
| Terminal | `scardtool <cmd>` | Hayır | Hayır |
| Interactive | `scardtool interactive` | Evet | ✅ ~/.scardtool_history |
| Script | `scardtool -f file.scard` / stdin / `-e` | Hayır | Hayır |
| MCP | `scardtool --mcp` | Evet | Hayır |

---

## Çevre Değişkenleri

| Değişken | Karşılığı | Not |
|----------|-----------|-----|
| `SCARDTOOL_READER` | `--reader / -r` | |
| `SCARDTOOL_PASSWORD` | `--password / -P` | Güvenli — history'de görünmez |
| `SCARDTOOL_CIPHER` | `--cipher` | |
| `SCARDTOOL_KEY` | `--key / -k` | Mifare key hex |
| `SCARDTOOL_JSON` | `--json / -j` | `1` = aktif |
| `SCARDTOOL_VERBOSE` | `--verbose / -v` | `1` = aktif |
| `SCARDTOOL_NO_WARN` | `--no-warn / -q` | `1` = aktif |
| `SCARDTOOL_ENCRYPT` | `--encrypt / -E` | `1` = aktif |
| `SCARDTOOL_DRY_RUN` | `--dry-run` | `1` = aktif |

---

## Komutlar (23 adet)

### CLI + MCP

| Komut | Required | Optional |
|-------|----------|---------|
| `list-readers` | — | `-j` |
| `connect` | `-r` | `--retry-ms`, `--max-retries` |
| `disconnect` | — | — |
| `uid` | `-r` | `-j` |
| `ats` | `-r` | `-j` |
| `card-info` | `-r` | `-c`, `--save-session`, `-j` |
| `send-apdu` | `-r`, `-a` | `--no-chain`, `-j` |
| `load-key` | `-r`, `-k` | `-n`, `-L/--volatile` |
| `auth` | `-r`, `-b` | `-t A/B`, `-n`, `-G` |
| `read` | `-r`, `-p` | `-l`, `-E`, `-P`, `-j` |
| `write` | `-r`, `-p`, `-d` | `-E`, `-P`, `--cipher` |
| `read-sector` | `-r`, `-s`, `-k` | `-t`, `-c`, `-E`, `-P` |
| `write-sector` | `-r`, `-s`, `-k`, `-d` | `-t`, `-c`, `-E`, `-P` |
| `read-trailer` | `-r`, `-s`, `-k` | `-t` |
| `write-trailer` | `-r`, `-s`, `--key-a`, `--key-b`, `--access` | — |
| `make-access` | — | `--b0..--bt`, `--table`, `-i`, `-j` |
| `detect-cipher` | `-r` | `--save-session`, `-j` |
| `explain-sw` | `<SW>` | `-j` |
| `explain-apdu` | `<hex|MACRO>` | `-j` |
| `list-macros` | — | `-g`, `--filter`, `-j` |
| `explain-macro` | `<NAME>` | `-j` |
| `begin-transaction` | `-r` | — |
| `end-transaction` | — | `-d leave/reset/unpower/eject` |

### Şifreleme Kısıtları

- Trailer blokları (`blk % 4 == 3`) **asla** şifrelenmez
- AES-GCM → +16B tag → Classic/Ultralight için uygun değil (sadece DESFire)
- AES-CBC / 3DES-CBC → PKCS#7 padding → sabit blok boyutunda sorun
- Kart blokları için en güvenli: **AES-128-CTR** (çıktı = giriş boyutu)
- IV karta yazılmaz: `PBKDF2(password+UID) + HKDF(block_addr)` ile deterministik

---

## MCP Tools (23 adet)

`list_readers`, `connect`, `disconnect`, `uid`, `ats`, `card_info`,
`send_apdu`, `load_key`, `auth`, `read`, `write`, `read_sector`,
`write_sector`, `read_trailer`, `make_access`, `detect_cipher`,
`explain_sw`, `explain_apdu`, `list_macros`, `explain_macro`,
`begin_transaction`, `end_transaction`

---

## Windows Build Grupları

### Grup 1: Hızlı geliştirme (`win-debug`, `win-relwithdebinfo`)
- `/MDd` veya `/MD` — dynamic CRT
- No LTCG — hızlı link
- Preset: `cmake --preset win-debug`

### Grup 2: Dağıtım (`win-release`)
- `/MT` — static CRT, **OpenSSL pre-built ile uyumlu**
- LTCG (`/GL` + `/LTCG`) — bütün program optimizasyonu
- Preset: `cmake --preset win-release`

> **Not:** OpenSSL Windows pre-built /MT ile gelir. Release binary'si
> `/MT` kullanmazsa linker hatası alırsın.

---

## TODO — Yakın Vadeli

- [ ] `read-sector` / `write-sector` dry-run'da cipher gösterimi (tamamlandı ✅)
- [ ] `card-info --save-session` kart tipini kaydeder (tamamlandı ✅)
- [ ] `.scard` script'te değişken desteği (`$READER`, `$KEY`)
- [ ] Shell completion (`--completion bash/zsh/fish`)
- [ ] `send-apdu` 61xx GET RESPONSE auto-chain (tamamlandı ✅)

## TODO — Simülasyon (Kullanıcı onayı bekleniyor)

cardsim — ayrı program, CardInterface üzerine PC/SC virtual reader:
```
scardtool ↔ PC/SC API ↔ cardsim (sanal reader)
                              ↑
                        CardInterface (Card/ projesi)
```

## TODO — Uzak Vadeli (Sadece kullanıcı söylediğinde)

- [ ] Renk desteği (`--color`, `--no-color`): hata=kırmızı, uyarı=sarı, bilgi=yeşil
- [ ] Codegen: `.scard` script → C/C++ kaynak kodu
- [ ] MCP TCP socket transport
- [ ] Sistem keyring entegrasyonu (libsecret / Windows Credential Store)
