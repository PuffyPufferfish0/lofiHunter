#include "EconomyManager.h"
#include <iostream>
#include <thread>
#include <array>
#include <memory>
#include <cstdio>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

EconomyManager::EconomyManager() 
    : m_balance(0), m_secondsAccumulated(0.0f), m_syncTimer(0.0f), m_userId("") {
    m_lastUpdateTime = std::chrono::steady_clock::now();
}

std::string EconomyManager::sendHttpRequest(const std::string& method, const std::string& endpoint, const std::string& jsonPayload) {
    std::string cmd;
#ifdef _WIN32
    std::string escapedPayload = jsonPayload;
    size_t pos = 0;
    while ((pos = escapedPayload.find("\"", pos)) != std::string::npos) {
        escapedPayload.replace(pos, 1, "\\\"");
        pos += 2;
    }
    cmd = "curl -s -X " + method + " http://127.0.0.1:8000" + endpoint + " -H \"Content-Type: application/json\" -d \"" + escapedPayload + "\" 2>&1";
#else
    cmd = "curl -s -X " + method + " http://127.0.0.1:8000" + endpoint + " -H \"Content-Type: application/json\" -d '" + jsonPayload + "' 2>&1";
#endif

    std::array<char, 128> buffer;
    std::string result;
    
#ifdef _WIN32
    std::unique_ptr<FILE, int(*)(FILE*)> pipe(_popen(cmd.c_str(), "r"), _pclose);
#else
    std::unique_ptr<FILE, int(*)(FILE*)> pipe(popen(cmd.c_str(), "r"), pclose);
#endif

    if (!pipe) return "";
    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        result += buffer.data();
    }
    return result;
}

std::string EconomyManager::attemptLogin(const std::string& userId, const std::string& password) {
    json j;
    j["user_id"] = userId;
    j["password"] = password;

    std::string responseStr = sendHttpRequest("POST", "/users/login", j.dump());
    std::cout << "[Network] Login Response: " << responseStr << std::endl;
    
    try {
        if (responseStr.empty()) return "Failed to connect to server.";
        json response = json::parse(responseStr);
        
        if (response.contains("detail")) {
            if (response["detail"].is_array()) return "Validation error. Check inputs.";
            return response["detail"].get<std::string>();
        }
        
        if (response.contains("status") && response["status"] == "success") {
            m_userId = userId;
            m_balance.store(response.value("balance", 0));
            m_secondsAccumulated = 0.0f;
            m_syncTimer = 0.0f;
            m_lastUpdateTime = std::chrono::steady_clock::now();
            std::cout << "[Economy] Logged in as " << m_userId << std::endl;
            return ""; // Success
        }
    } catch (...) { 
        std::cerr << "[Network Error] JSON parse failed." << std::endl;
        return "Invalid server response. Check terminal!"; 
    }
    return "Unknown error.";
}

std::string EconomyManager::registerUser(const std::string& userId, const std::string& username, const std::string& password) {
    json j;
    j["user_id"] = userId;
    j["username"] = username;
    j["password"] = password;

    std::string responseStr = sendHttpRequest("POST", "/users/register", j.dump());
    std::cout << "[Network] Register Response: " << responseStr << std::endl;
    
    try {
        if (responseStr.empty()) return "Failed to connect to server.";
        json response = json::parse(responseStr);
        
        if (response.contains("detail")) {
            if (response["detail"].is_array()) return "Validation error. Check inputs.";
            return response["detail"].get<std::string>();
        }
        
        if (response.contains("message")) return ""; // Success
    } catch (...) { 
        std::cerr << "[Network Error] JSON parse failed." << std::endl;
        return "Invalid server response. Check terminal!"; 
    }
    return "Unknown error.";
}

bool EconomyManager::isLoggedIn() const {
    return !m_userId.empty();
}

void EconomyManager::update(bool isMusicPlaying, float boostMultiplier) {
    auto currentTime = std::chrono::steady_clock::now();
    std::chrono::duration<float> deltaTime = currentTime - m_lastUpdateTime;
    m_lastUpdateTime = currentTime;

    if (isMusicPlaying && isLoggedIn()) {
        m_secondsAccumulated += deltaTime.count() * boostMultiplier;
        m_syncTimer += deltaTime.count(); 

        while (m_secondsAccumulated >= SECONDS_PER_CREDIT) {
            m_secondsAccumulated -= SECONDS_PER_CREDIT;
        }

        if (m_syncTimer >= SECONDS_PER_SYNC) {
            float timeToReport = m_syncTimer;
            m_syncTimer = 0.0f; 
            std::thread([this, timeToReport, boostMultiplier]() {
                sendHeartbeat(timeToReport, boostMultiplier);
            }).detach();
        }
    }
}

int EconomyManager::getBalance() const {
    return m_balance.load(); 
}

float EconomyManager::getProgressToNextCredit() const {
    return m_secondsAccumulated / SECONDS_PER_CREDIT;
}

bool EconomyManager::canAfford(int amount) const {
    return m_balance.load() >= amount;
}

bool EconomyManager::spendCredits(int amount) {
    if (canAfford(amount)) {
        m_balance.fetch_sub(amount); 
        
        std::thread([this, amount]() {
            json j;
            j["user_id"] = m_userId;
            j["amount"] = amount;
            std::string responseStr = sendHttpRequest("POST", "/users/spend", j.dump());
            std::cout << "[Network] Spend Response: " << responseStr << std::endl;
        }).detach();
        
        return true;
    }
    return false;
}

void EconomyManager::sendHeartbeat(float secondsToReport, float multiplier) {
    if (m_userId.empty()) return;

    json j;
    j["user_id"] = m_userId;
    j["seconds_listened"] = secondsToReport;
    j["active_multiplier"] = multiplier;

    std::string responseStr = sendHttpRequest("POST", "/users/heartbeat", j.dump());
    
    try {
        if (!responseStr.empty()) {
            json response = json::parse(responseStr);
            if (response.contains("new_balance")) {
                m_balance.store(response["new_balance"].get<int>());
                std::cout << "[Economy] Synced with server! Bank: " << m_balance.load() << " CR" << std::endl;
            }
        }
    } catch (...) {
        std::cerr << "[Economy] Sync failed. Response: " << responseStr << std::endl;
    }
}