#include "TextureLoader.h"
#include <iostream>
#include <fstream>
#include <algorithm>

// This macro tells stb_image to actually compile the implementation code here
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

TextureLoader::TextureLoader() 
    : m_width(0), m_height(0), m_channels(0), 
      m_isAnimated(false), m_currentFrame(0), m_lastFrameTime(0.0) {}

TextureLoader::~TextureLoader() {
    clear();
}

void TextureLoader::clear() {
    if (!m_textureIDs.empty()) {
        glDeleteTextures((GLsizei)m_textureIDs.size(), m_textureIDs.data());
        m_textureIDs.clear();
    }
    m_delays.clear();
    m_width = 0;
    m_height = 0;
    m_channels = 0;
    m_isAnimated = false;
    m_currentFrame = 0;
}

// Safely move the OpenGL IDs to a new object without deleting them
TextureLoader::TextureLoader(TextureLoader&& other) noexcept 
    : m_textureIDs(std::move(other.m_textureIDs)),
      m_delays(std::move(other.m_delays)),
      m_width(other.m_width), m_height(other.m_height), m_channels(other.m_channels),
      m_isAnimated(other.m_isAnimated), m_currentFrame(other.m_currentFrame), m_lastFrameTime(other.m_lastFrameTime) {
    
    other.m_textureIDs.clear(); // Nullify the old one so the destructor doesn't delete it
    other.clear();
}

TextureLoader& TextureLoader::operator=(TextureLoader&& other) noexcept {
    if (this != &other) {
        clear(); // Clean up our own existing textures first
        
        m_textureIDs = std::move(other.m_textureIDs);
        m_delays = std::move(other.m_delays);
        m_width = other.m_width;
        m_height = other.m_height;
        m_channels = other.m_channels;
        m_isAnimated = other.m_isAnimated;
        m_currentFrame = other.m_currentFrame;
        m_lastFrameTime = other.m_lastFrameTime;

        other.m_textureIDs.clear();
        other.clear();
    }
    return *this;
}

// Helper to check file extensions case-insensitively
std::string toLower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c){ return std::tolower(c); });
    return s;
}

bool TextureLoader::loadFromFile(const std::string& filepath) {
    clear(); // Ensure we start clean

    std::string lowerPath = toLower(filepath);
    if (lowerPath.length() >= 4 && lowerPath.substr(lowerPath.length() - 4) == ".gif") {
        // --- LOAD ANIMATED GIF ---
        std::ifstream file(filepath, std::ios::binary | std::ios::ate);
        if (!file.is_open()) return false;
        std::streamsize size = file.tellg();
        file.seekg(0, std::ios::beg);
        std::vector<unsigned char> buffer(size);
        if (!file.read(reinterpret_cast<char*>(buffer.data()), size)) return false;

        int* delays = nullptr;
        int frameCount = 0;
        
        // Extract all frames and delays into memory
        stbi_uc* data = stbi_load_gif_from_memory(buffer.data(), (int)size, &delays, &m_width, &m_height, &frameCount, &m_channels, 4);
        
        if (!data) return false;

        m_isAnimated = true;
        m_textureIDs.resize(frameCount);
        m_delays.assign(delays, delays + frameCount);
        glGenTextures(frameCount, m_textureIDs.data());

        int frameSize = m_width * m_height * 4;
        
        // Push every frame to the GPU as its own texture
        for (int i = 0; i < frameCount; ++i) {
            glBindTexture(GL_TEXTURE_2D, m_textureIDs[i]);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE); 
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, m_width, m_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data + (i * frameSize));
        }
        
        // Free CPU RAM
        stbi_image_free(data);
        stbi_image_free(delays);
        
        m_lastFrameTime = glfwGetTime();
        m_currentFrame = 0;
        return true;
    } 
    else {
        // --- LOAD STATIC IMAGE (PNG/JPG) ---
        unsigned char* data = stbi_load(filepath.c_str(), &m_width, &m_height, &m_channels, 4);
        if (!data) return false;

        GLuint texID;
        glGenTextures(1, &texID);
        glBindTexture(GL_TEXTURE_2D, texID);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE); 
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, m_width, m_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
        stbi_image_free(data);

        m_textureIDs.push_back(texID);
        m_isAnimated = false;
        return true;
    }
}

GLuint TextureLoader::getTextureID() {
    if (m_textureIDs.empty()) return 0;
    
    // If it's a static image, just return the only ID we have
    if (!m_isAnimated || m_textureIDs.size() == 1) return m_textureIDs[0];

    // --- ANIMATION TICKER ---
    double currentTime = glfwGetTime();
    double delaySeconds = m_delays[m_currentFrame] / 1000.0;
    
    // Safeguard: Some GIFs have a 0ms delay which causes infinite loops
    if (delaySeconds <= 0.0) delaySeconds = 0.1; 

    // Advance the frame if enough time has passed!
    while (currentTime - m_lastFrameTime >= delaySeconds) {
        m_lastFrameTime += delaySeconds;
        m_currentFrame = (m_currentFrame + 1) % m_textureIDs.size();
        
        delaySeconds = m_delays[m_currentFrame] / 1000.0;
        if (delaySeconds <= 0.0) delaySeconds = 0.1;
    }

    return m_textureIDs[m_currentFrame];
}

int TextureLoader::getWidth() const { return m_width; }
int TextureLoader::getHeight() const { return m_height; }