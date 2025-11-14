#include "../nob.hpp"
#include <vector>
#include <string>
#include <algorithm>

void build_main(const std::string& build_dir, const std::string& src_dir);
void clean_build(const std::string& build_dir);
void find_src_files_recursively(const std::string& base_dir, const std::string& current_dir, std::vector<std::string>& src_files);

int main(int argc, char* argv[])
{
    REBUILD_YOURSELF(argc, argv);

    const std::string build_dir = "./build";
    const std::string src_dir = "./src";

    std::string command = "build"; // Default command
    if (argc > 1) {
        command = argv[1];
    }

    if (command == "build") {
        build_main(build_dir, src_dir);
    } else if (command == "clean") {
        clean_build(build_dir);
    }
    return 0;
}

void clean_build(const std::string& build_dir) {
    NOB::INFO("Cleaning build directory...");
    if (NOB::exists(build_dir)) {
        if (NOB::remove_dir_recursively(build_dir)) {
            NOB::INFO("Build directory cleaned.");
        } else {
            NOB::ERROR("Failed to clean build directory.");
        }
    } else {
        NOB::WARN("Build directory does not exist, nothing to clean.");
    }
}

void find_src_files_recursively(const std::string& base_dir, const std::string& current_dir, std::vector<std::string>& src_files) {
    auto result = NOB::read_entire_dir_result(current_dir);
    if (!result.success) {
        NOB::ERROR("Failed to read directory " + current_dir + ": " + result.error_message);
        return;
    }
    
    for (const auto& entry : result.files) {
        std::string path = current_dir + "/" + entry;
        if (NOB::is_dir(path)) {
            find_src_files_recursively(base_dir, path, src_files);
        } else if (NOB::is_src_file(entry)) {
            src_files.push_back(path);
        }
    }
}

void build_main(const std::string& build_dir, const std::string& src_dir) {
    if (!NOB::mkdir_if_not_exists(build_dir)) {
        return;
    }
     if (!NOB::mkdir_if_not_exists(build_dir + "/obj")) {
        return;
    }

    std::vector<std::string> src_files;
    find_src_files_recursively(src_dir, src_dir, src_files);

    if (src_files.empty()) {
        NOB::ERROR("No source files found in " + src_dir);
        return;
    }

    std::vector<NOB::Command> compile_cmds;
    std::vector<std::string> lib_obj_paths;
    std::string main_obj_path;

    for(const auto& src_path: src_files){
        // Use cross-platform path handling
        std::string rel_path = NOB::get_relative_path(src_path, src_dir);
        std::replace(rel_path.begin(), rel_path.end(), '/', '_');
        std::replace(rel_path.begin(), rel_path.end(), '\\', '_');
        size_t dot_pos = rel_path.rfind('.');
        std::string obj_name = rel_path.substr(0, dot_pos) + ".o";
        std::string obj_path = build_dir + "/obj/" + obj_name;

        if (src_path.find("main.cpp") == std::string::npos) {
            lib_obj_paths.push_back(obj_path);
        } else {
            main_obj_path = obj_path;
        }

        if (NOB::is_updated(src_path, obj_path)) {
            NOB::Command cmd;
            cmd.emplace_back("g++");
            cmd.emplace_back("-c");
            cmd.emplace_back("-I" + src_dir);
            cmd.emplace_back("-o");
            cmd.emplace_back(obj_path);
            cmd.emplace_back(src_path);
            compile_cmds.push_back(cmd);
        }
    }

    if (!compile_cmds.empty()) {
        NOB::INFO("Compiling...");
        if (!NOB::run_commands_parallel(compile_cmds)) {
            NOB::ERROR("Compilation failed.");
            return;
        }
    } else {
        NOB::INFO("All object files are up to date.");
    }

    const std::string lib_path = build_dir + "/libnob_example.a";
    NOB::INFO("Archiving static library...");
    NOB::Command archive_cmd;
    archive_cmd.emplace_back("ar");
    archive_cmd.emplace_back("rcs");
    archive_cmd.emplace_back(lib_path);
    for (const auto& path : lib_obj_paths) {
        archive_cmd.emplace_back(path);
    }
     if (!NOB::run_command(archive_cmd)) {
        NOB::ERROR("Archiving failed.");
        return;
    }


    NOB::INFO("Linking...");
    NOB::Command link_cmd;
    link_cmd.emplace_back("g++");
    link_cmd.emplace_back("-o");
    link_cmd.emplace_back(build_dir + "/main");
    link_cmd.emplace_back(main_obj_path);
    link_cmd.emplace_back(lib_path);


    if (!NOB::run_command(link_cmd)) {
        NOB::ERROR("Linking failed.");
        return;
    }

    NOB::INFO("Build finished successfully.");
}