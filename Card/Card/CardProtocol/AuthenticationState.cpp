#include "AuthenticationState.h"
#include "../CardModel/CardMemoryLayout.h"
#include "Result.h"
#include <iostream>
#include <algorithm>

// ════════════════════════════════════════════════════════════════════════════════
// Construction
// ════════════════════════════════════════════════════════════════════════════════

AuthenticationState::AuthenticationState(const CardMemoryLayout& cardMemory,
                                         std::chrono::milliseconds defaultTimeout)
    : cardMemory_(cardMemory), 
      defaultTimeout_(defaultTimeout),
      cachingEnabled_(true) {
}

// ════════════════════════════════════════════════════════════════════════════════
// Authentication Management
// ════════════════════════════════════════════════════════════════════════════════

void AuthenticationState::markAuthenticated(int sector, KeyType kt) {
	if (!isValidSector(sector)) {
		PcscError::make(CardError::InvalidData,
			"Invalid sector: " + std::to_string(sector)).throwIfError();
		return;
	}
    else if (cachingEnabled_) {
        sessions_[sector] = AuthSession(sector, kt, defaultTimeout_);
    }
}

void AuthenticationState::invalidate(int sector) {
    if (isValidSector(sector)) {
        sessions_.erase(sector);
    }
}

void AuthenticationState::clearAll() {
    sessions_.clear();
}

// ════════════════════════════════════════════════════════════════════════════════
// Authentication Queries
// ════════════════════════════════════════════════════════════════════════════════

bool AuthenticationState::isAuthenticated(int sector) const {
    if (!cachingEnabled_) return false;
    
    auto it = sessions_.find(sector);
    if (it == sessions_.end()) return false;
    
    return !it->second.isExpired();
}

bool AuthenticationState::isAuthenticatedWith(int sector, KeyType kt) const {
    if (!isAuthenticated(sector)) return false;
    
    auto it = sessions_.find(sector);
    return it->second.keyUsed == kt;
}

const AuthSession* AuthenticationState::getSession(int sector) const {
    auto it = sessions_.find(sector);
    if (it == sessions_.end()) return nullptr;
    
    if (it->second.isExpired()) {
        return nullptr;
    }
    
    return &it->second;
}

const KeyType* AuthenticationState::getAuthenticatedKey(int sector) const {
    auto session = getSession(sector);
    if (session == nullptr) return nullptr;
    return &session->keyUsed;
}

// ════════════════════════════════════════════════════════════════════════════════
// Batch Queries
// ════════════════════════════════════════════════════════════════════════════════

std::vector<int> AuthenticationState::getAuthenticatedSectors() const {
    std::vector<int> result;
    for (const auto& pair : sessions_) {
        if (!pair.second.isExpired()) {
            result.push_back(pair.first);
        }
    }
    return result;
}

void AuthenticationState::printAuthenticationStatus() const {
    std::cout << "=== Authentication Status ===\n";
    std::cout << "Caching: " << (cachingEnabled_ ? "Enabled" : "Disabled") << "\n";
    std::cout << "Sessions: " << sessions_.size() << " stored\n\n";
    
    if (sessions_.empty()) {
        std::cout << "No authenticated sectors\n";
        return;
    }
    
    int sectorCount = cardMemory_.is4K() ? 40 : 16;
    for (int s = 0; s < sectorCount; ++s) {
        auto it = sessions_.find(s);
        if (it != sessions_.end()) {
            std::cout << "Sector " << s << ": ";
            if (it->second.isExpired()) {
                std::cout << "EXPIRED (";
            } else {
                std::cout << "AUTH (" << (it->second.keyUsed == KeyType::A ? "A" : "B") << ", ";
                auto remaining = getTimeRemaining(s);
                std::cout << remaining.count() << "ms remaining)\n";
            }
        }
    }
}

int AuthenticationState::countAuthenticated() const {
    int count = 0;
    for (const auto& pair : sessions_) {
        if (!pair.second.isExpired()) {
            count++;
        }
    }
    return count;
}

// ════════════════════════════════════════════════════════════════════════════════
// Configuration
// ════════════════════════════════════════════════════════════════════════════════

void AuthenticationState::setDefaultTimeout(std::chrono::milliseconds timeout) {
    defaultTimeout_ = timeout;
}

void AuthenticationState::setSectorTimeout(int sector, std::chrono::milliseconds timeout) {
    auto it = sessions_.find(sector);
    if (it != sessions_.end()) {
        it->second.timeout = timeout;
        it->second.refresh();
    }
}

void AuthenticationState::enableCaching(bool enable) {
    cachingEnabled_ = enable;
    if (!enable) {
        clearAll();
    }
}

bool AuthenticationState::isCachingEnabled() const {
    return cachingEnabled_;
}

// ════════════════════════════════════════════════════════════════════════════════
// Timeout Management
// ════════════════════════════════════════════════════════════════════════════════

void AuthenticationState::refresh(int sector) {
    auto it = sessions_.find(sector);
    if (it != sessions_.end()) {
        it->second.refresh();
    }
}

void AuthenticationState::removeExpiredSessions() {
    std::vector<int> toRemove;
    for (const auto& pair : sessions_) {
        if (pair.second.isExpired()) {
            toRemove.push_back(pair.first);
        }
    }
    for (int sector : toRemove) {
        sessions_.erase(sector);
    }
}

std::chrono::milliseconds AuthenticationState::getTimeRemaining(int sector) const {
    auto it = sessions_.find(sector);
    if (it == sessions_.end()) return std::chrono::milliseconds(0);
    
    auto now = std::chrono::system_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - it->second.authTime);
    auto remaining = it->second.timeout - elapsed;
    
    if (remaining.count() < 0) return std::chrono::milliseconds(0);
    return remaining;
}

// ════════════════════════════════════════════════════════════════════════════════
// Internal Helpers
// ════════════════════════════════════════════════════════════════════════════════

bool AuthenticationState::isValidSector(int sector) const {
    int maxSector = cardMemory_.is4K() ? 40 : 16;
    return sector >= 0 && sector < maxSector;
}
