/** @file SessionFile.h
 * @brief Persistent parameter store (.scardtool_session). */
#pragma once
#include <string>
#include <map>
#include <optional>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <iostream>

// ════════════════════════════════════════════════════════════════════════════════
// SessionFile — .scardtool_session kalıcı parametre deposu
//
// Format (INI-benzeri, sade):
//   # yorum satırı
//   reader=0
//   reader-name=ACR1281U
//
// Arama sırası:
//   1. Çalışma dizini (./.scardtool_session)
//   2. Home dizini   (~/.scardtool_session)
//
// Kullanım:
//   SessionFile sess;
//   if (sess.load()) { ... }
//   auto r = sess.get("reader");
//   sess.set("reader", "0");
//   sess.save();
// ════════════════════════════════════════════════════════════════════════════════

static constexpr const char* SESSION_FILENAME = ".scardtool_session";

class SessionFile {
public:
    // Yükle — önce çalışma dizini, sonra home
    bool load() {
        for (auto& path : candidatePaths()) {
            if (loadFrom(path)) {
                loadedPath_ = path;
                return true;
            }
        }
        return false;
    }

    // Belirli bir path'ten yükle
    bool loadFrom(const std::string& path) {
        std::ifstream f(path);
        if (!f.is_open()) return false;

        data_.clear();
        std::string line;
        while (std::getline(f, line)) {
            // trim
            auto start = line.find_first_not_of(" \t\r");
            if (start == std::string::npos) continue;
            line = line.substr(start);

            if (line.empty() || line[0] == '#') continue;

            auto eq = line.find('=');
            if (eq == std::string::npos) continue;

            std::string key   = trim(line.substr(0, eq));
            std::string value = trim(line.substr(eq + 1));
            if (!key.empty()) data_[key] = value;
        }
        loadedPath_ = path;
        return true;
    }

    // Kaydet — yüklenen path'e, yoksa çalışma dizinine
    bool save(const std::string& path = "") const {
        std::string target = path.empty()
            ? (loadedPath_.empty() ? std::string(SESSION_FILENAME) : loadedPath_)
            : path;

        std::ofstream f(target, std::ios::trunc);
        if (!f.is_open()) {
            std::cerr << "Warning: Cannot write session file: " << target << "\n";
            return false;
        }

        f << "# scardtool session file — auto-generated\n";
        for (auto& [k, v] : data_)
            f << k << "=" << v << "\n";

        std::cout << "Session saved to: " << target << "\n";
        return true;
    }

    std::optional<std::string> get(const std::string& key) const {
        auto it = data_.find(key);
        if (it != data_.end()) return it->second;
        return std::nullopt;
    }

    void set(const std::string& key, const std::string& value) {
        data_[key] = value;
    }

    void remove(const std::string& key) { data_.erase(key); }

    bool has(const std::string& key) const { return data_.count(key) > 0; }

    bool isLoaded() const { return !loadedPath_.empty(); }
    const std::string& loadedPath() const { return loadedPath_; }

    const std::map<std::string, std::string>& all() const { return data_; }

private:
    std::map<std::string, std::string> data_;
    std::string loadedPath_;

    static std::vector<std::string> candidatePaths() {
        std::vector<std::string> paths;
        paths.push_back(SESSION_FILENAME); // cwd

        // home dir
        const char* home = nullptr;
#ifdef _WIN32
        home = std::getenv("USERPROFILE");
        if (!home) home = std::getenv("HOMEDRIVE");
#else
        home = std::getenv("HOME");
#endif
        if (home) {
            std::string homePath = std::string(home) + "/" + SESSION_FILENAME;
            paths.push_back(homePath);
        }
        return paths;
    }

    static std::string trim(const std::string& s) {
        auto start = s.find_first_not_of(" \t\r\n");
        if (start == std::string::npos) return "";
        auto end = s.find_last_not_of(" \t\r\n");
        return s.substr(start, end - start + 1);
    }
};
