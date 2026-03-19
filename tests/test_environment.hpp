#pragma once

#ifdef _WIN32
// #include <windows.h>
#else
#include <grp.h>
#include <pwd.h>
#include <unistd.h>
#endif

#include <filesystem>
#include <fstream>
#include <vector>

#define REQUIRE_ROOT()                             \
  if (geteuid() != 0) {                            \
    DOCTEST_INFO("Test requires root privileges"); \
    return;                                        \
  }

namespace fs = std::filesystem;

#ifdef __linux__
inline gid_t get_primary_gid(uid_t uid) {
  auto* user_entry = getpwuid(uid);
  if (user_entry == nullptr) throw std::runtime_error("could not find passwd entry for user id " + std::to_string(uid));
  return user_entry->pw_gid;
}

inline gid_t get_secondary_gid(uid_t uid) {
  auto* user_entry = getpwuid(uid);

  if (user_entry == nullptr) throw std::runtime_error("could not find passwd entry for user id " + std::to_string(uid));

  gid_t primary_gid           = get_primary_gid(uid);
  const std::string user_name = user_entry->pw_name;

  int ngroups = 0;
  getgrouplist(user_name.c_str(), primary_gid, nullptr, &ngroups);
  std::vector<gid_t> groups(ngroups);
  getgrouplist(user_name.c_str(), primary_gid, groups.data(), &ngroups);

  gid_t secondary_gid = -1;
  for (gid_t group : groups) {
    if (group != primary_gid) {
      secondary_gid = group;
      break;
    }
  }

  return static_cast<gid_t>(secondary_gid);
}

inline std::string get_user_name(uid_t uid) {
  auto* user_entry = getpwuid(uid);
  if (user_entry == nullptr) throw std::runtime_error("could not find passwd entry for user id " + std::to_string(uid));
  return user_entry->pw_name;
}

inline std::string get_group_name(gid_t gid) {
  auto* group_entry = getgrgid(gid);
  if (group_entry == nullptr) throw std::runtime_error("could not find group entry for group id " + std::to_string(gid));
  return group_entry->gr_name;
}
#endif

struct TestEnvironment {
  const char* root_directory_name                  = "pathlib_test_environment";
  const char* directory_a_name                     = "dir_a";
  const char* directory_b_name                     = "dir_b";
  const char* nested_directory_name                = "nested_dir";
  const char* non_existant_file_name               = "non_existant_file.txt";
  const char* file_in_directory_a_name             = "file_a.txt";
  const char* file_in_directory_b_name             = "file_b.txt";
  const char* other_owner_file_name                = "other_owner_file.txt";
  const char* symlink_to_file_a_name               = "symlink_file_a.txt";
  const char* symlink_to_directory_b_name          = "symlink_directory_b";
  const char* symlink_to_other_owner_file_name     = "symlink_to_other_owner_file.txt";
  const char* home_directory_unix_variable_name    = "HOME";
  const char* home_directory_windows_variable_name = "USERPROFILE";

  const fs::path root_directory;
  const fs::path directory_a;
  const fs::path directory_b;
  const fs::path nested_directory;
  const fs::path tmp_directory;
  const fs::path non_existant_file;
  const fs::path file_in_directory_a;
  const fs::path file_in_directory_b;
  const fs::path file_a_relative_to_root;
  const fs::path file_b_relative_to_root;
  const fs::path other_owner_file;
  const fs::path symlink_to_file_a;
  const fs::path symlink_to_directory_b;
  const fs::path symlink_to_other_owner_file;
  const fs::path home_directory_unix;
  const fs::path home_directory_windows;

#ifdef __linux__
  uid_t user_uid;
  gid_t user_gid;
  gid_t user_secondary_gid;
  std::string user_name;
  std::string user_group_name;
  std::string user_secondary_group_name;

  uid_t root_uid = 0;
  gid_t root_gid;
  std::string root_name;
  std::string root_group_name;

  uid_t other_uid = 65534;  // user nobody
  gid_t other_gid;
  std::string other_name;
  std::string other_group_name;
#endif

  TestEnvironment()
      : root_directory(fs::current_path() / root_directory_name),
        directory_a(root_directory / directory_a_name),
        directory_b(root_directory / directory_b_name),
        nested_directory(directory_a / nested_directory_name),
        tmp_directory(fs::temp_directory_path()),
        non_existant_file(root_directory / non_existant_file_name),
        file_in_directory_a(directory_a / file_in_directory_a_name),
        file_in_directory_b(directory_b / file_in_directory_b_name),
        file_a_relative_to_root(fs::relative(file_in_directory_a, root_directory)),
        file_b_relative_to_root(fs::relative(file_in_directory_b, root_directory)),
        other_owner_file(nested_directory / other_owner_file_name),
        symlink_to_file_a(root_directory / symlink_to_file_a_name),
        symlink_to_directory_b(root_directory / symlink_to_directory_b_name),
        symlink_to_other_owner_file(root_directory / symlink_to_other_owner_file_name),
        home_directory_unix(std::string("$") + home_directory_unix_variable_name),
        home_directory_windows(std::string("%") + home_directory_windows_variable_name + "%") {
    // clean up state, in case previous cleanup failed
    cleanup();

#ifdef __linux__
    setup_ids_and_names();
#endif

    fs::create_directories(nested_directory);
    fs::create_directories(directory_b);
    std::ofstream(file_in_directory_a).close();
    std::ofstream(file_in_directory_b).close();
    std::ofstream(other_owner_file).close();

#ifdef __linux__
    fs::create_symlink(file_in_directory_a, symlink_to_file_a);
    fs::create_directory_symlink(directory_b, symlink_to_directory_b);
    fs::create_symlink(other_owner_file, symlink_to_other_owner_file);
#endif

#ifdef __linux__
    // change group of custom owner files
    chown(other_owner_file.c_str(), other_uid, other_gid);
    lchown(symlink_to_other_owner_file.c_str(), other_uid, other_gid);
#endif
  }

  ~TestEnvironment() { cleanup(); }

  TestEnvironment(const TestEnvironment&)            = delete;
  TestEnvironment& operator=(const TestEnvironment&) = delete;

#ifdef __linux__
  void setup_ids_and_names() {
    const char* ENV_SUDO_UID = getenv("SUDO_UID");

    user_uid                  = ENV_SUDO_UID ? atoi(ENV_SUDO_UID) : geteuid();
    user_gid                  = get_primary_gid(user_uid);
    user_secondary_gid        = get_secondary_gid(user_uid);
    user_name                 = get_user_name(user_uid);
    user_group_name           = get_group_name(user_gid);
    user_secondary_group_name = get_group_name(user_secondary_gid);

    root_gid        = get_primary_gid(root_uid);
    root_name       = get_user_name(root_uid);
    root_group_name = get_group_name(root_gid);

    other_gid        = get_primary_gid(other_uid);
    other_name       = get_user_name(other_uid);
    other_group_name = get_group_name(other_gid);
  }
#endif

  void cleanup() noexcept {
    std::error_code ec;
    if (fs::exists(root_directory, ec)) {
      fs::remove_all(root_directory, ec);
    }
  }
};
