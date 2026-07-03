#include "EconomyManager.h"

EconomyManager::EconomyManager() 
    : m_balance(0), m_secondsAccumulated(0.0f) {
    // Initialize the clock
    m_lastUpdateTime = std::chrono::steady_clock::now();
}

void EconomyManager::update(bool isMusicPlaying) {
    // Calculate how much real time has passed since the last frame
    auto currentTime = std::chrono::steady_clock::now();
    std::chrono::duration<float> deltaTime = currentTime - m_lastUpdateTime;
    m_lastUpdateTime = currentTime;

    // Only add time if the music process is actually running
    if (isMusicPlaying) {
        m_secondsAccumulated += deltaTime.count();

        // If we hit our threshold, grant a credit and reset the timer!
        while (m_secondsAccumulated >= SECONDS_PER_CREDIT) {
            m_balance++;
            m_secondsAccumulated -= SECONDS_PER_CREDIT;
        }
    }
}

int EconomyManager::getBalance() const {
    return m_balance;
}

float EconomyManager::getProgressToNextCredit() const {
    // Returns a value between 0.0 and 1.0 (perfect for a UI progress bar)
    return m_secondsAccumulated / SECONDS_PER_CREDIT;
}