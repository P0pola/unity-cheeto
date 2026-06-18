#include "pch.h"
#include "logger.h"

void Logger_::writeLog(const char* file, int line, LogLevel level, const std::string& message) {

    if (m_excludedLevels.find(level) != m_excludedLevels.end())
        return;

    const std::lock_guard<std::mutex> lock(m_logMutex);

    std::string formattedMessage = formatLogMessage(file, line, level, message);

    // Write to console if enabled
    if (static_cast<int>(m_output) & static_cast<int>(LogOutput::Console)) {
        writeToConsole(formattedMessage, level);
    }

    // Write to file if enabled
    if (static_cast<int>(m_output) & static_cast<int>(LogOutput::File)) {
        writeToFile(formattedMessage);
    }

    // Write to DebugView if enabled
    if (static_cast<int>(m_output) & static_cast<int>(LogOutput::DebugView)) {
        writeToDebugView(formattedMessage);
    }
}

void Logger_::attachConsole() {
#ifdef _WIN32
    if (!m_consoleAttached) {
        AllocConsole();

        m_hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
        SetConsoleOutputCP(CP_UTF8);

        DWORD dwMode = 0;
        if (GetConsoleMode(m_hConsole, &dwMode)) {
            dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
            SetConsoleMode(m_hConsole, dwMode);
        }

        // stdin for readKey only
        freopen_s((FILE**)stdin, "CONIN$", "r", stdin);
        // redirect stdout/stderr to NUL — suppress game/UE engine output
        freopen_s((FILE**)stdout, "NUL", "w", stdout);
        freopen_s((FILE**)stderr, "NUL", "w", stderr);

        m_consoleAttached = true;
    }
#endif
}

void Logger_::detachConsole() {
#ifdef _WIN32
    if (m_consoleAttached) {
        m_hConsole = INVALID_HANDLE_VALUE;
        FreeConsole();
        m_consoleAttached = false;
    }
#endif
}

void Logger_::clearConsole() {
#ifdef _WIN32
    if (m_hConsole == INVALID_HANDLE_VALUE)
        return;

    CONSOLE_SCREEN_BUFFER_INFO csbi;
    if (!GetConsoleScreenBufferInfo(m_hConsole, &csbi))
        return;

    DWORD size  = csbi.dwSize.X * csbi.dwSize.Y;
    COORD coord = {0, 0};
    DWORD written;

    FillConsoleOutputCharacter(m_hConsole, TEXT(' '), size, coord, &written);
    GetConsoleScreenBufferInfo(m_hConsole, &csbi);
    FillConsoleOutputAttribute(m_hConsole, csbi.wAttributes, size, coord, &written);
    SetConsoleCursorPosition(m_hConsole, coord);
#endif
}

char Logger_::consoleReadKey() {
    return std::cin.get();
}

bool Logger_::prepareFileLogging(const std::string& directory) {
    try {
        // Create directory if it doesn't exist
        if (!std::filesystem::exists(directory)) {
            if (!std::filesystem::create_directories(directory)) {
                return false;
            }
        }

        // Generate filename with timestamp
        auto now    = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);

        struct tm tm_buf;
#ifdef _WIN32
        gmtime_s(&tm_buf, &time_t);
#else
        gmtime_r(&time_t, &tm_buf);
#endif

        // Create filename with timestamp using simple string concatenation
        std::string filename = "log_" + std::to_string(1900 + tm_buf.tm_year) + "-" +
                               (tm_buf.tm_mon + 1 < 10 ? "0" : "") + std::to_string(tm_buf.tm_mon + 1) + "-" +
                               (tm_buf.tm_mday < 10 ? "0" : "") + std::to_string(tm_buf.tm_mday) + "_" +
                               (tm_buf.tm_hour < 10 ? "0" : "") + std::to_string(tm_buf.tm_hour) + "-" +
                               (tm_buf.tm_min < 10 ? "0" : "") + std::to_string(tm_buf.tm_min) + "-" +
                               (tm_buf.tm_sec < 10 ? "0" : "") + std::to_string(tm_buf.tm_sec) + ".txt";

        m_logFilePath = directory + "/" + filename;

        // Close existing file if open
        if (m_logFile && m_logFile->is_open()) {
            m_logFile->close();
        }

        // Open new log file
        m_logFile = std::make_unique<std::ofstream>(m_logFilePath, std::ios::out | std::ios::app);
        if (!m_logFile->is_open()) {
            m_logFile.reset();
            return false;
        }

        return true;
    } catch (const std::exception&) {
        return false;
    }
}

