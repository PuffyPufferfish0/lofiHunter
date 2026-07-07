#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include <stdio.h>
#include <string>
#include <vector>
#include <map>
#include <fstream>
#include <sstream>
#include <iostream>
#include <nlohmann/json.hpp>
#define GL_SILENCE_DEPRECATION
#include <GLFW/glfw3.h>

#include "AudioBackend.h"
#include "EconomyManager.h"
#include "TextureLoader.h"

using json = nlohmann::json;

struct ItemStats { float boostMultiplier; int cost; };
struct PlacedItem {
    std::string id;
    TextureLoader texture;
    ImVec2 pos;
    float scale = 0.35f;
};

std::map<std::string, ItemStats> loadCatalog(const std::string& filepath) {
    std::map<std::string, ItemStats> catalog;
    std::ifstream file(filepath);
    if (!file.is_open()) return catalog;

    std::string line;
    while (std::getline(file, line)) {
        std::stringstream ss(line);
        std::string id, boostStr, costStr;
        if (std::getline(ss, id, '-') && std::getline(ss, boostStr, '-') && std::getline(ss, costStr, '-')) {
            catalog[id] = { std::stof(boostStr), std::stoi(costStr) };
        }
    }
    return catalog;
}

void saveRoom(const std::vector<PlacedItem>& activeItems, const std::string& filepath) {
    json j = json::array();
    for (const auto& item : activeItems) {
        j.push_back({{"id", item.id}, {"x", item.pos.x}, {"y", item.pos.y}});
    }
    std::ofstream outFile(filepath);
    if (outFile.is_open()) outFile << j.dump(4);
}

bool loadRoom(std::vector<PlacedItem>& activeItems, const std::string& filepath) {
    std::ifstream inFile(filepath);
    if (!inFile.is_open()) return false;
    
    try {
        json j;
        inFile >> j;
        for (const auto& itemJson : j) {
            std::string id = itemJson.value("id", "");
            float x = itemJson.value("x", 50.0f);
            float y = itemJson.value("y", 100.0f);
            
            PlacedItem item;
            item.id = id;
            item.pos = ImVec2(x, y);
            if (item.texture.loadFromFile("../itemAssets/" + id + ".gif") || 
                item.texture.loadFromFile("../itemAssets/" + id + ".png")) {
                activeItems.push_back(std::move(item));
            }
        }
        return true;
    } catch (...) { return false; }
}

static void glfw_error_callback(int error, const char* description) {
    fprintf(stderr, "GLFW Error %d: %s\n", error, description);
}

