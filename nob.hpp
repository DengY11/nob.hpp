#ifndef NOB_HPP
#define NOB_HPP

#include <string>
#include <vector>
#include <iostream>
#include <sstream>
#include <cstdlib> 
#include <mutex>
#include <atomic>
#include <chrono>
#include <iomanip>

#ifdef _WIN32
    #include <windows.h>
    #include <direct.h>
#else // POSIX
    #include <sys/stat.h>
    #include <sys/types.h>
    #include <dirent.h>
    #include <unistd.h>
    #include <errno.h>
    #include <sys/wait.h> 
    #include <signal.h>   
#endif

namespace NOB {

#ifdef _WIN32
    #include <process.h> // For _execv
#endif

// Thread-safe logging system with proper synchronization
class Logger {
private:
    static std::mutex log_mutex;
    
    static void format_log(std::ostream& out, const std::string& level, const std::string& msg) {
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()) % 1000;
        
        out << "[" << std::put_time(std::localtime(&time_t), "%H:%M:%S") 
            << "." << std::setfill('0') << std::setw(3) << ms.count() 
            << "][" << level << "] " << msg << std::endl;
    }
    
public:
    static void log(const std::string& level, const std::string& msg) {
        std::lock_guard<std::mutex> lock(log_mutex);
        std::ostream& out = (level == "INFO") ? std::cout : std::cerr;
        format_log(out, level, msg);
    }
};

// Static member definition
std::mutex Logger::log_mutex;

inline void INFO(const std::string& msg) {
    Logger::log("INFO", msg);
}

inline void ERROR(const std::string& msg) {
    Logger::log("ERROR", msg);
}

inline void WARN(const std::string& msg) {
    Logger::log("WARN", msg);
}


// Command argument representation
class CommandArg {
public:
    std::string value;
    
    CommandArg(const std::string& arg) : value(arg) {}
    CommandArg(const char* arg) : value(arg) {}
    
    // Parse command line string respecting quotes and escapes
    static std::vector<std::string> parse_command_line(const std::string& command_line) {
        std::vector<std::string> result;
        std::string current;
        bool in_single_quotes = false;
        bool in_double_quotes = false;
        bool escape_next = false;
        
        for (char c : command_line) {
            if (escape_next) {
                current += c;
                escape_next = false;
                continue;
            }
            
            if (c == '\\' && !in_single_quotes) {
                escape_next = true;
                continue;
            }
            
            if (c == '\'' && !in_double_quotes) {
                in_single_quotes = !in_single_quotes;
                continue;
            }
            
            if (c == '"' && !in_single_quotes) {
                in_double_quotes = !in_double_quotes;
                continue;
            }
            
            if (c == ' ' && !in_single_quotes && !in_double_quotes) {
                if (!current.empty()) {
                    result.push_back(current);
                    current.clear();
                }
                continue;
            }
            
            current += c;
        }
        
        if (!current.empty()) {
            result.push_back(current);
        }
        
        return result;
    }
};

using Command = std::vector<CommandArg>;

// Cross-platform path utilities
inline char get_path_separator() {
#ifdef _WIN32
    return '\\';
#else
    return '/';
#endif
}

inline std::string normalize_path(const std::string& path) {
    std::string normalized = path;
    char separator = get_path_separator();
    char other_separator = (separator == '/') ? '\\' : '/';
    
    std::replace(normalized.begin(), normalized.end(), other_separator, separator);
    
    std::string result;
    bool last_was_separator = false;
    for (char c : normalized) {
        if (c == separator) {
            if (!last_was_separator) {
                result += c;
                last_was_separator = true;
            }
        } else {
            result += c;
            last_was_separator = false;
        }
    }
    
    return result;
}

inline std::string get_relative_path(const std::string& full_path, const std::string& base_path) {
    std::string normalized_full = normalize_path(full_path);
    std::string normalized_base = normalize_path(base_path);
    
    if (normalized_full.find(normalized_base) == 0) {
        std::string rel_path = normalized_full.substr(normalized_base.length());
        if (!rel_path.empty() && (rel_path[0] == '/' || rel_path[0] == '\\')) {
            rel_path = rel_path.substr(1);
        }
        return rel_path;
    }
    return normalized_full;
}

