#ifndef PCSC_WORKSHOP_LOG_H
#define PCSC_WORKSHOP_LOG_H

#include <iostream>
#include <sstream>
#include <string>
#include <memory>
#include <mutex>
#include <unordered_map>

namespace pcsc {

// Log Seviyeleri
enum class LogLevel {
	Off = 0,
	Error = 1,
	Warning = 2,
	Info = 3,
	Debug = 4
};

// Log Kategorileri
enum class LogCategory {
	General = 0,
	PCSC = 1,
	Reader = 2,
	Cipher = 3,
	Connection = 4,
	Card = 5
};

// Log Türleri (birbirinden baðýmsýz açýlýp kapatýlabilir)
enum class LogType {
	Error = 0,
	Warning = 1,
	Info = 2,
	Debug = 3
};

class Log {
public:
	// Singleton instance'ýna eriþim
	static Log& getInstance();

	// ============================================================
	// Global Log Seviyesi Ayarlarý
	// ============================================================

	// Genel log seviyesini ayarla (Off, Error, Warning, Info, Debug)
	void setLogLevel(LogLevel level);

	// Mevcut log seviyesini getir
	LogLevel getLogLevel() const;

	// ============================================================
	// Baðýmsýz Log Türü Kontrolleri
	// ============================================================

	// Belirli bir log türünü aç/kapat
	void enableLogType(LogType type, bool enable = true);
	void disableLogType(LogType type);
	void toggleLogType(LogType type);

	// Belirli bir log türünün açýk olup olmadýðýný kontrol et
	bool isLogTypeEnabled(LogType type) const;

	// Tüm log türlerini aç
	void enableAllLogTypes();

	// Tüm log türlerini kapat
	void disableAllLogTypes();

	// ============================================================
	// Kategori Bazýnda Kontrol
	// ============================================================

	// Belirli bir kategoriye ait logu aç/kapat
	void enableCategory(LogCategory category, bool enable = true);
	void disableCategory(LogCategory category);

	// Belirli bir kategoriye ait logu kontrol et
	bool isCategoryEnabled(LogCategory category) const;

	// Tüm kategorileri aç
	void enableAllCategories();

	// Tüm kategorileri kapat
	void disableAllCategories();

	// ============================================================
	// Hazýr Kontrol Fonksiyonlarý
	// ============================================================

	bool isDebugEnabled() const;
	bool isInfoEnabled() const;
	bool isWarningEnabled() const;
	bool isErrorEnabled() const;

	bool isDebugEnabledForCategory(LogCategory category) const;
	bool isInfoEnabledForCategory(LogCategory category) const;
	bool isWarningEnabledForCategory(LogCategory category) const;
	bool isErrorEnabledForCategory(LogCategory category) const;

	// ============================================================
	// Log Fonksiyonlarý
	// ============================================================

	void debug(const std::string& message, LogCategory category = LogCategory::General);
	void info(const std::string& message, LogCategory category = LogCategory::General);
	void warning(const std::string& message, LogCategory category = LogCategory::General);
	void error(const std::string& message, LogCategory category = LogCategory::General);

	// ============================================================
	// Konfigürasyon
	// ============================================================

	// Varsayýlan ayarlarý yükle
	void resetToDefaults();

	// Tüm ayarlarý konsola yazdýr
	void printSettings() const;

private:
	Log();
	~Log() = default;

	// Copy ve move iþlemleri engelle
	Log(const Log&) = delete;
	Log& operator=(const Log&) = delete;
	Log(Log&&) = delete;

	// Helper fonksiyonlar
	std::string getCategoryName(LogCategory category) const;
	std::string getLogTypeName(LogType type) const;
	std::string getLogLevelName(LogLevel level) const;
	bool shouldLog(LogType type, LogCategory category) const;

	// Internal unlocked helpers - deadlock'u önlemek için
	// UYARI: Bu fonksiyonlar zaten kilitlenmiþ baðlamda çaðrýlmalýdýr
	void enableAllLogTypesUnlocked();
	void disableAllLogTypesUnlocked();
	void enableAllCategoriesUnlocked();
	void disableAllCategoriesUnlocked();

	// Member variables
	LogLevel m_logLevel;
	std::unordered_map<int, bool> m_enabledLogTypes;      // LogType -> enabled
	std::unordered_map<int, bool> m_enabledCategories;    // LogCategory -> enabled

