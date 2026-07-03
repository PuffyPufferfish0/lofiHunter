#include "AudioBackend.h"
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include <fcntl.h>
#include <iostream>
#include <pty.h>     // For openpty()
#include <termios.h> // For terminal settings
#include <regex>     // ADDED: For stripping ANSI escape codes

AudioBackend::AudioBackend() : m_pid(-1), m_isPaused(false) {
    m_pipeOut[0] = -1; // We will use this to store the master PTY file descriptor
    m_pipeOut[1] = -1; // Unused in PTY setup, but kept for consistency
}

AudioBackend::~AudioBackend() {
    stop();
}

bool AudioBackend::start() {
    if (isRunning()) return true;

    int master_fd, slave_fd;
    
    // Create a pseudo-terminal. 
    // master_fd is what our app reads from.
    // slave_fd is what lowfi thinks is its standard output/input.
    if (openpty(&master_fd, &slave_fd, nullptr, nullptr, nullptr) == -1) {
        std::cerr << "Failed to open PTY" << std::endl;
        return false;
    }

    m_pid = fork();

    if (m_pid == 0) {
        // --- CHILD PROCESS ---
        close(master_fd); // Child doesn't need the master end

        // Create a new session and set the slave PTY as the controlling terminal
        setsid();
        if (ioctl(slave_fd, TIOCSCTTY, nullptr) == -1) {
             exit(1);
        }

        // Redirect standard input, output, and error to the slave PTY
        dup2(slave_fd, STDIN_FILENO);
        dup2(slave_fd, STDOUT_FILENO);
        dup2(slave_fd, STDERR_FILENO);
        
        // Close the original slave_fd since it's duplicated to standard streams
        close(slave_fd);

        // Execute lowfi WITHOUT the "play" argument, but force it to be minimal
        // We use -m to hide the bottom bar, reducing garbage text we have to parse later.
        execlp("lowfi", "lowfi", "-m", nullptr);

        // If execlp returns, it failed to find/run the command
        std::cerr << "Failed to execute lowfi. Is it installed?" << std::endl;
        exit(1); 
    } 
    else if (m_pid > 0) {
        // --- PARENT PROCESS (Our App) ---
        close(slave_fd); // Parent doesn't need the slave end

        // Store the master_fd so we can read from it later
        m_pipeOut[0] = master_fd; 

        // Make the master PTY NON-BLOCKING so our ImGui loop doesn't freeze
        int flags = fcntl(m_pipeOut[0], F_GETFL, 0);
        fcntl(m_pipeOut[0], F_SETFL, flags | O_NONBLOCK);

        m_isPaused = false;
        return true;
    }

    // Fork failed
    close(master_fd);
    close(slave_fd);
    return false; 
}

void AudioBackend::pause() {
    if (isRunning() && !m_isPaused) {
        kill(m_pid, SIGSTOP); // Suspend process at OS level
        m_isPaused = true;
    }
}

void AudioBackend::resume() {
    if (isRunning() && m_isPaused) {
        kill(m_pid, SIGCONT); // Resume process
        m_isPaused = false;
    }
}

void AudioBackend::togglePlayPause() {
    if (!isRunning()) {
        start();
    } else if (m_isPaused) {
        resume();
    } else {
        pause();
    }
}

void AudioBackend::stop() {
    if (isRunning()) {
        kill(m_pid, SIGKILL);
        waitpid(m_pid, nullptr, 0); // Wait for OS to clean it up
        m_pid = -1;
        m_isPaused = false;
        
        if (m_pipeOut[0] != -1) {
            close(m_pipeOut[0]);
            m_pipeOut[0] = -1;
        }
    }
}

bool AudioBackend::isRunning() const {
    if (m_pid <= 0) return false;
    
    // Check if process is still alive (WNOHANG means don't block)
    int status;
    pid_t result = waitpid(m_pid, &status, WNOHANG);
    
    // If waitpid returns the PID, the process has exited
    if (result == m_pid) {
        const_cast<AudioBackend*>(this)->m_pid = -1;
        return false;
    }
    return true;
}

bool AudioBackend::isPaused() const {
    return m_isPaused;
}

std::string AudioBackend::pollOutput() {
    if (m_pipeOut[0] == -1) return "";

    char buffer[256];
    ssize_t bytesRead = read(m_pipeOut[0], buffer, sizeof(buffer) - 1);
    
    if (bytesRead > 0) {
        buffer[bytesRead] = '\0';
        std::string rawOutput(buffer);
        
        // Strip ANSI escape codes using a regular expression
        // ADDED '?': Handles terminal cursor hiding codes like [?25l
        static const std::regex ansiRegex("\x1B\\[[0-9;?]*[a-zA-Z]");
        return std::regex_replace(rawOutput, ansiRegex, "");
    }
    return "";
}

void AudioBackend::sendCommand(const std::string& cmd) {
    if (m_pipeOut[0] == -1 || !isRunning()) return;
    
    // Write the command to the master PTY file descriptor
    write(m_pipeOut[0], cmd.c_str(), cmd.length());
}