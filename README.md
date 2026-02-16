PCSC Workshop — Smartcard reader utilities
=========================================

A small Windows C++14 project demonstrating PC/SC smartcard communication and a pluggable reader/cipher architecture.

Project highlights
- Reader abstraction with PIMPL: `Reader` / `ACR1281UReader` (reader-specific APDU read/write logic).
- Cipher strategy: `ICipher` interface with an example `XorCipher` implementation. Ciphers expose a `test()` method to self-verify correctness.
- Separation of concerns: the reader code is independent from encryption; new cipher implementations can be plugged in easily.

Files of interest
- `PCSC_workshop1.cpp` — program entry, reader selection and test harness.
- `CardConnection.h` / `PcscUtils.h` — PC/SC wrapper utilities and helper functions.
- `Reader.h` / `Reader.cpp` — Reader abstraction and `ACR1281UReader` implementation (PIMPL).
- `Cipher.h` / `Cipher.cpp` — `ICipher` strategy and `XorCipher` implementation.

Prerequisites
- Windows (desktop) with PC/SC runtime (Winscard).
- Visual Studio (recommended) or msbuild; project targets x86/x64 configurations.
- `winscard.lib` is required and referenced in the headers.

Build
- Open the solution `PCSC_workshop1` in Visual Studio and build.
- Or use msbuild from the repo root:

  `msbuild PCSC_workshop1\PCSC_workshop1.vcxproj /p:Configuration=Debug /p:Platform=x64`

Run
1. Insert a smartcard-compatible reader with a tag.
2. Run the `PCSC_workshop1` executable from Visual Studio or command line. The program will list readers and prompt selection.
3. The `TEST` routine in `PCSC_workshop1.cpp` runs some DESFire queries and reader read/write demonstrations.

Tests
- `XorCipher::test()` is executed during the secured reader test (`ACR1281UReader::testACR1281UReaderSecured`) before any card writes. The test checks roundtrip encrypt/decrypt, empty input handling and multi-block behavior.
- To add unit tests or other cipher tests, implement `bool test() const` for new `ICipher` types and invoke them where appropriate.

Extending
- Add new cipher implementations by deriving from `ICipher` and implementing `encrypt`, `decrypt`, and `test`.
- To use a new cipher in secured flows, construct the cipher and pass it to `Reader::writePageEncrypted` / `Reader::readPageDecrypted`.

License & Contribution
- This project is a workshop/demo. Modify and extend for your needs. Contributions and improvements are welcome.

