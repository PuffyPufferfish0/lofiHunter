#include "AudioBackend.h"
#include <iostream>
#include <regex>

#ifdef _WIN32
// ==========================================
// WINDOWS IMPLEMENTATION
// ==========================================
#include <tlhelp32.h>

AudioBackend::AudioBackend() : m_isPaused(false), m_hProcess(NULL), m_hPipeOutRead(NULL), m_hPipeInWrite(NULL), m_processID(0) {}

AudioBackend::~AudioBackend() { stop(); }

bool AudioBackend::start() {
    if (isRunning()) return true;

    // 1. Setup security attributes to allow our child process to inherit our pipes
    SECURITY_ATTRIBUTES saAttr;
    saAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
    saAttr.bInheritHandle = TRUE;
    saAttr.lpSecurityDescriptor = NULL;

    HANDLE hPipeOutWrite = NULL;
    HANDLE hPipeInRead = NULL;

    // 2. Create pipes for Output (from lowfi) and Input (to lowfi)
    if (!CreatePipe(&m_hPipeOutRead, &hPipeOutWrite, &saAttr, 0)) return false;
    if (!SetHandleInformation(m_hPipeOutRead, HANDLE_FLAG_INHERIT, 0)) return false; // Parent keeps read end

    if (!CreatePipe(&hPipeInRead, &m_hPipeInWrite, &saAttr, 0)) return false;
    if (!SetHandleInformation(m_hPipeInWrite, HANDLE_FLAG_INHERIT, 0)) return false; // Parent keeps write end

    // 3. Configure the child process to use our pipes instead of the real terminal
    STARTUPINFOA si;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    si.hStdError = hPipeOutWrite;
    si.hStdOutput = hPipeOutWrite;
    si.hStdInput = hPipeInRead;
    si.dwFlags |= STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE; // Hide the ugly Windows cmd.exe popup!

    PROCESS_INFORMATION pi;
    ZeroMemory(&pi, sizeof(pi));

    // 4. Start lowfi! (CreateProcess requires a mutable string)
    char cmd[] = "lowfi -m";
    if (!CreateProcessA(NULL, cmd, NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi)) {
        std::cerr << "Failed to start lowfi on Windows. Is it installed and in your PATH?" << std::endl;
        CloseHandle(m_hPipeOutRead); m_hPipeOutRead = NULL;
        CloseHandle(m_hPipeInWrite); m_hPipeInWrite = NULL;
        CloseHandle(hPipeOutWrite);
        CloseHandle(hPipeInRead);
        return false;
    }

    // Save handles and close the ones we don't need
    m_hProcess = pi.hProcess;
    m_processID = pi.dwProcessId;
    CloseHandle(pi.hThread);
    CloseHandle(hPipeOutWrite);
    CloseHandle(hPipeInRead);

    m_isPaused = false;
    return true;
}

void AudioBackend::suspendResumeThreads(bool suspend) {
    if (!m_hProcess) return;
    HANDLE hThreadSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
    THREADENTRY32 te32;
    te32.dwSize = sizeof(THREADENTRY32);

    if (Thread32First(hThreadSnapshot, &te32)) {
        do {
            if (te32.th32OwnerProcessID == m_processID) {
                HANDLE hThread = OpenThread(THREAD_SUSPEND_RESUME, FALSE, te32.th32ThreadID);
                if (hThread) {
                    if (suspend) SuspendThread(hThread);
                    else ResumeThread(hThread);
                    CloseHandle(hThread);
                }
            }
        } while (Thread32Next(hThreadSnapshot, &te32));
    }
    CloseHandle(hThreadSnapshot);
}

void AudioBackend::pause() {
    if (isRunning() && !m_isPaused) {
        suspendResumeThreads(true);
        m_isPaused = true;
    }
}

void AudioBackend::resume() {
    if (isRunning() && m_isPaused) {
        suspendResumeThreads(false);
        m_isPaused = false;
    }
}

void AudioBackend::stop() {
    if (isRunning()) {
        TerminateProcess(m_hProcess, 0);
        WaitForSingleObject(m_hProcess, INFINITE);
        CloseHandle(m_hProcess);
        m_hProcess = NULL;
    }
    if (m_hPipeOutRead) { CloseHandle(m_hPipeOutRead); m_hPipeOutRead = NULL; }
    if (m_hPipeInWrite) { CloseHandle(m_hPipeInWrite); m_hPipeInWrite = NULL; }
    m_isPaused = false;
}

bool AudioBackend::isRunning() const {
    if (!m_hProcess) return false;
    DWORD exitCode;
    if (GetExitCodeProcess(m_hProcess, &exitCode) && exitCode == STILL_ACTIVE) return true;
    
    // Process has died, clean up handle
    const_cast<AudioBackend*>(this)->m_hProcess = NULL;
    return false;
}