inline std::string path_to_filename(const std::string& path) {
    std::string normalized = normalize_path(path);
    size_t pos = normalized.find_last_of("/\\");
    return (pos == std::string::npos) ? normalized : normalized.substr(pos + 1);
}

inline bool is_src_file(const std::string& file) {
    std::string filename = path_to_filename(file);
    size_t dot_pos = filename.rfind('.');
    if (dot_pos == std::string::npos) {
        return false;
    }
    std::string ext = filename.substr(dot_pos);
    return ext == ".c" || ext == ".cpp" || ext == ".cxx" || ext == ".cc";
}

inline bool exists(const std::string& path) {
#ifdef _WIN32
    struct _stat buffer;
    return (_stat(path.c_str(), &buffer) == 0);
#else
    struct stat buffer;
    return (stat(path.c_str(), &buffer) == 0);
#endif
}

inline bool is_dir(const std::string& path) {
#ifdef _WIN32
    struct _stat st;
    if (_stat(path.c_str(), &st) != 0) return false;
    return (st.st_mode & _S_IFDIR) != 0;
#else
    struct stat st;
    if (stat(path.c_str(), &st) != 0) return false;
    return S_ISDIR(st.st_mode);
#endif
}

inline bool is_updated(const std::string& src, const std::string& dst) {
#ifdef _WIN32
    struct _stat src_stat, dst_stat;
    if (_stat(src.c_str(), &src_stat) != 0) {
        return false; // Source does not exist
    }
    if (_stat(dst.c_str(), &dst_stat) != 0) {
        return true; // Destination does not exist
    }
    return src_stat.st_mtime > dst_stat.st_mtime;
#else
    struct stat src_stat, dst_stat;
    if (stat(src.c_str(), &src_stat) != 0) {
        return false; // Source does not exist
    }
    if (stat(dst.c_str(), &dst_stat) != 0) {
        return true; // Destination does not exist
    }
    return src_stat.st_mtime > dst_stat.st_mtime;
#endif
}

inline bool remove_file(const std::string& path) {
#ifdef _WIN32
    if (!DeleteFile(path.c_str())) {
        if (GetLastError() != ERROR_FILE_NOT_FOUND) {
            ERROR("DeleteFile failed for " + path + " with error " + std::to_string(GetLastError()));
            return false;
        }
    }
#else
    if (unlink(path.c_str()) != 0) {
        if (errno != ENOENT) { // Don't report error if file just doesn't exist
            perror("unlink");
            return false;
        }
    }
#endif
    return true;
}

inline bool is_header_file(const std::string& file) {
    size_t dot_pos = file.rfind('.');
    if (dot_pos == std::string::npos) {
        return false;
    }
    std::string ext = file.substr(dot_pos);
    return ext == ".h" || ext == ".hpp" || ext == ".hxx";
}

// Directory reading result with proper error handling
struct DirReadResult {
    std::vector<std::string> files;
    bool success;
    std::string error_message;
    
    DirReadResult() : success(true) {}
    DirReadResult(const std::string& error) : success(false), error_message(error) {}
};

inline DirReadResult read_entire_dir_result(const std::string& dir) {
    std::vector<std::string> files;
    
#ifdef _WIN32
    std::string search_path = dir + "/*";
    WIN32_FIND_DATA fd;
    HANDLE hFind = FindFirstFile(search_path.c_str(), &fd);
    
    if (hFind == INVALID_HANDLE_VALUE) {
        DWORD error = GetLastError();
        if (error == ERROR_FILE_NOT_FOUND) {
            return DirReadResult("Directory not found: " + dir);
        } else if (error == ERROR_ACCESS_DENIED) {
            return DirReadResult("Access denied: " + dir);
        } else {
            return DirReadResult("FindFirstFile failed for " + dir + " with error " + std::to_string(error));
        }
    }
    
    do {
        std::string name = fd.cFileName;
        if (name != "." && name != "..") {
            files.push_back(name);
        }
    } while (FindNextFile(hFind, &fd) != 0);
    
    FindClose(hFind);
#else
    DIR *d = opendir(dir.c_str());
    if (d == NULL) {
        if (errno == ENOENT) {
            return DirReadResult("Directory not found: " + dir);
        } else if (errno == EACCES) {
            return DirReadResult("Access denied: " + dir);
        } else {
            return DirReadResult("opendir failed for " + dir + ": " + strerror(errno));
        }
    }
    
    struct dirent *ent;
    while ((ent = readdir(d)) != NULL) {
        std::string name = ent->d_name;
        if (name != "." && name != "..") {
            files.push_back(name);
        }
    }
    
    closedir(d);
#endif
    
    DirReadResult result;
    result.files = files;
    return result;
}

