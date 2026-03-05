#include "Log.h"
#include <iostream>
#include <iomanip>
#include <chrono>

namespace pcsc {

Log& Log::getInstance() {
	static Log instance;
	return instance;
}

Log::Log() : m_logLevel(LogLevel::Debug) {
	// Varsayýlan olarak tüm log türleri ve kategoriler açýk
	resetToDefaults();
}

// ============================================================
// Global Log Seviyesi Ayarlarý
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
// Baðýmsýz Log Türü Kontrolleri
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

// Internal helper - lock olmaksýzýn tüm türleri etkinleþtir
// UYARI: Sadece zaten kilitlenmiþ baðlamda kullan
void Log::enableAllLogTypesUnlocked() {
	for (int i = 0; i < 4; ++i) {
		m_enabledLogTypes[i] = true;
	}
}

// Internal helper - lock olmaksýzýn tüm türleri devre dýþý býrak
// UYARI: Sadece zaten kilitlenmiþ baðlamda kullan
void Log::disableAllLogTypesUnlocked() {
	for (int i = 0; i < 4; ++i) {
		m_enabledLogTypes[i] = false;
	}
}

// ============================================================
// Kategori Bazýnda Kontrol
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

// Internal helper - lock olmaksýzýn tüm kategorileri etkinleþtir
// UYARI: Sadece zaten kilitlenmiþ baðlamda kullan
void Log::enableAllCategoriesUnlocked() {
	for (int i = 0; i < 6; ++i) {
		m_enabledCategories[i] = true;
	}
}

// Internal helper - lock olmaksýzýn tüm kategorileri devre dýþý býrak
// UYARI: Sadece zaten kilitlenmiþ baðlamda kullan
void Log::disableAllCategoriesUnlocked() {
	for (int i = 0; i < 6; ++i) {
		m_enabledCategories[i] = false;
	}
}

// ============================================================
// Hazýr Kontrol Fonksiyonlarý
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
// Log Fonksiyonlarý
// ============================================================

void Log::debug(const std::string& message, LogCategory category) {
	if (!shouldLog(LogType::Debug, category)) return;
	std::lock_guard<std::mutex> lock(m_mutex);
	std::cout << "[DEBUG][" << getCategoryName(category) << "] " << message << std::endl;
}

void Log::info(const std::string& message, LogCategory category) {
	if (!shouldLog(LogType::Info, category)) return;
	std::lock_guard<std::mutex> lock(m_mutex);
	std::cout << "[INFO][" << getCategoryName(category) << "] " << message << std::endl;
}

void Log::warning(const std::string& message, LogCategory category) {
	if (!shouldLog(LogType::Warning, category)) return;
	std::lock_guard<std::mutex> lock(m_mutex);
	std::cout << "[WARNING][" << getCategoryName(category) << "] " << message << std::endl;
}

void Log::error(const std::string& message, LogCategory category) {
	if (!shouldLog(LogType::Error, category)) return;
	std::lock_guard<std::mutex> lock(m_mutex);
	std::cerr << "[ERROR][" << getCategoryName(category) << "] " << message << std::endl;
}

// ============================================================
// Konfigürasyon
// ============================================================

void Log::resetToDefaults() {
	std::lock_guard<std::mutex> lock(m_mutex);
	m_logLevel = LogLevel::Debug;
	enableAllLogTypesUnlocked();
	enableAllCategoriesUnlocked();
}

void Log::printSettings() const {
	std::lock_guard<std::mutex> lock(m_mutex);
	std::cout << "\n========== LOG SETTINGS ==========\n";
	std::cout << "Global Log Level: " << getLogLevelName(m_logLevel) << "\n\n";
	
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
