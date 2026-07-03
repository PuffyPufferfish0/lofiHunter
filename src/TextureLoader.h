#pragma once

#include <string>
#include <GLFW/glfw3.h> // We need this for OpenGL types like GLuint

class TextureLoader {
public:
    TextureLoader();
    ~TextureLoader();

    // Loads a PNG/JPG from the file path and generates an OpenGL texture
    bool loadFromFile(const std::string& filepath);

    // Getters for ImGui
    GLuint getTextureID() const;
    int getWidth() const;
    int getHeight() const;

private:
    GLuint m_textureID;
    int m_width;
    int m_height;
    int m_channels;
};