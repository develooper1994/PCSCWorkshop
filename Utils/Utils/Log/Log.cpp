#include "Log.h"
#include <iostream>
#include <iomanip>
#include <chrono>
#include <ctime>
#include <sstream>

namespace pcsc {

Log& Log::getInstance() {
	static Log instance(true);
	return instance;
}

Log::Log() : m_logLevel(LogLevel::Debug) {
	// Varsayılan olarak tüm log türleri ve kategoriler açık
	resetToDefaults();
}

// ============================================================
// Global Log Seviyesi Ayarları
// ============================================================

void Log::setLogLevel(LogLevel level) {
	std::lock_guard<std::mutex> lock(m_mutex);
	m_logLevel = level;
}

LogLevel Log::getLogLevel() const {
	std::lock_guard<std::mutex> lock(m_mutex);
	return m_logLevel;
}

// ============================================================
// Bağımsız Log Türü Kontrolleri
// ============================================================

void Log::enableLogType(LogType type, bool enable) {
	std::lock_guard<std::mutex> lock(m_mutex);
	m_enabledLogTypes[static_cast<int>(type)] = enable;
}

void Log::disableLogType(LogType type) {
	enableLogType(type, false);
}

void Log::toggleLogType(LogType type) {
	std::lock_guard<std::mutex> lock(m_mutex);
	auto it = m_enabledLogTypes.find(static_cast<int>(type));
	if (it != m_enabledLogTypes.end()) {
		it->second = !it->second;
	}
}

bool Log::isLogTypeEnabled(LogType type) const {
	std::lock_guard<std::mutex> lock(m_mutex);
	auto it = m_enabledLogTypes.find(static_cast<int>(type));
	return it != m_enabledLogTypes.end() ? it->second : false;
}

void Log::enableAllLogTypes() {
	std::lock_guard<std::mutex> lock(m_mutex);
	for (int i = 0; i < 4; ++i) {
		m_enabledLogTypes[i] = true;
	}
}

void Log::disableAllLogTypes() {
	std::lock_guard<std::mutex> lock(m_mutex);
	for (int i = 0; i < 4; ++i) {
		m_enabledLogTypes[i] = false;
	}
}

// Internal helper - lock olmaksızın tüm türleri etkinleştir
// UYARI: Sadece zaten kilitlenmiş bağlamda kullan
void Log::enableAllLogTypesUnlocked() {
	for (int i = 0; i < 4; ++i) {
		m_enabledLogTypes[i] = true;
	}
}

// Internal helper - lock olmaksızın tüm türleri devre dışı bırak
// UYARI: Sadece zaten kilitlenmiş bağlamda kullan
void Log::disableAllLogTypesUnlocked() {
	for (int i = 0; i < 4; ++i) {
		m_enabledLogTypes[i] = false;
	}
}

// ============================================================
// Kategori Bazında Kontrol
// ============================================================

void Log::enableCategory(LogCategory category, bool enable) {
	std::lock_guard<std::mutex> lock(m_mutex);
	m_enabledCategories[static_cast<int>(category)] = enable;
}

void Log::disableCategory(LogCategory category) {
	enableCategory(category, false);
}

bool Log::isCategoryEnabled(LogCategory category) const {
	std::lock_guard<std::mutex> lock(m_mutex);
	auto it = m_enabledCategories.find(static_cast<int>(category));
	return it != m_enabledCategories.end() ? it->second : false;
}

void Log::enableAllCategories() {
	std::lock_guard<std::mutex> lock(m_mutex);
	for (int i = 0; i < 6; ++i) {
		m_enabledCategories[i] = true;
	}
}

void Log::disableAllCategories() {
	std::lock_guard<std::mutex> lock(m_mutex);
	for (int i = 0; i < 6; ++i) {
		m_enabledCategories[i] = false;
	}
}

// Internal helper - lock olmaksızın tüm kategorileri etkinleştir
// UYARI: Sadece zaten kilitlenmiş bağlamda kullan
void Log::enableAllCategoriesUnlocked() {
	for (int i = 0; i < 6; ++i) {
		m_enabledCategories[i] = true;
	}
}

// Internal helper - lock olmaksızın tüm kategorileri devre dışı bırak
// UYARI: Sadece zaten kilitlenmiş bağlamda kullan
void Log::disableAllCategoriesUnlocked() {
	for (int i = 0; i < 6; ++i) {
		m_enabledCategories[i] = false;
	}
}

// ============================================================
// Hazır Kontrol Fonksiyonları
// ============================================================

bool Log::isDebugEnabled() const {
	std::lock_guard<std::mutex> lock(m_mutex);
	return m_logLevel >= LogLevel::Debug && m_enabledLogTypes.at(static_cast<int>(LogType::Debug));
}

bool Log::isInfoEnabled() const {
	std::lock_guard<std::mutex> lock(m_mutex);
	return m_logLevel >= LogLevel::Info && m_enabledLogTypes.at(static_cast<int>(LogType::Info));
}

bool Log::isWarningEnabled() const {
	std::lock_guard<std::mutex> lock(m_mutex);
	return m_logLevel >= LogLevel::Warning && m_enabledLogTypes.at(static_cast<int>(LogType::Warning));
}

bool Log::isErrorEnabled() const {
	std::lock_guard<std::mutex> lock(m_mutex);
	return m_logLevel >= LogLevel::Error && m_enabledLogTypes.at(static_cast<int>(LogType::Error));
}

bool Log::isDebugEnabledForCategory(LogCategory category) const {
	return isDebugEnabled() && isCategoryEnabled(category);
}

bool Log::isInfoEnabledForCategory(LogCategory category) const {
	return isInfoEnabled() && isCategoryEnabled(category);
}

bool Log::isWarningEnabledForCategory(LogCategory category) const {
	return isWarningEnabled() && isCategoryEnabled(category);
}

bool Log::isErrorEnabledForCategory(LogCategory category) const {
	return isErrorEnabled() && isCategoryEnabled(category);
}

// ============================================================
// Log Fonksiyonları
// ============================================================

void Log::debug(const std::string& message, LogCategory category) { log(LogType::Debug, message, category, std::cout); }

void Log::info(const std::string& message, LogCategory category) { log(LogType::Info, message, category, std::cout); }

void Log::warning(const std::string& message, LogCategory category) { log(LogType::Warning, message, category, std::cout); }

void Log::error(const std::string& message, LogCategory category) { log(LogType::Error, message, category, std::cerr); }

void Log::log(LogType type, const std::string& message, LogCategory category, std::ostream& out) {
	if (!shouldLog(type, category)) return;
	std::lock_guard<std::mutex> lock(m_mutex);
	out << getColor(type)
	    << "[" << getTimestamp() << "]"
	    << "[TID:" << getThreadId() << "]"
	    << "[" << getCategoryName(category) << "]"
	    << "[";

	switch (type) {
		case LogType::Debug:
			out << "DEBUG";
			break;
		case LogType::Info:
			out << "INFO";
			break;
		case LogType::Warning:
			out << "WARNING";
			break;
		case LogType::Error:
			out << "ERROR";
			break;
	}

	out << "] " << message << resetColor() << std::endl;
}

// ============================================================
// Konfigürasyon
// ============================================================

void Log::resetToDefaults() {
	std::lock_guard<std::mutex> lock(m_mutex);
	m_logLevel = LogLevel::Debug;
	m_useColors = false;
	enableAllLogTypesUnlocked();
	enableAllCategoriesUnlocked();
}

void Log::printSettings() const {
	std::lock_guard<std::mutex> lock(m_mutex);
	std::cout << "\n========== LOG SETTINGS ==========\n";
	std::cout << "Global Log Level: " << getLogLevelName(m_logLevel) << "\n\n";
	std::cout << "Use Colors: " << (m_useColors ? "Yes" : "No") << "\n\n";
	
	std::cout << "Log Types:\n";
	for (int i = 0; i < 4; ++i) {
		auto it = m_enabledLogTypes.find(i);
		std::string typeName = getLogTypeName(static_cast<LogType>(i));
		bool enabled = (it != m_enabledLogTypes.end()) ? it->second : false;
		std::cout << "  [" << (enabled ? "?" : "?") << "] " << typeName << "\n";
	}
	
	std::cout << "\nLog Categories:\n";
	for (int i = 0; i < 6; ++i) {
		auto it = m_enabledCategories.find(i);
		std::string categoryName = getCategoryName(static_cast<LogCategory>(i));
		bool enabled = (it != m_enabledCategories.end()) ? it->second : false;
		std::cout << "  [" << (enabled ? "?" : "?") << "] " << categoryName << "\n";
	}
	std::cout << "==================================\n\n";
}

// ============================================================
// Helper Fonksiyonlar
// ============================================================

std::string Log::getCategoryName(LogCategory category) const {
	switch (category) {
		case LogCategory::General: return "General";
		case LogCategory::PCSC: return "PCSC";
		case LogCategory::Reader: return "Reader";
		case LogCategory::Cipher: return "Cipher";
		case LogCategory::Connection: return "Connection";
		case LogCategory::Card: return "Card";
		default: return "Unknown";
	}
}

const char* Log::getColor(LogType type) const {
	switch (type) {
		case LogType::Debug: return "\033[36m"; // Cyan
		case LogType::Info: return "\033[32m"; // Green
		case LogType::Warning: return "\033[33m"; // Yellow
		case LogType::Error: return "\033[31m"; // Red
		default: return "\033[0m"; // Reset
	}
}

const char* Log::resetColor() { return m_useColors ? "\033[0m" : ""; }

std::string Log::getTimestamp() {
	auto now = std::chrono::system_clock::now();
	auto in_time_t = std::chrono::system_clock::to_time_t(now);
	auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
	              now.time_since_epoch()) %
	          1000;

