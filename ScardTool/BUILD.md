# scardtool — Build Kılavuzu

## Gereksinimler

| Platform | Araç |
|----------|------|
| Tüm | CMake >= 3.14, C++17, Git |
| Windows | Visual Studio 2019+ veya MSVC + Ninja, Windows SDK |
| Linux | `libpcsclite-dev`, GCC 9+ veya Clang 10+ |
| macOS | Xcode CLI Tools, `brew install cmake` |

**Linux kurulum:**
```bash
sudo apt-get install cmake libpcsclite-dev build-essential  # Ubuntu/Debian
sudo dnf install cmake pcsc-lite-devel gcc-c++              # Fedora
sudo pacman -S cmake ccid pcsclite                          # Arch
```

---

## Build — Preset ile (Önerilen)

```bash
# Konfigüre
cmake --preset linux-debug      # Linux geliştirme
cmake --preset linux-release    # Linux dağıtım
cmake --preset win-debug        # Windows geliştirme (/MD)
cmake --preset win-release      # Windows dağıtım (/MT + LTCG)

# Derle
cmake --build --preset linux-debug --target scardtool
cmake --build --preset win-release --target scardtool

# Test
ctest --preset linux-debug
```

## Build — Manuel

```bash
cd PCSCWorkshop
cmake -B build -DCMAKE_BUILD_TYPE=Debug   # geliştirme
cmake -B build -DCMAKE_BUILD_TYPE=Release # dağıtım
cmake --build build --target scardtool scardtool_tests -j$(nproc)
./build/ScardTool/Tests/scardtool_tests   # 286 test
```

---

## Windows /MT vs /MD Sorunu

OpenSSL Windows pre-built **`/MT`** (static CRT) ile gelir.
MSVC default **`/MD`** (dynamic CRT) kullanır → linker hatası.

**Çözüm:**
```bat
# Geliştirme: /MD (hızlı derleme)
cmake --preset win-debug

# Dağıtım: /MT (OpenSSL uyumlu, LTCG optimizasyonu)
cmake --preset win-release
```

---

## CMake Seçenekleri

| Seçenek | Varsayılan | Açıklama |
|---------|-----------|----------|
| `SCARDTOOL_USE_LINENOISE` | ON | linenoise-ng readline |
| `BUILD_TESTING` | ON | Test suite |
| `SCARDTOOL_ENABLE_LTCG` | OFF | /GL + /LTCG (win-release'de ON) |
| `CMAKE_MSVC_RUNTIME_LIBRARY` | — | MultiThreaded=/MT, MultiThreadedDLL=/MD |

```bash
cmake -B build -DSCARDTOOL_USE_LINENOISE=OFF  # Linenoise olmadan
cmake -B build -DBUILD_TESTING=OFF            # Test olmadan
```

---

## Test

```bash
./build/ScardTool/Tests/scardtool_tests                              # Tümü
./build/ScardTool/Tests/scardtool_tests --gtest_filter="ArgParser*"  # Filtreli
./build/ScardTool/Tests/scardtool_tests --gtest_filter="Payload*"   # Şifreleme
```

**Not:** Testler PCSC donanımı gerektirmez (dry-run + birim testleri).

---

## PC/SC Servisi

```bash
# Linux
sudo systemctl start pcscd
sudo systemctl enable pcscd   # Otomatik başlat
pcsc_scan                     # Reader tespiti

# Windows
# services.msc → "Smart Card" → Başlat
```

---

## MCP Kurulumu

### Claude Code `.mcp.json`:
```json
{
  "mcpServers": {
    "scardtool": {
      "command": "/path/to/build/ScardTool/scardtool",
      "args": ["--mcp"]
    }
  }
}
```

**Config dosya konumları:**
- Windows: `%APPDATA%\Claude\claude_desktop_config.json`
- macOS: `~/Library/Application Support/Claude/claude_desktop_config.json`
- Linux: `~/.config/Claude/claude_desktop_config.json`

### Test:
```bash
npx @modelcontextprotocol/inspector ./build/ScardTool/scardtool --mcp
```

---

## Sorun Giderme

| Sorun | Çözüm |
|-------|-------|
| `pcsclite not found` | `sudo apt-get install libpcsclite-dev` |
| `No readers found` | `sudo systemctl start pcscd` |
| `SCardEstablishContext failed` (Win) | services.msc → Smart Card → Başlat |
| linenoise build hatası | `-DSCARDTOOL_USE_LINENOISE=OFF` |
| `/MT /MD` linker hatası | `cmake --preset win-release` kullan |
