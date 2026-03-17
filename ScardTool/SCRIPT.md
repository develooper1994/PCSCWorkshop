# .scard Script Dili — Başvuru Kılavuzu

scardtool, `ScriptEngine` adlı yerleşik bir script yorumlayıcısı içerir.
Bash'e benzer, PC/SC otomasyon için tasarlanmış, basit ama eksiksiz bir dildir.

---

## İçindekiler

1. [Temel Kullanım](#1-temel-kullanım)
2. [Değişkenler](#2-değişkenler)
3. [Aritmetik İfadeler](#3-aritmetik-ifadeler)
4. [Bit İşlemleri](#4-bit-işlemleri)
5. [Boolean ve Koşul İfadeleri](#5-boolean-ve-koşul-ifadeleri)
6. [if / elif / else / fi](#6-if--elif--else--fi)
7. [while Döngüsü](#7-while-döngüsü)
8. [for Döngüsü](#8-for-döngüsü)
9. [Döngü Kontrolü: break / continue](#9-döngü-kontrolü-break--continue)
10. [echo](#10-echo)
11. [set Direktifleri](#11-set-direktifleri)
12. [Yorumlar ve Shebang](#12-yorumlar-ve-shebang)
13. [Çevre Değişkeni Entegrasyonu](#13-çevre-değişkeni-entegrasyonu)
14. [Hata Yönetimi](#14-hata-yönetimi)
15. [Güvenlik Sınırları](#15-güvenlik-sınırları)
16. [Gerçek Dünya Örnekleri](#16-gerçek-dünya-örnekleri)
17. [Tam Sözdizimi Özeti](#17-tam-sözdizimi-özeti)

---

## 1. Temel Kullanım

Script'ler `.scard` uzantılı düz metin dosyalarıdır. Her satır ya bir
scardtool komutu ya da bir script deyimidir.

```bash
# Dosyadan çalıştır
scardtool -f myscript.scard

# Satır içi script (noktalı virgülle)
scardtool -e "uid -r 0; card-info -r 0"

# Pipe ile stdin
echo "uid -r 0" | scardtool
cat script.scard | scardtool

# Dry-run ile test et (donanıma dokunmaz)
scardtool --dry-run -f myscript.scard
```

### Shebang

Script dosyasının ilk satırına shebang eklenebilir:

```bash
#!/usr/bin/env scardtool
uid -r 0
```

Shebang satırı her zaman yok sayılır.

---

## 2. Değişkenler

Değişken isimleri büyük/küçük harf duyarlıdır. `$` prefix'i ile tanımlanır
ya da kullanılır; her iki yazım da geçerlidir.

### Tanımlama

```scard
$READER = 0
$KEY    = FFFFFFFFFFFF
$PAGE   = 4
$LABEL  = "my-tag"
```

### Kullanım

`$` işareti ile değişken içeren her yerde genişletilir:

```scard
load-key -r $READER -k $KEY
read     -r $READER -p $PAGE
echo "Reader: $READER  Key: $KEY"
```

### Kural ve Sınırlar

| Kural | Açıklama |
|-------|----------|
| Büyük/küçük harf | `$reader` ≠ `$READER` |
| İsim karakterleri | Harf, rakam, `_` |
| Tanımsız değişken | Olduğu gibi bırakılır: `$UNDEF` → `$UNDEF` |
| Değer türleri | Her şey string; sayısal işlemlerde otomatik parse edilir |

---

## 3. Aritmetik İfadeler

Atama sırasında sağ taraf tek bir binary operatörden oluşabilir:

```
$VAR = OPERAND1 OP OPERAND2
```

**Boşluklar zorunludur**: `$N = $N+1` hatalıdır, `$N = $N + 1` doğrudur.

### Desteklenen Operatörler

| Operatör | Açıklama | Örnek |
|----------|----------|-------|
| `+` | Toplama | `$N = $N + 1` |
| `-` | Çıkarma | `$N = $N - 1` |
| `*` | Çarpma | `$N = $N * 2` |
| `/` | Tam bölme | `$N = $N / 4` |
| `%` | Modülo | `$N = 17 % 5` → 2 |

### Sayı Yazım Biçimleri

| Biçim | Taban | Örnek | Değer |
|-------|-------|-------|-------|
| Ondalık | 10 | `255` | 255 |
| Hexadecimal | 16 | `0xFF` | 255 |
| Binary | 2 | `0b11111111` | 255 |

```scard
$A = 255
$B = 0xFF
$C = 0b11111111
# $A, $B ve $C hepsi 255'e eşittir
```

### Taşma Koruması

Tüm aritmetik işlemler `int64_t` ile yapılır. Taşma, sıfıra bölme veya
geçersiz shift durumlarında **hesaplama yapılmaz**, `stderr`'e hata mesajı
yazılır ve değişken değişmez:

```scard
$MAX = 9223372036854775807    # INT64_MAX
$MAX = $MAX + 1               # Hata: integer overflow — $MAX değişmez

$X = 10
$X = $X / 0                  # Hata: division by zero — $X değişmez
$X = $X % 0                  # Hata: modulo by zero — $X değişmez
```

---

## 4. Bit İşlemleri

Bit operatörleri tamsayı değerler üzerinde çalışır.

### Operatörler

| Operatör | Açıklama | Örnek | Sonuç |
|----------|----------|-------|-------|
| `&` | Bitwise AND | `0xFF & 0x0F` | 15 |
| `\|` | Bitwise OR | `0xF0 \| 0x0F` | 255 |
| `^` | Bitwise XOR | `0xFF ^ 0x0F` | 240 |
| `~` | Bitwise NOT (unary) | `~0` | -1 |
| `<<` | Sol kaydırma | `1 << 4` | 16 |
| `>>` | Sağ kaydırma | `256 >> 3` | 32 |

```scard
# Bayrak testi
$FLAGS = 0b10110011
$MASK  = 0b00001111
$LOW   = $FLAGS & $MASK     # Alt 4 bit: 3
$HIGH  = $FLAGS >> 4        # Üst 4 bit: 11

# Bayrak set etme
$FLAGS = $FLAGS | 0b00000100   # bit 2'yi set et

# Bayrak temizleme
$FLAGS = $FLAGS & 0b11111011   # bit 2'yi temizle

# Bayrak toggle
$FLAGS = $FLAGS ^ 0b00000001   # bit 0'ı toggle et

# Byte seçimi
$KEY_BYTE0 = $KEY >> 0  & 0xFF
$KEY_BYTE1 = $KEY >> 8  & 0xFF
```

### Kaydırma Sınırları

Kaydırma miktarı 0..63 aralığında olmalıdır:

```scard
$X = 1 << 63    # Geçerli: INT64_MIN (-9223372036854775808)
$X = 1 << 64    # Hata: invalid shift amount
$X = 1 << -1    # Hata: invalid shift amount
```

---

## 5. Boolean ve Koşul İfadeleri

### Boolean Literaller

`true` ve `false` değişken değeri veya koşul olarak kullanılabilir:

```scard
$FLAG = true
$OK   = false

if $FLAG
    echo "flag is set"
fi
```

Sayısal eşdeğerleri: `true` = 1, `false` = 0.
Sıfır dışındaki her sayı koşulda `true` olarak değerlendirilir.

### Karşılaştırma Operatörleri

| Operatör | Açıklama |
|----------|----------|
| `==` | Eşit |
| `!=` | Eşit değil |
| `<` | Küçük |
| `>` | Büyük |
| `<=` | Küçük veya eşit |
| `>=` | Büyük veya eşit |

Sayısal değerlerde `int64_t` karşılaştırması yapılır. Sayısal olmayan
değerlerde string karşılaştırması (`==`, `!=`) yapılır.

### Mantıksal Operatörler

| Operatör | Açıklama | Örnek |
|----------|----------|-------|
| `&&` | Mantıksal AND | `$A > 0 && $B < 10` |
| `\|\|` | Mantıksal OR | `$A == 0 \|\| $B == 0` |
| `not` | Mantıksal NOT | `not $FLAG` |

**Önemli:** `&&` ve `||` boşlukla ayrılmalıdır: `$A>0&&$B<10` hatalı.

### Exit Code Koşulları

```scard
# Son çalıştırılan komutun başarı durumunu sorgula
connect -r 0
if ok         # son komut başarılıysa (exit code = 0)
    uid -r 0
fi

if fail       # son komut başarısızsa
    echo "bağlantı kurulamadı"
fi
```

`ok` / `success` ve `fail` / `error` eş anlamlıdır.

### Koşul Örnekleri

```scard
if $N == 5               # sayısal eşitlik
if $N != 0               # sıfır değil
if $N > 3 && $N < 10     # aralık testi
if $FLAGS & 0x01         # bit testi (sıfır olmayan = true)
if not $DONE             # olumsuzlama
if $TYPE == "desfire"    # string karşılaştırma
if ok && $SECTOR < 16    # exit code + değişken
```

---

## 6. if / elif / else / fi

```scard
if KOŞUL
    # then bloğu
elif KOŞUL
    # elif bloğu
else
    # else bloğu
fi
```

`elif` ve `else` isteğe bağlıdır. `elif` birden fazla kez kullanılabilir.

### Örnek

```scard
$SECTOR = 3

if $SECTOR == 0
    echo "sector 0: manufacturer"
elif $SECTOR < 16
    echo "sector $SECTOR: user data"
elif $SECTOR == 16
    echo "sector 16: trailer"
else
    echo "geçersiz sector"
fi
```

### if İçinde if (İç İçe)

```scard
connect -r 0
if ok
    uid -r 0
    if ok
        echo "UID alındı"
    else
        echo "UID alınamadı"
    fi
else
    echo "bağlantı hatası"
fi
```

---

## 7. while Döngüsü

```scard
while KOŞUL
    # gövde
done
```

Koşul her iterasyondan önce değerlendirilir. Başlangıçta yanlışsa gövde
hiç çalıştırılmaz.

### Örnek: Tüm Sektörleri Tara

```scard
$S = 0
while $S < 16
    read-sector --dry-run -r 0 -s $S -k FFFFFFFFFFFF
    if ok
        echo "sector $S: OK"
    else
        echo "sector $S: FAIL"
    fi
    $S = $S + 1
done
echo "tarama tamamlandı"
```

### Maksimum İterasyon

Sonsuz döngüleri önlemek için varsayılan iterasyon sınırı **10.000**'dir.
Aşıldığında `stop-on-error false` modunda uyarı verilir, aksi halde hata
çıkışı yapılır.

---

## 8. for Döngüsü

```scard
for $VAR in DEGER1 DEGER2 DEGER3
    # gövde
done
```

Liste değerleri boşlukla ayrılır. Değerler string ya da sayı olabilir.
Değerlerde `$DEĞIŞKEN` genişletmesi yapılır.

### Örnekler

```scard
# Sabit liste
for $BLOCK in 0 1 2
    read -r 0 -p $BLOCK
done

# Değişkenler listede
$B0 = 4
$B1 = 5
for $PAGE in $B0 $B1
    read --dry-run -r 0 -p $PAGE
done

# İç içe: her sektörün her bloğu
$SECTORS = 16
$S = 0
while $S < $SECTORS
    for $B in 0 1 2
        read -r 0 -p $B
    done
    $S = $S + 1
done
```

---

## 9. Döngü Kontrolü: break / continue

`break` — Döngüyü hemen bitirir.
`continue` — Kalan gövdeyi atlar, bir sonraki iterasyona geçer.

```scard
$I = 0
while $I < 100
    uid -r 0
    if fail
        echo "hata, duruyorum"
        break
    fi
    $I = $I + 1
done

# continue örneği
for $SECTOR in 0 1 2 3 4 5
    if $SECTOR == 3
        echo "sector 3 atlandı"
        continue
    fi
    read-sector -r 0 -s $SECTOR -k FFFFFFFFFFFF
done
```

---

## 10. echo

`echo` yerleşik bir komuttur; bir scardtool komutu değildir ve PC/SC'ye
APDU göndermez.

```scard
echo Merhaba
echo "Değer: $READER"
echo   # boş satır yazar
```

İsteğe bağlı tırnak işaretleri (`"..."`) temizlenir.
`$VAR` ifadeleri her zaman genişletilir.

---

## 11. set Direktifleri

Script davranışını kontrol eder:

```scard
set stop-on-error true    # herhangi bir hata scripti durdurur (varsayılan)
set stop-on-error false   # hatalar görmezden gelinir, script devam eder
```

### Kullanım Örneği

```scard
set stop-on-error false   # bağlantı hatalarına toleranslı tara
$S = 0
while $S < 16
    read-sector -r 0 -s $S -k FFFFFFFFFFFF
    $S = $S + 1
done
```

---

## 12. Yorumlar ve Shebang

`#` karakteri satır yorumunu başlatır. `#` öncesine boşluk olabilir.

```scard
#!/usr/bin/env scardtool
# Bu bir yorumdur

$READER = 0   # satır sonu yorumu
# $KEY = FFFFFFFFFFFF  ← bu satır devre dışı
```

---

## 13. Çevre Değişkeni Entegrasyonu

Script başlatılırken bazı `SCARDTOOL_*` çevre değişkenleri otomatik
olarak script değişkenlerine aktarılır:

| Çevre Değişkeni | Script Değişkeni |
|-----------------|------------------|
| `SCARDTOOL_READER` | `$READER` |
| `SCARDTOOL_KEY` | `$KEY` |
| `SCARDTOOL_CIPHER` | `$CIPHER` |

```bash
# Kabuğa bağımlı parametreler önceden ayarla:
export SCARDTOOL_READER=1
export SCARDTOOL_KEY=AABBCCDDEEFF
scardtool -f provison.scard
```

```scard
#!/usr/bin/env scardtool
# provision.scard — SCARDTOOL_READER ve SCARDTOOL_KEY otomatik yüklenir
echo "Reader: $READER"
echo "Key:    $KEY"
load-key -r $READER -k $KEY
auth     -r $READER -b 4 -t A
```

Komut satırı `--dry-run` ya da `-E` gibi bayraklar script içinde
doğrudan geçirilen komutlara etki eder; script değişkeni olarak
erişilemezler.

---

## 14. Hata Yönetimi

### Stop-on-error (Varsayılan: true)

Bir scardtool komutu sıfır olmayan çıkış kodu döndürdüğünde script
durur. Davranışı `set stop-on-error false` ile değiştirilebilir.

### exit code Koşulları

`if ok` / `if fail` doğrudan son komutun çıkış kodunu sorgular. Bu,
birden fazla adımın başarısını kontrol etmek için kullanışlıdır:

```scard
load-key -r 0 -k FFFFFFFFFFFF
if fail
    echo "Hata: varsayılan anahtar çalışmıyor"
    set stop-on-error false
    load-key -r 0 -k 000000000000
    if ok
        echo "Sıfır anahtarla devam"
    else
        echo "Hiçbir anahtar çalışmadı"
        break
    fi
fi
```

### Hata Mesajları

Aritmetik hatalar `stderr`'e yazılır; script akışını durdurmaz ancak
değişken değeri değişmez:

```
Script error: integer overflow in '9223372036854775807 + 1'
Script error: division by zero in '10 / 0'
Script error: invalid shift amount 64
```

---

## 15. Güvenlik Sınırları

| Sınır | Değer | Aşıldığında |
|-------|-------|-------------|
| Maksimum while iterasyonu | 10.000 | Uyarı + durma |
| Tamsayı aralığı | INT64_MIN..INT64_MAX | Hata mesajı, değişmez |
| Shift miktarı | 0..63 | Hata mesajı, değişmez |

---

## 16. Gerçek Dünya Örnekleri

### 16.1 — Tüm Mifare Classic Sektörlerini Dök

```scard
#!/usr/bin/env scardtool
# dump_classic.scard — 16 sektörü sırası ile oku

$READER  = 0
$KEY     = FFFFFFFFFFFF
$SECTORS = 16
$S       = 0

echo "=== Mifare Classic 1K dump ==="

while $S < $SECTORS
    read-sector -r $READER -s $S -k $KEY
    if ok
        echo "--- sector $S ---"
    else
        echo "sector $S: okuma hatası (anahtar hatalı olabilir)"
    fi
    $S = $S + 1
done

echo "=== dump tamamlandı ==="
```

### 16.2 — Toplu Kart Provisioning

```scard
#!/usr/bin/env scardtool
# provision.scard — kart hazırlama scripti
# Kullanım: SCARDTOOL_READER=0 scardtool -f provision.scard

$SECTOR = 1
$PAGE   = 4
$NEWKEY = A0A1A2A3A4A5

# Bağlantı ve UID
connect -r $READER
if fail
    echo "Hata: reader $READER'a bağlanılamadı"
    break
fi

uid -r $READER
echo "Kart hazırlanıyor..."

# Varsayılan anahtarla auth
load-key -r $READER -k FFFFFFFFFFFF
auth     -r $READER -b 4 -t A
if fail
    echo "Hata: auth başarısız (kart zaten hazırlanmış olabilir)"
    break
fi

# Veri yaz
write -r $READER -p $PAGE -d 01020304050607080910111213141516
if ok
    echo "Veri yazıldı: page $PAGE"
fi

# Yeni anahtar ile trailer güncelle
write-trailer -r $READER -s $SECTOR \
    --key-a $NEWKEY \
    --key-b FFFFFFFFFFFFFF \
    --access 7F0F08
if ok
    echo "Trailer güncellendi: sector $SECTOR"
fi

disconnect
echo "Provisioning tamamlandı."
```

### 16.3 — Bit Maskesi ile Bayrak Okuma

```scard
#!/usr/bin/env scardtool
# flags.scard — access bits analizi

$READER  = 0
$SECTOR  = 0
$TRAILER = 0xFF078069   # örnek access condition

# Alt nibble: block 0 erişim koşulları
$BLK0_COND = $TRAILER & 0x0000000F
echo "Block 0 access: $BLK0_COND"

if $BLK0_COND == 0
    echo "Block 0: transport configuration"
elif $BLK0_COND == 1
    echo "Block 0: read/write (A or B)"
elif $BLK0_COND == 6
    echo "Block 0: read-only"
else
    echo "Block 0: custom access ($BLK0_COND)"
fi

# Key A okunabilir mi? (bit 3)
$KEY_A_READABLE = $TRAILER >> 3 & 1
if $KEY_A_READABLE == 1
    echo "Key A okunabilir"
else
    echo "Key A korumalı (normal)"
fi
```

### 16.4 — Çoklu Reader ile Paralel Test

```scard
#!/usr/bin/env scardtool
# multi_reader.scard — reader'ları sırayla test et

set stop-on-error false

for $R in 0 1 2 3
    echo "--- Reader $R ---"
    connect -r $R
    if ok
        uid -r $R
        echo "Reader $R: kart mevcut"
        disconnect
    else
        echo "Reader $R: kart yok veya erişilemiyor"
    fi
done

echo "Tüm reader'lar tarandı"
```

### 16.5 — AES-CTR Şifreli Yedekleme

```scard
#!/usr/bin/env scardtool
# encrypted_backup.scard
# Kullanım: scardtool -W -f encrypted_backup.scard
# (-W interaktif parola girişi için)

$READER = 0
$KEY    = FFFFFFFFFFFF
$S      = 0

while $S < 16
    read-sector -r $READER -s $S -k $KEY -E --cipher aes-ctr
    if ok
        echo "sector $S: şifreli yedek alındı"
    else
        echo "sector $S: atlandı"
    fi
    $S = $S + 1
done
```

---

## 17. Tam Sözdizimi Özeti

```
SCRIPT     := (STMT '\n')*

STMT       := COMMENT
            | SHEBANG           # sadece ilk satır
            | ASSIGNMENT
            | ECHO_STMT
            | SET_STMT
            | IF_STMT
            | WHILE_STMT
            | FOR_STMT
            | BREAK
            | CONTINUE
            | COMMAND

COMMENT    := '#' .*

SHEBANG    := '#!' .*

ASSIGNMENT := ('$')? NAME '=' EXPR
            | ('$')? NAME '=' VALUE

EXPR       := VALUE OP VALUE
            | '~' VALUE          # unary NOT

VALUE      := LITERAL | VARIABLE | HEX | BINARY

LITERAL    := [0-9]+             # ondalık
HEX        := '0x' [0-9A-Fa-f]+
BINARY     := '0b' [01]+
VARIABLE   := '$' NAME

OP         := '+' | '-' | '*' | '/' | '%'
            | '&' | '|' | '^' | '<<' | '>>'

ECHO_STMT  := 'echo' (.*)

SET_STMT   := 'set' 'stop-on-error' ('true'|'false')

IF_STMT    := 'if' COND '\n' (STMT '\n')* (ELIF_BLOCK)* (ELSE_BLOCK)? 'fi'

ELIF_BLOCK := 'elif' COND '\n' (STMT '\n')*

ELSE_BLOCK := 'else' '\n' (STMT '\n')*

WHILE_STMT := 'while' COND '\n' (STMT '\n')* 'done'

FOR_STMT   := 'for' '$'NAME 'in' VALUE+ '\n' (STMT '\n')* 'done'

BREAK      := 'break'

CONTINUE   := 'continue'

COND       := SIMPLE_COND (('&&' | '||') SIMPLE_COND)*

SIMPLE_COND := 'ok' | 'success' | 'fail' | 'error'
             | 'true' | 'false'
             | 'not' SIMPLE_COND
             | VALUE CMP VALUE
             | VALUE              # sıfır olmayan = true

CMP        := '==' | '!=' | '<' | '>' | '<=' | '>='

NAME       := [A-Za-z_][A-Za-z0-9_]*

COMMAND    := <geçerli bir scardtool komutu ve argümanları>
```

### Hızlı Başvuru Kartı

| Yapı | Sözdizimi |
|------|-----------|
| Değişken tanımla | `$N = 42` |
| Değişken kullan | `cmd -r $N` |
| Aritmetik | `$N = $N + 1` |
| Bit AND | `$R = $A & $B` |
| Bit OR | `$R = $A \| $B` |
| Bit XOR | `$R = $A ^ $B` |
| Bit NOT | `$R = ~$A` |
| Sol kaydırma | `$R = $A << 4` |
| Sağ kaydırma | `$R = $A >> 4` |
| Hex literal | `$R = 0xFF` |
| Bin literal | `$R = 0b1010` |
| if | `if COND ... fi` |
| elif | `elif COND` |
| else | `else` |
| while | `while COND ... done` |
| for | `for $V in a b c ... done` |
| break | `break` |
| continue | `continue` |
| echo | `echo "msg $VAR"` |
| yorum | `# bu bir yorumdur` |
| stop-on-error | `set stop-on-error false` |
| exit code test | `if ok / if fail` |
| boolean AND | `if $A > 0 && $B < 10` |
| boolean OR | `if $A == 0 \|\| $B == 0` |
| boolean NOT | `if not $FLAG` |