	mutable std::mutex m_mutex;
};

} // namespace pcsc

// ============================================================
// Helper Makrolar
// ============================================================

// Temel makrolar
#define LOG_DEBUG(msg) LOG_DEBUG_CAT(msg, pcsc::LogCategory::General)
#define LOG_INFO(msg) LOG_INFO_CAT(msg, pcsc::LogCategory::General)
#define LOG_WARNING(msg) LOG_WARNING_CAT(msg, pcsc::LogCategory::General)
#define LOG_ERROR(msg) LOG_ERROR_CAT(msg, pcsc::LogCategory::General)

// Kategori ile birlikte makrolar
#define LOG_DEBUG_CAT(msg, cat) do { \
	if (pcsc::Log::getInstance().isDebugEnabledForCategory(cat)) { \
		pcsc::Log::getInstance().debug(msg, cat); \
	} \
} while(false)

#define LOG_INFO_CAT(msg, cat) do { \
	if (pcsc::Log::getInstance().isInfoEnabledForCategory(cat)) { \
		pcsc::Log::getInstance().info(msg, cat); \
	} \
} while(false)

#define LOG_WARNING_CAT(msg, cat) do { \
	if (pcsc::Log::getInstance().isWarningEnabledForCategory(cat)) { \
		pcsc::Log::getInstance().warning(msg, cat); \
	} \
} while(false)

#define LOG_ERROR_CAT(msg, cat) do { \
	if (pcsc::Log::getInstance().isErrorEnabledForCategory(cat)) { \
		pcsc::Log::getInstance().error(msg, cat); \
	} \
} while(false)

// Kýsa isim makrolarý
#define LOG_PCSC_DEBUG(msg) LOG_DEBUG_CAT(msg, pcsc::LogCategory::PCSC)
#define LOG_PCSC_INFO(msg) LOG_INFO_CAT(msg, pcsc::LogCategory::PCSC)
#define LOG_PCSC_WARNING(msg) LOG_WARNING_CAT(msg, pcsc::LogCategory::PCSC)
#define LOG_PCSC_ERROR(msg) LOG_ERROR_CAT(msg, pcsc::LogCategory::PCSC)

#define LOG_READER_DEBUG(msg) LOG_DEBUG_CAT(msg, pcsc::LogCategory::Reader)
#define LOG_READER_INFO(msg) LOG_INFO_CAT(msg, pcsc::LogCategory::Reader)
#define LOG_READER_WARNING(msg) LOG_WARNING_CAT(msg, pcsc::LogCategory::Reader)
#define LOG_READER_ERROR(msg) LOG_ERROR_CAT(msg, pcsc::LogCategory::Reader)

#define LOG_CIPHER_DEBUG(msg) LOG_DEBUG_CAT(msg, pcsc::LogCategory::Cipher)
#define LOG_CIPHER_INFO(msg) LOG_INFO_CAT(msg, pcsc::LogCategory::Cipher)
#define LOG_CIPHER_WARNING(msg) LOG_WARNING_CAT(msg, pcsc::LogCategory::Cipher)
#define LOG_CIPHER_ERROR(msg) LOG_ERROR_CAT(msg, pcsc::LogCategory::Cipher)

#define LOG_CONN_DEBUG(msg) LOG_DEBUG_CAT(msg, pcsc::LogCategory::Connection)
#define LOG_CONN_INFO(msg) LOG_INFO_CAT(msg, pcsc::LogCategory::Connection)
#define LOG_CONN_WARNING(msg) LOG_WARNING_CAT(msg, pcsc::LogCategory::Connection)
#define LOG_CONN_ERROR(msg) LOG_ERROR_CAT(msg, pcsc::LogCategory::Connection)

#define LOG_CARD_DEBUG(msg) LOG_DEBUG_CAT(msg, pcsc::LogCategory::Card)
#define LOG_CARD_INFO(msg) LOG_INFO_CAT(msg, pcsc::LogCategory::Card)
#define LOG_CARD_WARNING(msg) LOG_WARNING_CAT(msg, pcsc::LogCategory::Card)
#define LOG_CARD_ERROR(msg) LOG_ERROR_CAT(msg, pcsc::LogCategory::Card)

#endif // PCSC_WORKSHOP_LOG_H