bool AudioBackend::isPaused() const { return m_isPaused; }

void AudioBackend::togglePlayPause() {
    if (!isRunning()) start();
    else if (m_isPaused) resume();
    else pause();
}

std::string AudioBackend::pollOutput() {
    if (!m_hPipeOutRead) return "";

    DWORD bytesAvail;
    // Check if data is available without freezing the app
    if (PeekNamedPipe(m_hPipeOutRead, NULL, 0, NULL, &bytesAvail, NULL) && bytesAvail > 0) {
        char buffer[256];
        DWORD bytesRead;
        if (ReadFile(m_hPipeOutRead, buffer, sizeof(buffer) - 1, &bytesRead, NULL) && bytesRead > 0) {
            buffer[bytesRead] = '\0';
            std::string rawOutput(buffer);
            static const std::regex ansiRegex("\x1B\\[[0-9;?]*[a-zA-Z]");
            return std::regex_replace(rawOutput, ansiRegex, "");
        }
    }
    return "";
}

void AudioBackend::sendCommand(const std::string& cmd) {
    if (!m_hPipeInWrite || !isRunning()) return;
    DWORD bytesWritten;
    WriteFile(m_hPipeInWrite, cmd.c_str(), cmd.length(), &bytesWritten, NULL);
}

#else
// ==========================================
// LINUX / POSIX IMPLEMENTATION
// ==========================================
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include <fcntl.h>
#include <pty.h>     
#include <termios.h> 

AudioBackend::AudioBackend() : m_pid(-1), m_isPaused(false) {
    m_pipeOut[0] = -1; 
    m_pipeOut[1] = -1; 
}

AudioBackend::~AudioBackend() { stop(); }

bool AudioBackend::start() {
    if (isRunning()) return true;

    int master_fd, slave_fd;
    if (openpty(&master_fd, &slave_fd, nullptr, nullptr, nullptr) == -1) {
        std::cerr << "Failed to open PTY" << std::endl;
        return false;
    }

    m_pid = fork();

    if (m_pid == 0) {
        close(master_fd); 
        setsid();
        if (ioctl(slave_fd, TIOCSCTTY, nullptr) == -1) exit(1);

        dup2(slave_fd, STDIN_FILENO);
        dup2(slave_fd, STDOUT_FILENO);
        dup2(slave_fd, STDERR_FILENO);
        close(slave_fd);

        execlp("lowfi", "lowfi", "-m", nullptr);
        std::cerr << "Failed to execute lowfi. Is it installed?" << std::endl;
        exit(1); 
    } 
    else if (m_pid > 0) {
        close(slave_fd); 
        m_pipeOut[0] = master_fd; 

        int flags = fcntl(m_pipeOut[0], F_GETFL, 0);
        fcntl(m_pipeOut[0], F_SETFL, flags | O_NONBLOCK);

        m_isPaused = false;
        return true;
    }

    close(master_fd);
    close(slave_fd);
    return false; 
}

void AudioBackend::pause() {
    if (isRunning() && !m_isPaused) {
        kill(m_pid, SIGSTOP); 
        m_isPaused = true;
    }
}

void AudioBackend::resume() {
    if (isRunning() && m_isPaused) {
        kill(m_pid, SIGCONT); 
        m_isPaused = false;
    }
}

void AudioBackend::togglePlayPause() {
    if (!isRunning()) start();
    else if (m_isPaused) resume();
    else pause();
}

void AudioBackend::stop() {
    if (isRunning()) {
        kill(m_pid, SIGKILL);
        waitpid(m_pid, nullptr, 0); 
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
    
    int status;
    pid_t result = waitpid(m_pid, &status, WNOHANG);
    if (result == m_pid) {
        const_cast<AudioBackend*>(this)->m_pid = -1;
        return false;
    }
    return true;
}

bool AudioBackend::isPaused() const { return m_isPaused; }

std::string AudioBackend::pollOutput() {
    if (m_pipeOut[0] == -1) return "";

    char buffer[256];
    ssize_t bytesRead = read(m_pipeOut[0], buffer, sizeof(buffer) - 1);
    
    if (bytesRead > 0) {
        buffer[bytesRead] = '\0';
        std::string rawOutput(buffer);
        static const std::regex ansiRegex("\x1B\\[[0-9;?]*[a-zA-Z]");
        return std::regex_replace(rawOutput, ansiRegex, "");
    }
    return "";
}

void AudioBackend::sendCommand(const std::string& cmd) {
    if (m_pipeOut[0] == -1 || !isRunning()) return;
    auto ignore = write(m_pipeOut[0], cmd.c_str(), cmd.length());
    (void)ignore; // Suppress compiler warning about unused result
}
#endif