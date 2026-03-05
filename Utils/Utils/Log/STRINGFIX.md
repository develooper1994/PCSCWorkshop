# ?? HATA FÝKSÝ: String Iterator Container Mismatch

## ?? SORUN

Runtime'da bu hata alýnýyordu:
```
_STL_VERIFY(_First._Getcont() == _Last._Getcont(), 
    "string iterators in range are from different containers");
```

## ?? ROOT CAUSE: Temporary Object Iterator'larý

Orijinal kod:
```cpp
std::wostringstream woss;
woss << L"Connected to: " << m_readerName;
LOG_CONN_INFO(std::string(woss.str().begin(), woss.str().end()));
//                         ^^^^^^^^^^^         ^^^^^^^^^^^
//                         1. Temporary         2. FARKLI Temporary!
```

**Problem:**
- `woss.str()` her çaðrýda **yeni** temporary `std::wstring` döndürüyor
- Ýlk `woss.str().begin()` ? 1. temporary'den iterator
- Ýkinci `woss.str().end()` ? 2. temporary'den iterator
- **Farklý container'lardan** gelen iterator'lar ? STL error ?

## ? ÇÖZÜM

Temporary'yi bir kez al, sonra iterator'lar oluþtur:

```cpp
// ? DOÐRU KOD
std::wstring wideReaderName = m_readerName;
std::string narrowReaderName(wideReaderName.begin(), wideReaderName.end());
LOG_CONN_INFO("Connected to: " + narrowReaderName);
```

## ?? KARÞILAÞTIRMA

### ? YANLIS
```cpp
LOG_CONN_INFO(std::string(woss.str().begin(), woss.str().end()));
// Farklý temporary'lerden iterator'lar
```

### ? DOÐRU
```cpp
std::wstring temp = woss.str();  // Bir kez al
std::string result(temp.begin(), temp.end());  // Ayný container
LOG_CONN_INFO("Connected to: " + narrowReaderName);
```

## ?? GENEL KURAL

**Iterator konstruktörleri kullanan zaman:**
```cpp
// ? Yanlýþ: Temporary'yi birden fazla kez çaðýrma
std::string s(someFunc().begin(), someFunc().end());

// ? Doðru: Bir kez al, sonra iþle
auto temp = someFunc();
std::string s(temp.begin(), temp.end());
```

## ?? YAPILAN DEÐÝÞÝKLÝK

**Dosya:** `Utils/Utils/CardConnection.cpp`
**Fonksiyon:** `CardConnection::connect()`

**Önce:**
```cpp
std::wostringstream woss;
woss << L"Connected to: " << m_readerName;
LOG_CONN_INFO(std::string(woss.str().begin(), woss.str().end()));
```

**Sonra:**
```cpp
std::wstring wideReaderName = m_readerName;
std::string narrowReaderName(wideReaderName.begin(), wideReaderName.end());
LOG_CONN_INFO("Connected to: " + narrowReaderName);
```

## ?? SONUÇ

? Iterator container mismatch hatasý çözüldü
? Code daha temiz ve okunabilir
? Proje baþarýyla derlenmiþtir

---

## ?? BONUS: Alternatif Çözümler

### Seçenek 1: codecvt kullan (C++11+)
```cpp
#include <codecvt>
std::wstring_convert<std::codecvt_utf8<wchar_t>> converter;
std::string narrowReaderName = converter.to_bytes(m_readerName);
```

### Seçenek 2: Basit loop
```cpp
std::string narrowReaderName;
for (wchar_t c : m_readerName) {
    narrowReaderName += static_cast<char>(c);
}
```

### Seçenek 3: Tercih edilen (kullanýlan)
```cpp
std::string narrowReaderName(m_readerName.begin(), m_readerName.end());
```