// Legacy interface for backward compatibility
inline std::vector<std::string> read_entire_dir(const std::string& dir) {
    DirReadResult result = read_entire_dir_result(dir);
    if (!result.success) {
        ERROR(result.error_message);
    }
    return result.files;
}

inline std::vector<std::string> read_entire_dir_recursively(const std::string& dir) {
    std::vector<std::string> files = read_entire_dir(dir);
    for (auto& name : files) {
        std::string full_path = dir + "/" + name;
        if (is_dir(full_path)) {
            std::vector<std::string> sub_files = read_entire_dir_recursively(full_path);
            files.insert(files.end(), sub_files.begin(), sub_files.end());
        }
    }
    return files;
}

inline bool remove_dir_recursively(const std::string& dir) {
    if (!is_dir(dir)) {
        ERROR("Cannot remove recursively: " + dir + " is not a directory.");
        return false;
    }

    std::vector<std::string> files = read_entire_dir(dir);
    bool success = true;

    for(const auto& name : files) {
        std::string full_path = dir + "/" + name;
        if (is_dir(full_path)) {
            if (!remove_dir_recursively(full_path)) {
                success = false;
            }
        } else {
            if (!remove_file(full_path)) {
                success = false;
            }
        }
    }

    if (success) {
#ifdef _WIN32
        if (_rmdir(dir.c_str()) != 0) {
            perror("_rmdir");
            return false;
        }
#else
        if (rmdir(dir.c_str()) != 0) {
            perror("rmdir");
            return false;
        }
#endif
    }

    return success;
}

inline bool mkdir_if_not_exists(const std::string& dir) {
#ifdef _WIN32
    if (_mkdir(dir.c_str()) == 0) {
        NOB::INFO("Created directory: " + dir);
        return true;
    }
    if (errno == EEXIST) {
        return true; // Already exists
    }
    return false;
#else
    struct stat st = {0};
    if (stat(dir.c_str(), &st) == -1) {
        if (errno == ENOENT) {
            if (mkdir(dir.c_str(), 0755) != 0) {
                perror("mkdir");
                return false;
            }
            NOB::INFO("Created directory: " + dir);
            return true;
        }
        perror("stat");
        return false;
    }
    return S_ISDIR(st.st_mode);
#endif
}