	std::tm tm_snapshot;

#if defined(_WIN32) || defined(_WIN64)
	localtime_s(&tm_snapshot, &in_time_t); // Windows thread-safe
#else
	localtime_r(&in_time_t, &tm_snapshot); // Linux/Unix thread-safe
#endif

	std::ostringstream ss;
	ss << std::put_time(&tm_snapshot, "%Y-%m-%d %H:%M:%S")
	   << '.' << std::setw(3) << std::setfill('0') << ms.count();
	return ss.str();
}

std::string Log::getThreadId() {
	std::ostringstream ss;
	ss << std::this_thread::get_id();
	return ss.str();
}

std::string Log::getLogTypeName(LogType type) const {
	switch (type) {
		case LogType::Error: return "Error";
		case LogType::Warning: return "Warning";
		case LogType::Info: return "Info";
		case LogType::Debug: return "Debug";
		default: return "Unknown";
	}
}

std::string Log::getLogLevelName(LogLevel level) const {
	switch (level) {
		case LogLevel::Off: return "Off";
		case LogLevel::Error: return "Error";
		case LogLevel::Warning: return "Warning";
		case LogLevel::Info: return "Info";
		case LogLevel::Debug: return "Debug";
		default: return "Unknown";
	}
}

bool Log::shouldLog(LogType type, LogCategory category) const {
	std::lock_guard<std::mutex> lock(m_mutex);
	
	// Log level kontrolü
	int typeLevel = static_cast<int>(type);
	if (m_logLevel < static_cast<LogLevel>(typeLevel)) {
		return false;
	}
	
	// Log type kontrolü
	auto typeIt = m_enabledLogTypes.find(typeLevel);
	if (typeIt != m_enabledLogTypes.end() && !typeIt->second) {
		return false;
	}
	
	// Kategori kontrolü
	auto catIt = m_enabledCategories.find(static_cast<int>(category));
	if (catIt != m_enabledCategories.end() && !catIt->second) {
		return false;
	}
	
	return true;
}

} // namespace pcsc
