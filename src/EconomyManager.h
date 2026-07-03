#pragma once
#include <chrono>

class EconomyManager {
public:
    EconomyManager();

    // Call this once per frame in your main loop
    void update(bool isMusicPlaying);

    // Getters for the UI
    int getBalance() const;
    float getProgressToNextCredit() const;

private:
    int m_balance;
    float m_secondsAccumulated;
    std::chrono::steady_clock::time_point m_lastUpdateTime;
    
    // How many seconds of listening equal 1 credit?
    // Set to 10 seconds for rapid testing! (Change to 60 for production)
    const float SECONDS_PER_CREDIT = 10.0f; 
};