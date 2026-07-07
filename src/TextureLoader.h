#pragma once

#include <string>
#include <vector>
#include <GLFW/glfw3.h> // We need this for OpenGL types like GLuint

class TextureLoader {
public:
    TextureLoader();
    ~TextureLoader();

    // Loads a PNG/JPG/GIF from the file path and generates OpenGL texture(s)
    bool loadFromFile(const std::string& filepath);

    // Getters for ImGui
    GLuint getTextureID(); // Removed 'const' so the GIF can update its internal timer!
    int getWidth() const;
    int getHeight() const;

    // Prevent accidental copying (Rule of 5 fix)
    TextureLoader(const TextureLoader&) = delete;
    TextureLoader& operator=(const TextureLoader&) = delete;
    
    // Allow safe moving
    TextureLoader(TextureLoader&& other) noexcept;
    TextureLoader& operator=(TextureLoader&& other) noexcept;

private:
    void clear();

    std::vector<GLuint> m_textureIDs;
    std::vector<int> m_delays;
    int m_width;
    int m_height;
    int m_channels;
    
    // Animation state
    bool m_isAnimated;
    int m_currentFrame;
    double m_lastFrameTime;
};