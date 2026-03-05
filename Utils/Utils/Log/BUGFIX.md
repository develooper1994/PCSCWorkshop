# ?? HATA FÝKSÝ: std::system_error (Deadlock)

## ?? SORUN

Runtime'da þu hata alýnýyordu:
```
0x00007FFC2045064C noktasýnda, Workshop1.exe üzerinde iþlenmeyen özel durum:
Microsoft C++ özel durumu: std::system_error.
Bellek konumu: 0x000000EC9E0FEE98.
```

## ?? ROOT CAUSE

`Log` sýnýfýnda **deadlock** problemi vardý:

1. `Constructor` çaðrý sýrasýnda `resetToDefaults()` ? **LOCK ALIR**
2. `resetToDefaults()` içinde `enableAllLogTypes()` ? **YENÝDEN LOCK ALMAYA ÇALIÞIR** ?
3. Ayný thread ayný mutex'i iki kez kilitlemek istedi
4. `std::mutex` **recursive olmadýðýndan** ? **DEADLOCK**

```cpp
// YANLIS KOD
Log::Log() {
    resetToDefaults();  // Lock-1
}

void Log::resetToDefaults() {
    std::lock_guard<std::mutex> lock(m_mutex);  // Lock-1 (zaten var!)
    enableAllLogTypes();  // Bu da lock istedi!
}

void Log::enableAllLogTypes() {
    std::lock_guard<std::mutex> lock(m_mutex);  // Lock-2 (çifte kilit!) ?
    for (int i = 0; i < 4; ++i) {
        m_enabledLogTypes[i] = true;
    }
}
```

## ? ÇÖZÜM

**Internal unlocked helper fonksiyonlar** oluþturduk:

```cpp
// DOÐRU KOD
void Log::enableAllLogTypesUnlocked() {
    // LOCK ALMAZ - zaten kilitlenmiþ baðlamda kullanýlýr
    for (int i = 0; i < 4; ++i) {
        m_enabledLogTypes[i] = true;
    }
}

void Log::resetToDefaults() {
    std::lock_guard<std::mutex> lock(m_mutex);  // Bir kez lock al
    m_logLevel = LogLevel::Debug;
    enableAllLogTypesUnlocked();      // Lock yok - güvenli!
    enableAllCategoriesUnlocked();    // Lock yok - güvenli!
}
```

## ?? YAPILAN DEÐÝÞÝKLÝKLER

### Log.h'de Private Bölüme Eklenenler:
```cpp
// Internal unlocked helpers
void enableAllLogTypesUnlocked();
void disableAllLogTypesUnlocked();
void enableAllCategoriesUnlocked();
void disableAllCategoriesUnlocked();
```

### Log.cpp'de Eklenenler:
```cpp
void Log::enableAllLogTypesUnlocked() {
    for (int i = 0; i < 4; ++i) {
        m_enabledLogTypes[i] = true;
    }
}

void Log::disableAllLogTypesUnlocked() {
    for (int i = 0; i < 4; ++i) {
        m_enabledLogTypes[i] = false;
    }
}

void Log::enableAllCategoriesUnlocked() {
    for (int i = 0; i < 6; ++i) {
        m_enabledCategories[i] = true;
    }
}

void Log::disableAllCategoriesUnlocked() {
    for (int i = 0; i < 6; ++i) {
        m_enabledCategories[i] = false;
    }
}
```

### Log.cpp'de Güncellenenler:
`resetToDefaults()` fonksiyonu:
```cpp
void Log::resetToDefaults() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_logLevel = LogLevel::Debug;
    enableAllLogTypesUnlocked();      // ? Unlocked version
    enableAllCategoriesUnlocked();    // ? Unlocked version
}
```

## ?? DESIGN PATTERN

Ýki seviye fonksiyon:

| Fonksiyon | Lock | Kullaným |
|-----------|------|----------|
| `enableAllLogTypes()` | **Var** ? | Public - dýþarýdan çaðrýlýr |
| `enableAllLogTypesUnlocked()` | **Yok** ? | Private - zaten kilitlenmiþ baðlamda |

**Kural:**
- Public fonksiyonlar: **LOCK ALIR** (dýþarýdan çaðrýldýðý için güvenli olmalý)
- Private Unlocked: **LOCK ALMAZ** (iç kullaným, çýktacý zaten kilitledi)

## ?? SONUÇ

? std::system_error hatasý çözüldü
? Deadlock problemi ortadan kaldýrýldý
? Thread-safety korundu
? Proje baþarýyla derlenmiþtir

---

**NOT:** Bu tür sorunlarý önlemek için her zaman:
1. Lock'larý minimize et
2. Lock altýndayken baþka lock almaya çalýþma
3. Unlocked helper fonksiyonlar kullan
4. `std::recursive_mutex` yerine iç helper'lar daha temiz
