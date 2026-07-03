#pragma once

#include <string>
#include <sys/types.h>

class AudioBackend {
public:
    AudioBackend();
    ~AudioBackend();

    // Process control
    bool start();       // Spawns the lowfi CLI
    void pause();       // Suspends the process (SIGSTOP)
    void resume();      // Resumes the process (SIGCONT)
    void stop();        // Kills the process (SIGKILL)
    void togglePlayPause();

    // State getters
    bool isRunning() const;
    bool isPaused() const;
    
    // Reads any new terminal output from lowfi without freezing our UI
    std::string pollOutput(); 

    // Sends a string command directly to lowfi's standard input
    void sendCommand(const std::string& cmd);

private:
    pid_t m_pid;          // Process ID of the background lowfi instance
    int m_pipeOut[2];     // Pipe to capture lowfi's terminal output
    bool m_isPaused;
};