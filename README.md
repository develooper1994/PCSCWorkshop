# PCSC Workshop — Smart Card Reader & DESFire Interface

High-performance C++17 library for smart card communication via PC/SC, with support for Mifare Classic/Ultralight and DESFire encrypted authentication.

**Platforms:** Windows (MSVC), Linux (GCC/Clang)  
**Build:** Visual Studio 2022 + CMake 3.14+  
**IDE:** Visual Studio Code (full debug/deploy support)

---

## Quick Start

### Windows

```bash
git clone <repo>
cd PCSCWorkshop
PCSC_workshop.sln              # Open in Visual Studio 2022
```

Or CMake:
```bash
cmake -S . -B build -G "Visual Studio 17 2022"
cmake --build build --config Debug
```

### Linux

```bash
sudo apt install libpcsclite-dev
git clone <repo>
cd PCSCWorkshop
cmake -S . -B build
cmake --build build
./build/Tests/PCSC_tests
```

### VS Code

```bash
code .
# Ctrl+Shift+B → CMake: Build All
# F5 → Debug Workshop1 or Tests
```

---

## Build Systems

### Visual Studio 2022
Direct `.sln` support. All projects inherit C++17 from `Directory.Build.props`.

### CMake (both platforms)
```bash
# Preset-based:
cmake --preset win-x64-debug    # Windows
cmake --preset linux-debug      # Linux

# Or manual:
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
```

---

## Architecture

```
Utils               PCSC context, reader discovery, cross-platform abstraction
Cipher              XorCipher, CaesarCipher, CngAES/3DES (Win-only)
Reader              ACR1281UReader for Mifare/NTAG cards
Card                CardIO (Mifare model), DesfireAuth/Commands
Workshop1           Main application executable
Tests               Unit & integration test suite
```

Dependencies: `Utils ← Cipher ← Reader ← Card ← {Workshop1, Tests}`

---

## Features

- ✅ **Mifare Classic** — Read/write with key auth & access control
- ✅ **Mifare Ultralight / NTAG** — Page-based I/O
- ✅ **DESFire** — Mutual 3-pass authentication (AES-128, 3DES), commands, secure messaging
- ✅ **Cross-platform** — Windows (MSVC) + Linux (GCC/Clang)
- ✅ **Exception-free** — `Result<T>` pattern for error handling
- ✅ **Template callbacks** — No `std::function` (performance/clarity)

---

## Code Standards

- **C++17** enforced via `Directory.Build.props` (MSVC) & root `CMakeLists.txt` (CMake)
- **Format:** clang-format (Microsoft style, Allman braces, tab indent)
- **Error handling:** `Result<T>` for non-throwing paths; `.unwrap()` for throwing versions
- **Cross-platform:** `Platform.h` for PCSC includes; `#ifdef _WIN32` guards where needed
- **No `std::function`** — use templates or function pointers

---

## Libraries

| Library | Platform | Usage |
|---------|----------|-------|
| winscard (PC/SC) | Windows: `winscard.lib` / Linux: `libpcsclite` | Smart card I/O |
| bcrypt (CNG) | Windows only | AES, 3DES, CMAC |

---

## Development

### Feature Branch Workflow
```bash
# New feature (never on main):
git checkout -b feature/xyz
# ... develop, test, build ...
git add . && git commit -m "feat: xyz"
git push origin feature/xyz
# ... merge to main after review ...
git checkout main && git merge feature/xyz
git push origin main
git branch -d feature/xyz && git push origin --delete feature/xyz

# Small fixes (main):
git add . && git commit -m "fix: bug" && git push origin main
```

### Build Validation
After any changes:
```bash
# Visual Studio
msbuild PCSC_workshop.sln

# CMake
cmake --build build
```

---

## Debugging

**Visual Studio:** F5 (built-in debugger)  
**VS Code (Linux/CMake):** F5 (GDB via `.vscode/launch.json`)

Enable logging:
```cpp
pcsc::Log::getInstance().setLogLevel(pcsc::LogLevel::Debug);
pcsc::Log::getInstance().enableAllCategories();
```

---

## Troubleshooting

| Issue | Solution |
|-------|----------|
| `winscard.lib` not found (Windows) | Install Windows SDK (VS 2022 includes it) |
| `PCSC/winscard.h` not found (Linux) | `sudo apt install libpcsclite-dev` |
| `CardDataTypes.h` not found (CMake) | Regenerate: `cmake -S . -B build` |
| Reader not detected | Check driver (ACR1281U needs ACR drivers) |

---

## Documentation

- **Code guidelines:** `.github/copilot-instructions.md`
- **Log system:** `Utils/Utils/Log/README.md`
- **Format:** `.clang-format`, `.editorconfig`
- **VS Code:** `.vscode/` (tasks, launch, settings)

---

## License

[Your License]

**Status:** Active development  
**Maintainer:** [Team/Person]