// Execute multiple commands in parallel with proper error handling
inline bool run_commands_parallel(const std::vector<Command>& commands) {
#ifdef _WIN32
    std::vector<HANDLE> processes;
    std::vector<PROCESS_INFORMATION> process_infos;
    
    // Launch all processes
    for (const auto& cmd : commands) {
        STARTUPINFO si = {sizeof(si)};
        PROCESS_INFORMATION pi = {0};
        
        std::string cmd_line;
        for (size_t i = 0; i < cmd.size(); ++i) {
            cmd_line += cmd[i].value;
            if (i != cmd.size() - 1) cmd_line += " ";
        }
        
        if (CreateProcess(NULL, const_cast<char*>(cmd_line.c_str()), NULL, NULL, 
                         TRUE, 0, NULL, NULL, &si, &pi)) {
            processes.push_back(pi.hProcess);
            process_infos.push_back(pi);
        } else {
            ERROR("CreateProcess failed: " + std::to_string(GetLastError()));
            
            // Clean up already launched processes
            for (HANDLE h : processes) {
                CloseHandle(h);
            }
            for (const auto& p : process_infos) {
                CloseHandle(p.hThread);
            }
            return false;
        }
    }
    
    // Wait for all processes to complete
    bool all_success = true;
    for (HANDLE hProcess : processes) {
        if (WaitForSingleObject(hProcess, INFINITE) != WAIT_OBJECT_0) {
            all_success = false;
        }
        CloseHandle(hProcess);
    }
    
    for (const auto& pi : process_infos) {
        CloseHandle(pi.hThread);
    }
    
    return all_success;
#else
    std::vector<pid_t> child_pids;
    
    // Fork all child processes
    for (const auto& cmd : commands) {
        pid_t pid = fork();
        if (pid < 0) {
            perror("fork");
            
            // Kill already forked children
            for (pid_t child_pid : child_pids) {
                kill(child_pid, SIGTERM);
            }
            return false;
        }
        
        if (pid == 0) { // Child process
            // Build argv array
            std::vector<std::string> args;
            std::vector<char*> argv;
            for (const auto& arg : cmd) {
                args.push_back(arg.value);
                argv.push_back(const_cast<char*>(args.back().c_str()));
            }
            argv.push_back(nullptr);
            
            // Debug: print what we're trying to execute
            std::string debug_cmd;
            for (const auto& arg : args) {
                debug_cmd += arg + " ";
            }
            
            // Execute command using system() as fallback
            int result = system(debug_cmd.c_str());
            exit(result);
        }
        
        child_pids.push_back(pid);
    }
    
    // Wait for all children
    bool all_success = true;
    for (pid_t pid : child_pids) {
        int status;
        if (waitpid(pid, &status, 0) < 0) {
            perror("waitpid");
            all_success = false;
        } else if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
            ERROR("Child process " + std::to_string(pid) + " failed with exit code " + 
                  std::to_string(WIFEXITED(status) ? WEXITSTATUS(status) : -1));
            all_success = false;
        }
    }
    
    return all_success;
#endif
}

// Construct command from string
inline Command command_from_string(const std::string& command_line) {
    Command result;
    auto parts = CommandArg::parse_command_line(command_line);
    for (const auto& part : parts) {
        result.emplace_back(part);
    }
    return result;
}

inline bool run_command(const Command& cmd) {
    if (cmd.empty()) {
        return true;
    }
    
    std::stringstream ss;
    for (size_t i = 0; i < cmd.size(); ++i) {
        ss << cmd[i].value << (i == cmd.size() - 1 ? "" : " ");
    }
    
    std::string final_cmd = ss.str();
    INFO("CMD: " + final_cmd);
    
    int result = system(final_cmd.c_str());
    if (result != 0) {
        ERROR("Command failed with exit code " + std::to_string(result));
        return false;
    }
    return true;
}

#define REBUILD_YOURSELF(argc, argv) \
    do { \
        if (!NOB::rebuild_yourself_if_needed(argc, argv, __FILE__)) { \
            return 1; \
        } \
    } while(0)

inline bool rebuild_yourself_if_needed(int argc, char** argv, const char* self_src_path) {
    const char* self_exe_path = argv[0];
    if (is_updated(self_src_path, self_exe_path) || is_updated("nob.hpp", self_exe_path)) {
        INFO("Build script has been updated. Rebuilding self...");
        
        Command rebuild_cmd;
        rebuild_cmd.emplace_back("g++");
        rebuild_cmd.emplace_back("-o");
        rebuild_cmd.emplace_back(self_exe_path);
        rebuild_cmd.emplace_back(self_src_path);
        
        if (!run_command(rebuild_cmd)) {
            ERROR("Failed to rebuild self. Aborting.");
            return false;
        }
        
        INFO("Re-spawning with new build script...");
        std::vector<char*> new_argv;
        for (int i = 0; i < argc; ++i) {
            new_argv.push_back(argv[i]);
        }
        new_argv.push_back(nullptr);
        
#ifdef _WIN32
        _execv(self_exe_path, new_argv.data());
#else
        execv(self_exe_path, new_argv.data());
#endif
        perror("execv");
        return false;
    }
    return true;
}

} // namespace NOB




#endif // NOB_HPP