int main(int, char**) {
    glfwSetErrorCallback(glfw_error_callback);
    if (!glfwInit()) return 1;

    const char* glsl_version = "#version 130";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    glfwWindowHint(GLFW_MAXIMIZED, GLFW_TRUE);
    
    GLFWwindow* window = glfwCreateWindow(1280, 720, "Music Idle Game", nullptr, nullptr);
    if (!window) return 1;
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1); 

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;    
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;         
    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init(glsl_version);

    ImVec4 clear_color = ImVec4(0.12f, 0.12f, 0.12f, 1.00f);
    
    AudioBackend audio;
    EconomyManager economy; 
    TextureLoader skinTexture; 
    
    std::string skinImageFile = "skin.png";
    float playX = 145.0f, playY = 325.0f, playW = 40.0f, playH = 40.0f;
    float skipX = 220.0f, skipY = 325.0f, skipW = 40.0f, skipH = 40.0f;

    std::ifstream skinFile("skin.json");
    if (skinFile.is_open()) {
        try {
            json config; skinFile >> config;
            skinImageFile = config.value("image", "skin.png");
            if (config.contains("hitboxes")) {
                auto& hb = config["hitboxes"];
                if (hb.contains("play")) { playX = hb["play"].value("x", 145.0f); playY = hb["play"].value("y", 325.0f); playW = hb["play"].value("width", 40.0f); playH = hb["play"].value("height", 40.0f); }
                if (hb.contains("skip")) { skipX = hb["skip"].value("x", 220.0f); skipY = hb["skip"].value("y", 325.0f); skipW = hb["skip"].value("width", 40.0f); skipH = hb["skip"].value("height", 40.0f); }
            }
        } catch (...) {}
    }
    bool hasSkin = skinTexture.loadFromFile(skinImageFile); 
    
    std::map<std::string, ItemStats> itemCatalog = loadCatalog("../catalog.txt");
    std::vector<PlacedItem> activeItems;
    
    // UI State for Login
    bool showRegisterTab = false;
    char inId[64] = "", inPass[64] = "", regId[64] = "", regUser[64] = "", regPass[64] = "";
    std::string authMessage = "";
    ImVec4 authMessageColor = ImVec4(1.0f, 0.0f, 0.0f, 1.0f); // Default to Red Error

    bool userInitialized = false;
    std::string roomSaveFile = "";
    bool isTrackPlaying = true;
    std::string consoleOutput = "Waiting for lowfi...\n";
    char commandInput[128] = "";

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        
        // ==========================================
        // 1. LOGIN & REGISTRATION SCREEN
        // ==========================================
        if (!economy.isLoggedIn()) {
            ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
            ImGui::Begin("lofiHunter Network", nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMove);
            
            // Tab Selection
            if (ImGui::Button("Sign In", ImVec2(120, 30))) { showRegisterTab = false; authMessage = ""; }
            ImGui::SameLine();
            if (ImGui::Button("Create Account", ImVec2(120, 30))) { showRegisterTab = true; authMessage = ""; }
            ImGui::Separator();
            ImGui::Dummy(ImVec2(0.0f, 5.0f));

            if (!showRegisterTab) {
                // --- LOGIN VIEW ---
                ImGui::Text("Player ID:");
                ImGui::SetNextItemWidth(250.0f);
                if (ImGui::IsWindowAppearing()) ImGui::SetKeyboardFocusHere();
                ImGui::InputText("##loginId", inId, IM_ARRAYSIZE(inId));
                
                ImGui::Text("Password:");
                ImGui::SetNextItemWidth(250.0f);
                bool enterPressed = ImGui::InputText("##loginPass", inPass, IM_ARRAYSIZE(inPass), ImGuiInputTextFlags_Password | ImGuiInputTextFlags_EnterReturnsTrue);
                
                ImGui::Dummy(ImVec2(0.0f, 10.0f));
                if (ImGui::Button("Login", ImVec2(250.0f, 40.0f)) || enterPressed) {
                    std::string idStr(inId);
                    std::string passStr(inPass);
                    if (!idStr.empty() && !passStr.empty()) {
                        authMessage = economy.attemptLogin(idStr, passStr);
                        if (authMessage.empty()) {
                            // Login Success! (Handled by the EconomyManager)
                        } else {
                            authMessageColor = ImVec4(1.0f, 0.2f, 0.2f, 1.0f);
                        }
                    } else {
                        authMessage = "Please fill out all fields.";
                        authMessageColor = ImVec4(1.0f, 0.2f, 0.2f, 1.0f);
                    }
                }
            } else {
                // --- REGISTER VIEW ---
                ImGui::Text("Choose a Player ID (No Spaces):");
                ImGui::SetNextItemWidth(250.0f);
                if (ImGui::IsWindowAppearing()) ImGui::SetKeyboardFocusHere();
                ImGui::InputText("##regId", regId, IM_ARRAYSIZE(regId));

                ImGui::Text("Display Name:");
                ImGui::SetNextItemWidth(250.0f);
                ImGui::InputText("##regUser", regUser, IM_ARRAYSIZE(regUser));
                
                ImGui::Text("Password:");
                ImGui::SetNextItemWidth(250.0f);
                bool enterPressed = ImGui::InputText("##regPass", regPass, IM_ARRAYSIZE(regPass), ImGuiInputTextFlags_Password | ImGuiInputTextFlags_EnterReturnsTrue);
                
                ImGui::Dummy(ImVec2(0.0f, 10.0f));
                if (ImGui::Button("Register", ImVec2(250.0f, 40.0f)) || enterPressed) {
                    std::string idStr(regId);
                    std::string userStr(regUser);
                    std::string passStr(regPass);
                    if (!idStr.empty() && !userStr.empty() && !passStr.empty()) {
                        authMessage = economy.registerUser(idStr, userStr, passStr);
                        if (authMessage.empty()) {
                            authMessage = "Account created! Please Sign In.";
                            authMessageColor = ImVec4(0.2f, 1.0f, 0.2f, 1.0f); // Green success
                            showRegisterTab = false; // Kick them over to the sign in tab
                        } else {
                            authMessageColor = ImVec4(1.0f, 0.2f, 0.2f, 1.0f); // Red Error
                        }
                    } else {
                        authMessage = "Please fill out all fields.";
                        authMessageColor = ImVec4(1.0f, 0.2f, 0.2f, 1.0f);
                    }
                }
            }

            if (!authMessage.empty()) {
                ImGui::Dummy(ImVec2(0.0f, 5.0f));
                ImGui::TextColored(authMessageColor, "%s", authMessage.c_str());
            }

            ImGui::End();

        } else {
            // ==========================================
            // 2. MAIN GAME LOOP
            // ==========================================
            if (!userInitialized) {
                std::string currentUserId(inId);
                roomSaveFile = "../itemAssets/room_" + currentUserId + ".json";
                if (!loadRoom(activeItems, roomSaveFile)) {
                    float startX = 50.0f;
                    for (const auto& pair : itemCatalog) {
                        PlacedItem item; item.id = pair.first; item.pos = ImVec2(startX, 100.0f); 
                        if (item.texture.loadFromFile("../itemAssets/" + pair.first + ".gif") || item.texture.loadFromFile("../itemAssets/" + pair.first + ".png")) {
                            activeItems.push_back(std::move(item)); startX += 150.0f; 
                        }
                    }
                    saveRoom(activeItems, roomSaveFile);
                }
                audio.start();
                userInitialized = true;
            }

            float currentEconomyBoost = 1.0f;
            for (const auto& item : activeItems) {
                if (itemCatalog.count(item.id)) currentEconomyBoost += itemCatalog[item.id].boostMultiplier;
            }

            economy.update(audio.isRunning() && isTrackPlaying, currentEconomyBoost);

            std::string newOutput = audio.pollOutput();
            if (!newOutput.empty()) consoleOutput += newOutput;

            // ROOM CANVAS
            const ImGuiViewport* viewport = ImGui::GetMainViewport();
            ImGui::SetNextWindowPos(viewport->WorkPos);
            ImGui::SetNextWindowSize(viewport->WorkSize);
            ImGuiWindowFlags canvasFlags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoBackground;
            ImGui::Begin("Room Canvas", nullptr, canvasFlags);
            bool roomNeedsSaving = false;

            for (auto it = activeItems.begin(); it != activeItems.end(); ++it) {
                ImVec2 scaledSize(it->texture.getWidth() * it->scale, it->texture.getHeight() * it->scale);
                ImGui::SetCursorPos(it->pos);
                ImGui::Image((ImTextureID)(intptr_t)it->texture.getTextureID(), scaledSize);
                ImGui::SetCursorPos(it->pos);
                ImGui::InvisibleButton(it->id.c_str(), scaledSize);
                
                if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                    PlacedItem clickedItem = std::move(*it);
                    activeItems.erase(it); activeItems.push_back(std::move(clickedItem));
                    roomNeedsSaving = true; break; 
                }
                if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
                    activeItems.back().pos.x += io.MouseDelta.x; activeItems.back().pos.y += io.MouseDelta.y;
                }
                if (ImGui::IsItemDeactivated()) roomNeedsSaving = true;
                if (ImGui::IsItemHovered()) {
                    if (itemCatalog.count(it->id)) ImGui::SetTooltip("Item ID: %s\nBoost: +%.0f%% Speed", it->id.c_str(), itemCatalog[it->id].boostMultiplier * 100.0f);
                }
            }
            ImGui::End();
            if (roomNeedsSaving) saveRoom(activeItems, roomSaveFile);

            // MUSIC CONTROLS
            ImGui::Begin("Music Player Controls", nullptr, ImGuiWindowFlags_AlwaysAutoResize); 
            float uiWidth = hasSkin ? (float)skinTexture.getWidth() : 350.0f;
            if (uiWidth < 350.0f) uiWidth = 350.0f;
            float rightPanelWidth = 150.0f;

            static bool showDebug = false; ImGui::Checkbox("Show Debug Controls", &showDebug); ImGui::Separator();
            if (audio.isRunning()) {
                if (isTrackPlaying) ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "Status: PLAYING (%.2fx Boost)", currentEconomyBoost);
                else ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "Status: PAUSED");
            } else ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "Status: STOPPED");
            
            ImGui::SameLine(uiWidth - rightPanelWidth);
            std::string balanceText = "Balance: " + std::to_string(economy.getBalance()) + " CR";
            ImGui::TextColored(ImVec4(1.0f, 0.84f, 0.0f, 1.0f), "%s", balanceText.c_str());

            if (showDebug) {
                if (ImGui::Button("Start Player")) { if (audio.start()) isTrackPlaying = true; } ImGui::SameLine();
                if (ImGui::Button("Stop Player")) { audio.stop(); isTrackPlaying = false; }
            }
            ImGui::SameLine(uiWidth - rightPanelWidth);

            char progressOverlay[32]; sprintf(progressOverlay, "%.1f %%", economy.getProgressToNextCredit() * 100.0f);
            ImGui::ProgressBar(economy.getProgressToNextCredit(), ImVec2(rightPanelWidth, 0.0f), progressOverlay);
            ImGui::Separator();

            if (hasSkin) {
                ImTextureID my_tex_id = (ImTextureID)(intptr_t)skinTexture.getTextureID();
                ImVec2 imageStartPos = ImGui::GetCursorPos(); 
                ImGui::Image(my_tex_id, ImVec2((float)skinTexture.getWidth(), (float)skinTexture.getHeight()));
                
                static bool showHitboxes = false;
                if (showDebug) {
                    ImGui::Checkbox("Show Hitbox Outlines", &showHitboxes);
                    if (showHitboxes) {
                        ImGui::PushItemWidth(uiWidth - 100.0f);
                        ImGui::SliderFloat("Play X", &playX, 0.0f, skinTexture.getWidth()); ImGui::SliderFloat("Play Y", &playY, 0.0f, skinTexture.getHeight()); ImGui::SliderFloat("Play W", &playW, 10.0f, 100.0f); ImGui::SliderFloat("Play H", &playH, 10.0f, 100.0f); ImGui::Separator();
                        ImGui::SliderFloat("Skip X", &skipX, 0.0f, skinTexture.getWidth()); ImGui::SliderFloat("Skip Y", &skipY, 0.0f, skinTexture.getHeight()); ImGui::SliderFloat("Skip W", &skipW, 10.0f, 100.0f); ImGui::SliderFloat("Skip H", &skipH, 10.0f, 100.0f); ImGui::PopItemWidth();
                        if (ImGui::Button("Save Hitboxes", ImVec2(uiWidth, 30))) {
                            json j; j["image"] = skinImageFile; j["hitboxes"]["play"] = {{"x", playX}, {"y", playY}, {"width", playW}, {"height", playH}}; j["hitboxes"]["skip"] = {{"x", skipX}, {"y", skipY}, {"width", skipW}, {"height", skipH}};
                            std::ofstream outFile("skin.json"); if (outFile.is_open()) outFile << j.dump(4);
                        }
                    }
                }
                ImVec2 endOfUiPos = ImGui::GetCursorPos();
                ImGui::SetCursorPos(ImVec2(imageStartPos.x + playX, imageStartPos.y + playY));
                if (ImGui::InvisibleButton("SkinPlayPause", ImVec2(playW, playH))) { audio.sendCommand("p"); isTrackPlaying = !isTrackPlaying; }
                if (showDebug && showHitboxes) ImGui::GetWindowDrawList()->AddRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax(), IM_COL32(255, 0, 0, 255), 0.0f, 0, 2.0f);
                
                ImGui::SetCursorPos(ImVec2(imageStartPos.x + skipX, imageStartPos.y + skipY));
                if (ImGui::InvisibleButton("SkinSkip", ImVec2(skipW, skipH))) audio.sendCommand("s");
                if (showDebug && showHitboxes) ImGui::GetWindowDrawList()->AddRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax(), IM_COL32(0, 255, 0, 255), 0.0f, 0, 2.0f);
                ImGui::SetCursorPos(endOfUiPos); ImGui::Dummy(ImVec2(0.0f, 5.0f)); 
            } 

            if (showDebug) {
                ImGui::Separator(); ImGui::Text("Terminal Controls:");
                if (ImGui::Button("Pause/Play (p)")) { audio.sendCommand("p"); isTrackPlaying = !isTrackPlaying; } ImGui::SameLine();
                if (ImGui::Button("Skip (s)")) { audio.sendCommand("s"); } ImGui::SameLine();
                if (ImGui::Button("Quit (q)")) { audio.sendCommand("q"); isTrackPlaying = false; } ImGui::Separator();
                ImGui::PushItemWidth(uiWidth - 50.0f);
                if (ImGui::InputText("##cmd", commandInput, IM_ARRAYSIZE(commandInput), ImGuiInputTextFlags_EnterReturnsTrue)) {
                    std::string cmdStr(commandInput); if (!cmdStr.empty()) { audio.sendCommand(cmdStr + "\n"); commandInput[0] = '\0'; } ImGui::SetKeyboardFocusHere(-1); 
                }
                ImGui::PopItemWidth(); ImGui::SameLine();
                if (ImGui::Button("Send")) { std::string cmdStr(commandInput); if (!cmdStr.empty()) { audio.sendCommand(cmdStr + "\n"); commandInput[0] = '\0'; } }
                ImGui::Separator();
                ImGui::BeginChild("ScrollingRegion", ImVec2(uiWidth, 40.0f), false, ImGuiWindowFlags_HorizontalScrollbar); ImGui::TextUnformatted(consoleOutput.c_str()); if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) ImGui::SetScrollHereY(1.0f); ImGui::EndChild();
            }
            ImGui::End();
        }

        ImGui::Render();
        int display_w, display_h; glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(clear_color.x * clear_color.w, clear_color.y * clear_color.w, clear_color.z * clear_color.w, clear_color.w);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(window);
    }

    ImGui_ImplOpenGL3_Shutdown(); ImGui_ImplGlfw_Shutdown(); ImGui::DestroyContext();
    glfwDestroyWindow(window); glfwTerminate();
    return 0;
}