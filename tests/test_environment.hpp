#pragma once

#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

struct TestEnvironment {
  const char* root_directory_name         = "pathlib_test_environment";
  const char* directory_a_name            = "dir_a";
  const char* directory_b_name            = "dir_b";
  const char* nested_directory_name       = "nested_dir";
  const char* non_existant_file_name      = "non_existant_file.txt";
  const char* file_in_directory_a_name    = "file_a.txt";
  const char* file_in_directory_b_name    = "file_b.txt";
  const char* symlink_to_file_a_name      = "symlink_file_a.txt";
  const char* symlink_to_directory_b_name = "symlink_directory_b";

  const fs::path root_directory;
  const fs::path directory_a;
  const fs::path directory_b;
  const fs::path nested_directory;
  const fs::path non_existant_file;
  const fs::path file_in_directory_a;
  const fs::path file_in_directory_b;
  const fs::path file_a_relative_to_root;
  const fs::path file_b_relative_to_root;
  const fs::path symlink_to_file_a;
  const fs::path symlink_to_directory_b;

  TestEnvironment()
      : root_directory(fs::current_path() / root_directory_name),
        directory_a(root_directory / directory_a_name),
        directory_b(root_directory / directory_b_name),
        nested_directory(directory_a / nested_directory_name),
        non_existant_file(root_directory / non_existant_file_name),
        file_in_directory_a(directory_a / file_in_directory_a_name),
        file_in_directory_b(directory_b / file_in_directory_b_name),
        file_a_relative_to_root(fs::relative(file_in_directory_a, root_directory)),
        file_b_relative_to_root(fs::relative(file_in_directory_b, root_directory)),
        symlink_to_file_a(root_directory / symlink_to_file_a_name),
        symlink_to_directory_b(root_directory / symlink_to_directory_b_name) {
    // clean up state, in case previous cleanup failed
    cleanup();

    fs::create_directories(nested_directory);
    fs::create_directories(directory_b);
    std::ofstream(file_in_directory_a).close();
    std::ofstream(file_in_directory_b).close();
    fs::create_symlink(file_in_directory_a, symlink_to_file_a);
    fs::create_directory_symlink(directory_b, symlink_to_directory_b);
  }

  ~TestEnvironment() { cleanup(); }

  TestEnvironment(const TestEnvironment&)            = delete;
  TestEnvironment& operator=(const TestEnvironment&) = delete;

  void cleanup() noexcept {
    std::error_code ec;
    if (fs::exists(root_directory, ec)) {
      fs::remove_all(root_directory, ec);
    }
  }
};
