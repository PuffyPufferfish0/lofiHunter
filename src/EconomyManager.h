#pragma once
#include <chrono>
#include <string>
#include <atomic>

class EconomyManager {
public:
    EconomyManager();

    // Session Management
    std::string attemptLogin(const std::string& userId, const std::string& password);
    std::string registerUser(const std::string& userId, const std::string& username, const std::string& password);
    bool isLoggedIn() const;

    // Call this once per frame in your main loop
    void update(bool isMusicPlaying, float boostMultiplier = 1.0f);

    // Getters for the UI
    int getBalance() const;
    float getProgressToNextCredit() const;

    // --- NEW: Spending Logic ---
    bool canAfford(int amount) const;
    bool spendCredits(int amount);

private:
    std::atomic<int> m_balance; 
    float m_secondsAccumulated; 
    float m_syncTimer;          
    std::chrono::steady_clock::time_point m_lastUpdateTime;
    std::string m_userId;

    // Networking helpers
    std::string sendHttpRequest(const std::string& method, const std::string& endpoint, const std::string& jsonPayload);
    void sendHeartbeat(float secondsToReport, float multiplier);
    
    const float SECONDS_PER_CREDIT = 10.0f; 
    const float SECONDS_PER_SYNC = 10.0f; 
};