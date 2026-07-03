#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include <stdio.h>
#include <string>
#include <vector>
#define GL_SILENCE_DEPRECATION
#include <GLFW/glfw3.h>

#include "AudioBackend.h"
#include "EconomyManager.h"
#include "TextureLoader.h"

static void glfw_error_callback(int error, const char* description)
{
    fprintf(stderr, "GLFW Error %d: %s\n", error, description);
}

int main(int, char**)
{
    glfwSetErrorCallback(glfw_error_callback);
    
    // FIXED: Removed the stray "AC" typo here!
    if (!glfwInit()) return 1;

    // FIXED: Ensured glsl_version is properly declared here
    const char* glsl_version = "#version 130";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);

    GLFWwindow* window = glfwCreateWindow(1280, 720, "Music Idle Game", nullptr, nullptr);
    if (window == nullptr) return 1;
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
    
    bool hasSkin = skinTexture.loadFromFile("skin.png"); // Make sure your image is named skin.png!
    
    // Auto-start the player on launch!
    audio.start();
    bool isTrackPlaying = true;
    
    std::string consoleOutput = "Waiting for lowfi...\n";
    char commandInput[128] = "";

    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();

        // Update the economy ONLY if the process is running AND not paused
        economy.update(audio.isRunning() && isTrackPlaying);

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        
        std::string newOutput = audio.pollOutput();
        if (!newOutput.empty()) {
            consoleOutput += newOutput;
        }

        // Apply the auto-resize flag here to make the window hug its contents perfectly
        ImGui::Begin("Music Player Controls", nullptr, ImGuiWindowFlags_AlwaysAutoResize); 

        // Calculate a fixed content width based on the skin image instead.
        float uiWidth = hasSkin ? (float)skinTexture.getWidth() : 350.0f;
        if (uiWidth < 350.0f) uiWidth = 350.0f;
        float rightPanelWidth = 150.0f;

        static bool showDebug = false;
        ImGui::Checkbox("Show Debug Controls", &showDebug);
        ImGui::Separator();

        // Row 1: Status (Left) and Credits (Right)
        if (audio.isRunning()) {
            if (isTrackPlaying) {
                ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "Status: PLAYING (Earning...)");
            } else {
                ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "Status: PAUSED (Economy Stopped)");
            }
        } else {
            ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "Status: STOPPED");
        }
        
        ImGui::SameLine(uiWidth - rightPanelWidth);
        std::string balanceText = "Balance: " + std::to_string(economy.getBalance()) + " CR";
        ImGui::TextColored(ImVec4(1.0f, 0.84f, 0.0f, 1.0f), "%s", balanceText.c_str());

        // Row 2: Main Buttons (Left) and Progress Bar (Right)
        if (showDebug) {
            if (ImGui::Button("Start Player")) {
                if (audio.start()) isTrackPlaying = true;
            }
            ImGui::SameLine();
            if (ImGui::Button("Stop Player")) {
                audio.stop();
                isTrackPlaying = false;
            }
            ImGui::SameLine(uiWidth - rightPanelWidth);
        } else {
            // If buttons are hidden, just move the cursor so the progress bar aligns properly
            ImGui::SetCursorPosX(uiWidth - rightPanelWidth);
        }

        char progressOverlay[32];
        sprintf(progressOverlay, "%.1f %%", economy.getProgressToNextCredit() * 100.0f);
        ImGui::ProgressBar(economy.getProgressToNextCredit(), ImVec2(rightPanelWidth, 0.0f), progressOverlay);

        ImGui::Separator();

        if (hasSkin) {
            ImGui::Text("Active Skin:");
            
            ImTextureID my_tex_id = (ImTextureID)(intptr_t)skinTexture.getTextureID();
            
            // 1. Save cursor position BEFORE drawing image (for hitbox offsets)
            ImVec2 imageStartPos = ImGui::GetCursorPos(); 
            
            // 2. Draw the Image
            ImGui::Image(my_tex_id, ImVec2((float)skinTexture.getWidth(), (float)skinTexture.getHeight()));
            
            // 3. Draw Debug UI naturally below the image
            static bool showHitboxes = false;
            static float playX = 145.0f, playY = 325.0f; 
            static float skipX = 220.0f, skipY = 325.0f;
            
            if (showDebug) {
                ImGui::Checkbox("Show Hitbox Outlines (Debug Mode)", &showHitboxes);
                if (showHitboxes) {
                    ImGui::SliderFloat("Play X", &playX, 0.0f, skinTexture.getWidth());
                    ImGui::SliderFloat("Play Y", &playY, 0.0f, skinTexture.getHeight());
                    ImGui::SliderFloat("Skip X", &skipX, 0.0f, skinTexture.getWidth());
                    ImGui::SliderFloat("Skip Y", &skipY, 0.0f, skinTexture.getHeight());
                }
            }

            // 4. Save the cursor position AFTER UI is drawn so we can restore it safely
            ImVec2 endOfUiPos = ImGui::GetCursorPos();

            // 5. Rewind cursor to draw hitboxes directly on top of the image
            float buttonSize = 40.0f;
            
            // --- PLAY BUTTON ---
            ImGui::SetCursorPos(ImVec2(imageStartPos.x + playX, imageStartPos.y + playY));
            if (ImGui::InvisibleButton("SkinPlayPause", ImVec2(buttonSize, buttonSize))) {
                audio.sendCommand("p"); 
                isTrackPlaying = !isTrackPlaying;
            }
            if (showDebug && showHitboxes) {
                ImGui::GetWindowDrawList()->AddRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax(), IM_COL32(255, 0, 0, 255), 0.0f, 0, 2.0f);
            }
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Play/Pause");

            // --- SKIP BUTTON ---
            ImGui::SetCursorPos(ImVec2(imageStartPos.x + skipX, imageStartPos.y + skipY));
            if (ImGui::InvisibleButton("SkinSkip", ImVec2(buttonSize, buttonSize))) {
                audio.sendCommand("s");
            }
            if (showDebug && showHitboxes) {
                ImGui::GetWindowDrawList()->AddRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax(), IM_COL32(0, 255, 0, 255), 0.0f, 0, 2.0f);
            }
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Skip");

            // 6. Restore the cursor to the bottom of the UI and submit a Dummy to satisfy ImGui's bounds checker
            ImGui::SetCursorPos(endOfUiPos);
            ImGui::Dummy(ImVec2(0.0f, 5.0f)); 

        } else {
            ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "No 'skin.png' found in the build directory.");
        }

        if (showDebug) {
            ImGui::Separator();
            ImGui::Text("Terminal Controls:");
            
            if (ImGui::Button("Pause/Play (p)")) { 
                audio.sendCommand("p"); 
                isTrackPlaying = !isTrackPlaying; 
            }
            ImGui::SameLine();
            if (ImGui::Button("Skip (s)")) { audio.sendCommand("s"); }
            ImGui::SameLine();
            if (ImGui::Button("Quit (q)")) { 
                audio.sendCommand("q"); 
                isTrackPlaying = false; 
            }

            ImGui::Separator();
            ImGui::Text("Manual Command Input:");
            
            ImGui::PushItemWidth(uiWidth - 50.0f); // Restrict input box width
            if (ImGui::InputText("##cmd", commandInput, IM_ARRAYSIZE(commandInput), ImGuiInputTextFlags_EnterReturnsTrue)) {
                std::string cmdStr(commandInput);
                if (!cmdStr.empty()) {
                    audio.sendCommand(cmdStr + "\n");
                    commandInput[0] = '\0';
                }
                ImGui::SetKeyboardFocusHere(-1); 
            }
            ImGui::PopItemWidth();
            
            ImGui::SameLine();
            if (ImGui::Button("Send")) {
                std::string cmdStr(commandInput);
                if (!cmdStr.empty()) {
                    audio.sendCommand(cmdStr + "\n");
                    commandInput[0] = '\0';
                }
            }

            ImGui::Separator();
            ImGui::Text("lowfi Terminal Output:");
            
            // Give the terminal a fixed height (40px as requested) and lock to uiWidth
            ImGui::BeginChild("ScrollingRegion", ImVec2(uiWidth, 40.0f), false, ImGuiWindowFlags_HorizontalScrollbar);
            ImGui::TextUnformatted(consoleOutput.c_str());
            if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
                ImGui::SetScrollHereY(1.0f);
            ImGui::EndChild();
        }

        ImGui::End();

        ImGui::Render();
        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(clear_color.x * clear_color.w, clear_color.y * clear_color.w, clear_color.z * clear_color.w, clear_color.w);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}