# scardtool

PC/SC akilli kart komut satiri araci ve MCP server.
Terminal, interaktif shell, script ve MCP server modlarini tek binary'de sunar.

## Hizli Baslangic

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --target scardtool -j4
./build/ScardTool/scardtool list-readers
./build/ScardTool/scardtool card-info -r 0
./build/ScardTool/scardtool send-apdu -r 0 -a GETUID
```

Tam dokumantasyon: [BUILD.md](BUILD.md)

## Global Secenekler

```
-S, --session        .scardtool_session dosyasini yukle
    --save-session   Parametreleri session'a kaydet
-j, --json           JSON cikti
-v, --verbose        Detayli log (APDU parse + SW aciklama)
    --dry-run        Donanima dokunmadan test
-q, --no-warn        Uyarilari bastir
-E, --encrypt        Sifrele/coz
    --cipher <algo>  detect|aes-ctr|aes-cbc|aes-gcm|3des-cbc|xor|none
-P, --password <pw>  Sifreleme parolasi
```

## Komutlar

### Temel
```bash
scardtool list-readers
scardtool connect    -r 0
scardtool disconnect
scardtool uid        -r 0
scardtool ats        -r 0
scardtool card-info  -r 0
```

### Ham APDU
```bash
scardtool send-apdu -r 0 -a FFCA000000    # hex
scardtool send-apdu -r 0 -a GETUID         # makro
scardtool send-apdu -r 0 -a "READ_BINARY:4:16"
scardtool --verbose send-apdu -r 0 -a GETUID  # APDU parse goster
```

### Mifare Classic
```bash
scardtool load-key    -r 0 -k FFFFFFFFFFFF
scardtool auth        -r 0 -b 4 -t A
scardtool read        -r 0 -p 4 -l 16
scardtool write       -r 0 -p 4 -d 00112233445566778899AABBCCDDEEFF
scardtool read-sector -r 0 -s 0 -k FFFFFFFFFFFF
scardtool read-trailer -r 0 -s 0 -k FFFFFFFFFFFF
scardtool make-access --table
scardtool write-trailer -r 0 -s 0 --key-a FFFFFFFFFFFF --key-b FFFFFFFFFFFF --access FF078069
```

### Sifreleme
```bash
# Algoritmay tespit et ve kaydet
scardtool detect-cipher -r 0 --save-session

# Sifreli yaz / oku
scardtool write -r 0 -p 4 -d "..." -E --cipher aes-ctr -P mypass
scardtool read  -r 0 -p 4 -l 16    -E --cipher aes-ctr -P mypass
```

### Katalog / Yardim
```bash
scardtool list-macros
scardtool list-macros --filter desfire
scardtool explain-macro GETUID
scardtool explain-sw 6982
scardtool explain-apdu "FF CA 00 00 00"
```

### Script Modu
```bash
scardtool -f myscript.scard
scardtool -e "list-readers; uid -r 0"
cat script.scard | scardtool
scardtool interactive
```

## MCP Entegrasyonu

`.mcp.json`:
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

### MCP Magazasi Yayimlama

scardtool local bir MCP server'dir (reader USB bagli olmali).
Anthropic'in mcp.so / MCP dizinine eklemek icin:

1. `npx @modelcontextprotocol/inspector scardtool --mcp` ile test edin
2. GitHub'a yukleyin
3. `smithery.ai` veya `mcp.so` gibi MCP dizin sitelerine kaydettirin
4. Ya da `claude_desktop_config.json` ile dogrudan dagitim:
   ```json
   { "mcpServers": { "scardtool": {
     "command": "npx", "args": ["-y", "@yourorg/scardtool-mcp"]
   }}}
   ```
   Bunun icin npm paketi olusturmaniz ve binary'yi paketlemeniz gerekir.

Ayrintilar icin: https://modelcontextprotocol.io/docs/distribution

## Desteklenen Kartlar

| Kart             | UID | ATS | Version | Auth | R/W | Sifreleme |
|------------------|-----|-----|---------|------|-----|-----------|
| Mifare Classic 1K | OK | OK | -       | OK   | OK  | aes-ctr   |
| Mifare Classic 4K | OK | OK | -       | OK   | OK  | aes-ctr   |
| Mifare Ultralight | OK | -  | OK      | -    | OK  | aes-ctr   |
| DESFire EV1/2/3  | OK | OK | OK      | -    | -   | aes-gcm   |

## Exit Codes

| Kod | Anlam |
|-----|-------|
| 0   | Basari |
| 1   | Genel hata |
| 2   | Gecersiz arguman |
| 3   | PC/SC hatasi |
| 4   | Kart iletisim hatasi |
