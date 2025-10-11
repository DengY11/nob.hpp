#ifndef NOB_HPP
#define NOB_HPP

#include <string>
#include <vector>
#include <iostream>
#include <sstream>
#include <cstdlib> // For system()
#include <mutex>

// Platform-specific includes for system calls
#ifdef _WIN32
    #include <windows.h>
    #include <direct.h>
#else // POSIX
    #include <sys/stat.h>
    #include <sys/types.h>
    #include <dirent.h>
    #include <unistd.h>
    #include <errno.h>
    #include <sys/wait.h> // For waitpid
#endif

namespace NOB {

#ifdef _WIN32
    #include <process.h> // For _execv
#endif

static std::mutex log_mutex;

inline void INFO(const std::string& msg) {
    std::lock_guard<std::mutex> guard(log_mutex);
    std::cout << "[INFO] " << msg << std::endl;
}

inline void ERROR(const std::string& msg) {
    std::lock_guard<std::mutex> guard(log_mutex);
    std::cerr << "[ERROR] " << msg << std::endl;
}

inline void WARN(const std::string& msg) {
    std::lock_guard<std::mutex> guard(log_mutex);
    std::cerr << "[WARN] " << msg << std::endl;
}


class Cmd;
using Cmds = std::vector<Cmd>;

// Represents a single part of a command line (e.g., "g++", "-o", "main")
class Cmd {
public:
    std::string part;
    Cmd(const std::string& cmd_part) : part(cmd_part) {}
    int run();
};

inline bool is_src_file(const std::string& file) {
    size_t dot_pos = file.rfind('.');
    if (dot_pos == std::string::npos) {
        return false;
    }
    std::string ext = file.substr(dot_pos);
    return ext == ".c" || ext == ".cpp" || ext == ".cxx";
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

inline std::vector<std::string> read_entire_dir(const std::string& dir) {
    std::vector<std::string> files;
#ifdef _WIN32
    std::string search_path = dir + "/*";
    WIN32_FIND_DATA fd;
    HANDLE hFind = FindFirstFile(search_path.c_str(), &fd);
    if (hFind == INVALID_HANDLE_VALUE) {
        // Not using ERROR() here to match the POSIX version's use of perror
        std::cerr << "[ERROR] FindFirstFile failed for " << dir << " with error " << GetLastError() << std::endl;
        return files;
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
        perror("opendir");
        return files;
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
    return files;
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

inline bool run_cmds_parallel(const std::vector<Cmds>& cmd_groups) {
#ifdef _WIN32
    // TODO: Windows implementation using CreateProcess and WaitForMultipleObjects
    ERROR("Parallel builds on Windows are not implemented yet.");
    return false;
#else
    std::vector<pid_t> children;

    for (const auto& cmds : cmd_groups) {
        pid_t pid = fork();
        if (pid < 0) {
            perror("fork");
            return false;
        }

        if (pid == 0) { // Child process
            std::vector<const char*> argv;
            for (const auto& cmd : cmds) {
                argv.push_back(cmd.part.c_str());
            }
            argv.push_back(NULL);

            execvp(argv[0], const_cast<char* const*>(argv.data()));
            // If execvp returns, it must have failed.
            perror("execvp");
            exit(1);
        }

        children.push_back(pid);
    }

    for (pid_t pid : children) {
        int status;
        if (waitpid(pid, &status, 0) < 0) {
            perror("waitpid");
            return false;
        }
        if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
            // A child process failed. No need to print a message here, 
            // as the child's execvp error or the command's own error output should be visible.
            return false;
        }
    }

    return true;
#endif
}

inline bool run_cmds(const Cmds& cmds) {
    if (cmds.empty()) {
        return true;
    }
    std::stringstream ss;
    for (size_t i = 0; i < cmds.size(); ++i) {
        ss << cmds[i].part << (i == cmds.size() - 1 ? "" : " ");
    }
    std::string final_cmd = ss.str();
    NOB::INFO("CMD: " + final_cmd);
    int result = system(final_cmd.c_str());
    if (result != 0) {
        NOB::ERROR("Command failed with exit code " + std::to_string(result));
        return false;
    }
    return true;
}

// The single-command run is less useful in this design, but implemented for completeness.
inline int Cmd::run() {
    return system(this->part.c_str());
}

#define REBUILD_YOURSELF(argc, argv) NOB::rebuild_yourself_if_needed(argc, argv, __FILE__)

inline void rebuild_yourself_if_needed(int argc, char** argv, const char* self_src_path) {
    const char* self_exe_path = argv[0];
    if (NOB::is_updated(self_src_path, self_exe_path) || NOB::is_updated("nob.hpp", self_exe_path)) {
        NOB::INFO("Build script has been updated. Rebuilding self...");
        NOB::Cmds rebuild_cmd;
        rebuild_cmd.emplace_back("g++");
        rebuild_cmd.emplace_back("-o");
        rebuild_cmd.emplace_back(self_exe_path);
        rebuild_cmd.emplace_back(self_src_path);
        if (!NOB::run_cmds(rebuild_cmd)) {
            NOB::ERROR("Failed to rebuild self. Aborting.");
            exit(1);
        }
        NOB::INFO("Re-spawning with new build script...");
        std::vector<char*> new_argv;
        for (int i = 0; i < argc; ++i) {
            new_argv.push_back(argv[i]);
        }
        new_argv.push_back(NULL);
#ifdef _WIN32
        _execv(self_exe_path, new_argv.data());
#else
        execv(self_exe_path, new_argv.data());
#endif
        perror("execv");
        exit(1);
    }
}

} // namespace NOB




#endif // NOB_HPP