void Logger_::closeFileLogging() {
    if (m_logFile && m_logFile->is_open()) {
        m_logFile->close();
        m_logFile.reset();
    }
}

void Logger_::writeToConsole(const std::string& formattedMessage, LogLevel level) {
    if (m_hConsole == INVALID_HANDLE_VALUE) return;

    auto write = [this](const std::string& s) {
        DWORD written;
        WriteConsoleA(m_hConsole, s.c_str(), (DWORD)s.size(), &written, nullptr);
    };

    if (m_enableColors) {
        const char* colorCode;
        switch (level) {
            case LogLevel::Debug:   colorCode = "\033[35m"; break; // magenta
            case LogLevel::Info:    colorCode = "\033[32m"; break; // green
            case LogLevel::Warning: colorCode = "\033[33m"; break; // yellow
            case LogLevel::Error:   colorCode = "\033[31m"; break; // red
            default:                colorCode = "\033[37m"; break; // white
        }

        std::string levelStr = "[" + getLevelString(level) + "]";
        size_t levelPos = formattedMessage.find(levelStr);

        if (levelPos != std::string::npos) {
            write(formattedMessage.substr(0, levelPos));
            write(colorCode);
            write(levelStr);
            write("\033[0m");
            write(formattedMessage.substr(levelPos + levelStr.size()));
        } else {
            write(colorCode);
            write(formattedMessage);
            write("\033[0m");
        }
        write("\n");
    } else {
        write(formattedMessage + "\n");
    }
}

void Logger_::writeToFile(const std::string& formattedMessage) {
    if (m_logFile && m_logFile->is_open()) {
        *m_logFile << formattedMessage << std::endl;
        m_logFile->flush();
    }
}

void Logger_::writeToDebugView(const std::string& formattedMessage) {
#ifdef _WIN32
    OutputDebugStringA((formattedMessage + "\n").c_str());
#endif
}

std::string Logger_::formatLogMessage(const char* file, int line, LogLevel level, const std::string& message) {
    std::string result;

    // Add timestamp if enabled
    if (m_showTimeStamp) {
        result += "[" + getCurrentTimeString() + "] ";
    }

    // Add file info if enabled
    if (m_showFileName && file && std::strlen(file) > 0) {
        std::string filename = std::filesystem::path(file).filename().string();
        result += "[" + filename;

        if (m_showLineNumber && line > 0) {
            result += ":" + std::to_string(line);
        }

        result += "] ";
    }

    // Add level
    result += "[" + getLevelString(level) + "] ";

    // Add message
    result += message;

    return result;
}

std::string Logger_::getLevelString(LogLevel level) {
    switch (level) {
        case LogLevel::Debug:
            return "DEBUG";
        case LogLevel::Info:
            return "INFO";
        case LogLevel::Warning:
            return "WARN";
        case LogLevel::Error:
            return "ERROR";
        default:
            return "LOG";
    }
}

std::string Logger_::getCurrentTimeString() {
    auto now    = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);

    struct tm tm_buf;
#ifdef _WIN32
    localtime_s(&tm_buf, &time_t);
#else
    localtime_r(&time_t, &tm_buf);
#endif

    // Format time using simple string concatenation
    std::string timeStr = (tm_buf.tm_hour < 10 ? "0" : "") + std::to_string(tm_buf.tm_hour) + ":" +
                          (tm_buf.tm_min < 10 ? "0" : "") + std::to_string(tm_buf.tm_min) + ":" +
                          (tm_buf.tm_sec < 10 ? "0" : "") + std::to_string(tm_buf.tm_sec);

    return timeStr;
}

#ifdef _WIN32
WORD Logger_::getLevelColor(LogLevel level) {
    switch (level) {
        case LogLevel::Debug:
            return FOREGROUND_BLUE | FOREGROUND_RED | FOREGROUND_INTENSITY;  // Magenta
        case LogLevel::Info:
            return FOREGROUND_GREEN | FOREGROUND_INTENSITY;  // Green
        case LogLevel::Warning:
            return FOREGROUND_GREEN | FOREGROUND_RED | FOREGROUND_INTENSITY;  // Yellow
        case LogLevel::Error:
            return FOREGROUND_RED | FOREGROUND_INTENSITY;  // Red
        default:
            return FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY;  // White
    }
}
#endif