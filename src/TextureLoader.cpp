#include "TextureLoader.h"
#include <iostream>

// This macro tells stb_image to actually compile the implementation code here
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

TextureLoader::TextureLoader() : m_textureID(0), m_width(0), m_height(0), m_channels(0) {}

TextureLoader::~TextureLoader() {
    // Clean up the texture from the GPU when the app closes
    if (m_textureID != 0) {
        glDeleteTextures(1, &m_textureID);
    }
}

bool TextureLoader::loadFromFile(const std::string& filepath) {
    // 1. Ask stb_image to load the file into CPU RAM
    // The '4' forces it to load as RGBA (Red, Green, Blue, Alpha)
    unsigned char* data = stbi_load(filepath.c_str(), &m_width, &m_height, &m_channels, 4);
    
    if (!data) {
        // Silently fail, main.cpp will handle the missing file gracefully
        return false; 
    }

    // 2. Generate an OpenGL texture ID
    glGenTextures(1, &m_textureID);
    glBindTexture(GL_TEXTURE_2D, m_textureID);

    // 3. Set texture parameters (How it looks when scaled)
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE); 
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    // 4. Upload the image pixels from CPU RAM to GPU VRAM
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, m_width, m_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
    
    // 5. Free the CPU memory since the GPU has it now
    stbi_image_free(data);
    
    return true;
}

GLuint TextureLoader::getTextureID() const {
    return m_textureID;
}

int TextureLoader::getWidth() const {
    return m_width;
}

int TextureLoader::getHeight() const {
    return m_height;
}