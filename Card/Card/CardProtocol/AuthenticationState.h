#ifndef AUTHENTICATIONSTATE_H
#define AUTHENTICATIONSTATE_H

#include "../CardModel/CardDataTypes.h"
#include "../CardModel/CardMemoryLayout.h"
#include <map>
#include <chrono>
#include <optional>

// ════════════════════════════════════════════════════════════════════════════════
// AuthenticationState - Track Authentication Sessions and Cache
// ════════════════════════════════════════════════════════════════════════════════
//
// Manages authentication state for sectors:
// - Track which sectors are currently authenticated
// - Which key was used (A or B)
// - Session validity (with timeout)
// - Cache authentication state to avoid re-authenticating
//
// Usage:
// 1. After successful auth: markAuthenticated()
// 2. Before operation: isAuthenticated() check
// 3. After sector change: invalidate old sector
// 4. Timeout handling: automatic expiry
//
// ════════════════════════════════════════════════════════════════════════════════

struct AuthSession {
    int sector;                                          // Which sector
    KeyType keyUsed;                                     // Key A or B
    std::chrono::system_clock::time_point authTime;     // When authenticated
    std::chrono::milliseconds timeout;                  // How long valid
    bool isValid;                                        // Explicitly valid/invalid

    AuthSession() : sector(-1), keyUsed(KeyType::A), timeout(5000), isValid(false) {}
    
    AuthSession(int s, KeyType kt, std::chrono::milliseconds t)
        : sector(s), keyUsed(kt), timeout(t), isValid(true) {
        authTime = std::chrono::system_clock::now();
    }

    // Check if session has expired
    bool isExpired() const {
        if (!isValid) return true;
        auto now = std::chrono::system_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - authTime);
        return elapsed > timeout;
    }

    // Reset timeout
    void refresh() {
        authTime = std::chrono::system_clock::now();
        isValid = true;
    }
};

class AuthenticationState {
public:
    // ────────────────────────────────────────────────────────────────────────────
    // Constructor
    // ────────────────────────────────────────────────────────────────────────────

    explicit AuthenticationState(const CardMemoryLayout& cardMemory,
                                 std::chrono::milliseconds defaultTimeout = std::chrono::milliseconds(5000));

    // ────────────────────────────────────────────────────────────────────────────
    // Authentication Management
    // ────────────────────────────────────────────────────────────────────────────

    // Mark sector as authenticated
    void markAuthenticated(int sector, KeyType kt);

    // Mark sector as not authenticated
    void invalidate(int sector);

    // Clear all authentication
    void clearAll();

    // ────────────────────────────────────────────────────────────────────────────
    // Authentication Queries
    // ────────────────────────────────────────────────────────────────────────────

    // Is sector currently authenticated?
    bool isAuthenticated(int sector) const;

    // Is sector authenticated with specific key?
    bool isAuthenticatedWith(int sector, KeyType kt) const;

    // Get current session info
    const AuthSession* getSession(int sector) const;

    // Get authenticated key type (if authenticated)
    const KeyType* getAuthenticatedKey(int sector) const;

    // ────────────────────────────────────────────────────────────────────────────
    // Batch Queries
    // ────────────────────────────────────────────────────────────────────────────

    // Get all authenticated sectors
    std::vector<int> getAuthenticatedSectors() const;

    // Get authentication status for all sectors
    void printAuthenticationStatus() const;

    // Count authenticated sectors
    int countAuthenticated() const;

    // ────────────────────────────────────────────────────────────────────────────
    // Configuration
    // ────────────────────────────────────────────────────────────────────────────

    // Set default timeout for new authentications
    void setDefaultTimeout(std::chrono::milliseconds timeout);

    // Set timeout for specific sector
    void setSectorTimeout(int sector, std::chrono::milliseconds timeout);

    // Enable/disable authentication caching
    void enableCaching(bool enable);

    bool isCachingEnabled() const;

    // ────────────────────────────────────────────────────────────────────────────
    // Timeout Management
    // ────────────────────────────────────────────────────────────────────────────

    // Refresh authentication session (reset timeout)
    void refresh(int sector);

    // Remove expired sessions
    void removeExpiredSessions();

    // Get time remaining for authentication
    std::chrono::milliseconds getTimeRemaining(int sector) const;

private:
    const CardMemoryLayout& cardMemory_;
    std::chrono::milliseconds defaultTimeout_;
    bool cachingEnabled_;

    // Storage: Sector -> AuthSession
    std::map<int, AuthSession> sessions_;

    // ────────────────────────────────────────────────────────────────────────────
    // Internal Helpers
    // ────────────────────────────────────────────────────────────────────────────

    // Check if sector is valid for card
    bool isValidSector(int sector) const;
};

#endif // AUTHENTICATIONSTATE_H
