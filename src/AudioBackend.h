#pragma once

#include <string>

// Include OS-Specific Headers
#ifdef _WIN32
#include <windows.h>
#else
#include <sys/types.h>
#endif

class AudioBackend {
public:
    AudioBackend();
    ~AudioBackend();

    // Process control
    bool start();       
    void pause();       
    void resume();      
    void stop();        
    void togglePlayPause();

    // State getters
    bool isRunning() const;
    bool isPaused() const;
    
    // Terminal interaction
    std::string pollOutput(); 
    void sendCommand(const std::string& cmd);

private:
    bool m_isPaused;

#ifdef _WIN32
    // --- Windows Specific Variables ---
    HANDLE m_hProcess;
    HANDLE m_hPipeOutRead;   // To read lowfi's output
    HANDLE m_hPipeInWrite;   // To send lowfi commands
    DWORD m_processID;
    
    // Windows doesn't have simple pause/resume signals, so we need a custom helper
    void suspendResumeThreads(bool suspend); 
#else
    // --- Linux/POSIX Specific Variables ---
    pid_t m_pid;          
    int m_pipeOut[2];     
#endif
};