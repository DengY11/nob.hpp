# nob.hpp - A C++ Header-Only Build Library

Inspired by the philosophy of [nob.h](https://github.com/tsoding/nob.h), `nob.hpp` is a simple, self-contained, header-only library for building C++ projects using C++ itself. 

Forget Makefiles, forget CMake. If you know C++, you already know how to use `nob.hpp`.

## Core Philosophy

- **No Dependencies**: Besides a C++11 compliant compiler, you don't need anything else.
- **Header-Only**: Just drop `nob.hpp` into your project and you're ready to go.
- **Use C++ to build C++**: Write your build scripts in the same language as your project. No context switching, no learning new syntax.
- **Self-Rebuilding**: The build script can automatically rebuild itself if it or `nob.hpp` is updated.

## Quick Start

Here is a simple example of a build script (`nob.cpp`) that compiles all `.cpp` files from a `src` directory into a single executable.

```cpp
// nob.cpp
#include "nob.hpp"

int main(int argc, char* argv[])
{
    REBUILD_YOURSELF(argc, argv);

    const std::string build_dir = "./build";
    const std::string src_dir = "./src";

    if (!NOB::mkdir_if_not_exists(build_dir)) return 1;

    std::vector<NOB::Cmds> compile_cmds;
    std::vector<std::string> obj_paths;

    auto src_files = NOB::read_entire_dir(src_dir);
    for(const auto& file: src_files){
        if(NOB::is_src_file(file)){
            std::string src_path = src_dir + "/" + file;
            std::string obj_path = build_dir + "/" + file + ".o";
            obj_paths.push_back(obj_path);

            if (NOB::is_updated(src_path, obj_path)) {
                NOB::Cmds cmd;
                cmd.emplace_back("g++");
                cmd.emplace_back("-c");
                cmd.emplace_back("-o");
                cmd.emplace_back(obj_path);
                cmd.emplace_back(src_path);
                compile_cmds.push_back(cmd);
            }
        }
    }

    if (!compile_cmds.empty()) {
        NOB::INFO("Compiling...");
        if (!NOB::run_cmds_parallel(compile_cmds)) return 1;
    }

    NOB::INFO("Linking...");
    NOB::Cmds link_cmd;
    link_cmd.emplace_back("g++");
    link_cmd.emplace_back("-o");
    link_cmd.emplace_back(build_dir + "/main");
    for (const auto& path : obj_paths) {
        link_cmd.emplace_back(path);
    }

    if (!NOB::run_cmds(link_cmd)) return 1;

    NOB::INFO("Build finished successfully.");
    return 0;
}
```

**How to run:**

1.  **Bootstrap the build script:**
    ```bash
    g++ -o nob nob.cpp
    ```

2.  **Run the build:**
    ```bash
    ./nob
    ```

That's it! If you modify `nob.cpp` or `nob.hpp`, the `nob` executable will automatically rebuild itself on the next run.

## API Reference

All functions are available under the `NOB` namespace.

### Logging

- `void INFO(const std::string& msg)`: Prints an informational message.
- `void WARN(const std::string& msg)`: Prints a warning message.
- `void ERROR(const std::string& msg)`: Prints an error message.

### Command Execution

- `class Cmd`: Represents a single part of a command.
- `using Cmds = std::vector<Cmd>`: Represents a full command line.
- `bool run_cmds(const Cmds& cmds)`: Executes a command serially.
- `bool run_cmds_parallel(const std::vector<Cmds>& cmd_groups)`: Executes multiple command groups in parallel (on POSIX systems).

### File System

- `bool exists(const std::string& path)`: Checks if a file or directory exists.
- `bool is_dir(const std::string& path)`: Checks if a path is a directory.
- `bool mkdir_if_not_exists(const std::string& dir)`: Creates a directory if it does not exist.
- `std::vector<std::string> read_entire_dir(const std::string& dir)`: Reads all entry names from a directory.
- `std::vector<std::string> read_entire_dir_recursively(const std::string& dir)`: Reads all entry names from a directory recursively.
- `bool remove_file(const std::string& path)`: Removes a file.
- `bool remove_dir_recursively(const std::string& dir)`: Recursively removes a directory and its contents.

### Build Helpers

- `bool is_src_file(const std::string& file)`: Returns true if the file has a `.c`, `.cpp`, or `.cxx` extension.
- `bool is_header_file(const std::string& file)`: Returns true if the file has a `.h`, `.hpp`, or `.hxx` extension.
- `bool is_updated(const std::string& src, const std::string& dst)`: Checks if the source file is newer than the destination file.

### Self-Rebuilding

- `REBUILD_YOURSELF(argc, argv)`: A macro that, when placed at the start of `main`, enables the build script to automatically update itself.
