#include <doctest/doctest.h>

#include <PathLib/PathLib.hpp>
#include <filesystem>

#include "test_environment.hpp"

namespace fs = std::filesystem;

TEST_CASE("absolute() from relative path") {
  TestEnvironment environment;
  PathLib::Path path(environment.file_a_relative_to_root);

  path.absolute();

  REQUIRE(path.path().is_absolute());
  REQUIRE(path.path() == fs::absolute(environment.file_a_relative_to_root));
  REQUIRE(path.good());
}

TEST_CASE("absolute() from absolute path") {
  TestEnvironment environment;
  PathLib::Path path(environment.file_in_directory_a);

  path.absolute();

  REQUIRE(path.path().is_absolute());
  REQUIRE(path.path() == fs::absolute(environment.file_in_directory_a));
  REQUIRE(path.good());
}

TEST_CASE("absolute() non-destructive") {
  TestEnvironment environment;
  PathLib::Path original_relative_path(environment.file_a_relative_to_root);
  PathLib::Path original_absolute_path(environment.file_in_directory_a);

  PathLib::Path result_from_relative_path = original_relative_path.absolute_copy();
  PathLib::Path result_from_absolute_path = original_absolute_path.absolute_copy();

  REQUIRE(original_relative_path.path() == environment.file_a_relative_to_root);
  REQUIRE(original_absolute_path.path() == environment.file_in_directory_a);
  REQUIRE(result_from_relative_path.path().is_absolute());
  REQUIRE(result_from_absolute_path.path().is_absolute());

  REQUIRE(original_relative_path.good());
  REQUIRE(original_absolute_path.good());
  REQUIRE(result_from_relative_path.good());
  REQUIRE(result_from_absolute_path.good());
}

#ifdef __linux__
TEST_CASE("absolute() symlink with and without resolve") {
  TestEnvironment environment;
  PathLib::Path path(environment.symlink_to_file_a);

  PathLib::Path unresolved_path = path.absolute_copy(false);
  PathLib::Path resolved_path   = path.absolute_copy(true);

  REQUIRE(unresolved_path.path() == environment.symlink_to_file_a);
  REQUIRE(resolved_path.path() == fs::absolute(environment.file_in_directory_a));

  REQUIRE(unresolved_path.good());
  REQUIRE(resolved_path.good());
}
#endif

TEST_CASE("relative() from absolute path") {
  TestEnvironment environment;
  PathLib::Path path(environment.file_in_directory_a);

  path.relative();

  REQUIRE(path.path().is_relative());
  REQUIRE(path.path() == fs::relative(environment.file_in_directory_a));
  REQUIRE(path.good());
}

TEST_CASE("relative() with absolute base") {
  TestEnvironment environment;
  const PathLib::Path directory_a_path(environment.directory_a);
  PathLib::Path path(environment.file_in_directory_a);

  path.relative(directory_a_path);
  REQUIRE(path.path().is_relative());
  REQUIRE(path.path() == environment.file_in_directory_a_name);
  REQUIRE(path.good());
}

TEST_CASE("relative() from relative path with absolute base returns empty path") {
  TestEnvironment environment;
  const PathLib::Path directory_a_path(environment.directory_a);
  PathLib::Path path(environment.file_a_relative_to_root);

  path.relative(directory_a_path);
  // path is relative, base_path is absolute
  // relative() with resolve_symlinks == false uses fs_lexically_relative()
  // lexically_relative() returns empty path when the paths are mixed between relative and absolute
  REQUIRE(path.path() == fs::path());
  REQUIRE(path.good());
}

TEST_CASE("relative() from relative path with relative base") {
  TestEnvironment environment;
  PathLib::Path path(environment.file_a_relative_to_root);
  const PathLib::Path root_directory_path(environment.root_directory);
  PathLib::Path directory_a_relative_to_root(fs::relative(environment.directory_a, environment.root_directory));

  path.relative(directory_a_relative_to_root);
  REQUIRE(path.path().is_relative());
  REQUIRE(path.path() == environment.file_in_directory_a_name);

  REQUIRE(root_directory_path.good());
  REQUIRE(path.good());
  REQUIRE(directory_a_relative_to_root.good());
}

TEST_CASE("relative() non-destructive") {
  TestEnvironment environment;
  PathLib::Path original_path(environment.file_in_directory_a);

  PathLib::Path result_path = original_path.relative_copy();

  REQUIRE(original_path.path() == environment.file_in_directory_a);
  REQUIRE(result_path.path().is_relative());

  REQUIRE(original_path.good());
  REQUIRE(result_path.good());
}

#ifdef __linux__
TEST_CASE("relative() symlink with and without resolve") {
  TestEnvironment environment;
  const PathLib::Path directory_a_path(environment.directory_a);
  PathLib::Path path(environment.symlink_to_file_a);

  PathLib::Path unresolved_path = path.relative_copy(directory_a_path, false);
  PathLib::Path resolved_path   = path.relative_copy(directory_a_path, true);

  REQUIRE(unresolved_path.path() == fs::path("..") / environment.symlink_to_file_a_name);
  REQUIRE(resolved_path.path() == fs::path(environment.file_in_directory_a_name));

  REQUIRE(unresolved_path.good());
  REQUIRE(resolved_path.good());
}
#endif

TEST_CASE("normalize() clean and messy path") {
  TestEnvironment environment;
  fs::path clean = environment.file_in_directory_a;
  fs::path messy = environment.directory_a / ".." / environment.directory_a_name / "." / environment.file_in_directory_a_name;
  PathLib::Path clean_path(clean);
  PathLib::Path messy_path(messy);

  clean_path.normalize();
  messy_path.normalize();

  REQUIRE(clean_path.path() == clean.lexically_normal());
  REQUIRE(messy_path.path() == messy.lexically_normal());
  REQUIRE(messy_path.path() == clean_path.path());

  REQUIRE(clean_path.good());
  REQUIRE(messy_path.good());
}

TEST_CASE("normalize() non-destructive") {
  TestEnvironment environment;
  fs::path messy = environment.directory_a / ".." / environment.directory_a_name / "." / environment.file_in_directory_a_name;
  PathLib::Path original_path(messy);

  PathLib::Path result_path = original_path.normalized_copy();

  REQUIRE(original_path.path() == messy);
  REQUIRE(result_path.path() == messy.lexically_normal());

  REQUIRE(original_path.good());
  REQUIRE(result_path.good());
}

#ifdef __linux__
TEST_CASE("normalize() symlink with and without resolve") {
  TestEnvironment environment;
  PathLib::Path path(environment.symlink_to_file_a / "..");
  PathLib::Path unresolved_path = path.normalized_copy(false);
  PathLib::Path resolved_path   = path.normalized_copy(true);

  REQUIRE(resolved_path.path() == environment.directory_a);
  REQUIRE(unresolved_path.path() == environment.root_directory);

  REQUIRE(unresolved_path.good());
  REQUIRE(resolved_path.good());
}
#endif

TEST_CASE("normalize() does not strip root separator") {
  fs::path unix_("/"), windows("C:\\");
  PathLib::Path path_unix(unix_);
  PathLib::Path path_windows(windows);

  path_unix.normalize();
  path_windows.normalize();

  REQUIRE(path_unix.path() == unix_);
  REQUIRE(path_windows.path() == windows);

  REQUIRE(path_unix.good());
  REQUIRE(path_windows.good());
}

TEST_CASE("resolve_environment_variables() no substitution") {
  TestEnvironment environment;
  PathLib::Path path(environment.file_in_directory_a);

  path.resolve_environment_variables();

  REQUIRE(path.path() == environment.file_in_directory_a);
  REQUIRE(path.good());
}

TEST_CASE("resolve_environment_variables() home directory") {
  TestEnvironment environment;
#ifdef _WIN32
  const char* home_variable_name = environment.home_directory_windows_variable_name;
  PathLib::Path home_path(environment.home_directory_windows);
#else
  const char* home_variable_name = environment.home_directory_unix_variable_name;
  PathLib::Path home_path(environment.home_directory_unix);
#endif

  home_path.resolve_environment_variables();

  REQUIRE(home_path.string() == getenv(home_variable_name));
  REQUIRE(home_path.good());
}

TEST_CASE("resolve_environment_variables() with path") {
  const char* env_var_name = "PATHLIB_TEST";
#ifdef __linux__
  std::string value = "tmp/pathlib_test";
#else
  std::string value = "tmp\\pathlib_test";
#endif

#ifdef __linux__
  setenv(env_var_name, value.c_str(), 1);
#else
  _putenv_s(env_var_name, value.c_str());
#endif

  // at start of path
  {
    PathLib::Path path(std::string("$") + env_var_name + "/file.txt");
    path.resolve_environment_variables();
#ifdef __linux__
    REQUIRE(path.string() == value + "/file.txt");
#else
    REQUIRE(path.string() == value + "\\file.txt");
#endif
    REQUIRE(path.good());
  }

  // in middle of path
  {
#ifdef __linux__
    PathLib::Path path(std::string("/root/$") + env_var_name + "/file.txt");
    path.resolve_environment_variables();
    REQUIRE(path.string() == "/root/" + value + "/file.txt");
#else
    PathLib::Path path(std::string("\\root\\$") + env_var_name + "\\file.txt");
    path.resolve_environment_variables();
    REQUIRE(path.string() == "\\root\\" + value + "\\file.txt");
#endif
    REQUIRE(path.good());
  }

  // multiple variables in path
  {
#ifdef __linux__
    PathLib::Path path(std::string("$") + env_var_name + "/sub/$" + env_var_name);
    path.resolve_environment_variables();
    REQUIRE(path.string() == value + "/sub/" + value);
#else
    PathLib::Path path(std::string("$") + env_var_name + "\\sub\\$" + env_var_name);
    path.resolve_environment_variables();
    REQUIRE(path.string() == value + "\\sub\\" + value);
#endif
    REQUIRE(path.good());
  }

#ifdef __linux__
  unsetenv(env_var_name);
#else
  _putenv_s(env_var_name, "");
#endif
}

TEST_CASE("resolve_environment_variables() Linux XDG") {
#ifndef _WIN32
  unsetenv("XDG_CACHE_HOME");
  unsetenv("XDG_CONFIG_DIRS");
  unsetenv("XDG_CONFIG_HOME");
  unsetenv("XDG_DATA_DIRS");
  unsetenv("XDG_DATA_HOME");
  unsetenv("XDG_RUNTIME_DIR");
  unsetenv("XDG_STATE_HOME");

  setenv("HOME", "/home/testuser", 1);

  {
    PathLib::Path path("$XDG_CONFIG_HOME/test");
    path.resolve_environment_variables();
    CHECK(path.string() == "/home/testuser/.config/test");
  }

  {
    PathLib::Path path("$XDG_DATA_HOME/test");
    path.resolve_environment_variables();
    CHECK(path.string() == "/home/testuser/.local/share/test");
  }

  {
    PathLib::Path path("$XDG_CACHE_HOME/test");
    path.resolve_environment_variables();
    CHECK(path.string() == "/home/testuser/.cache/test");
  }

  {
    PathLib::Path path("$XDG_STATE_HOME/test");
    path.resolve_environment_variables();
    CHECK(path.string() == "/home/testuser/.local/state/test");
  }

  {
    PathLib::Path path("$XDG_CONFIG_DIRS/test");
    path.resolve_environment_variables();
    CHECK(path.string() == "/etc/xdg/test");
  }

  {
    PathLib::Path path("$XDG_DATA_DIRS/test");
    path.resolve_environment_variables();
    CHECK(path.string() == "/usr/local/share:/usr/share/test");
  }

  {
    PathLib::Path path("$XDG_RUNTIME_DIR/test");
    path.resolve_environment_variables();

    uid_t uid = getuid();
    CHECK(path.string() == "/run/user/" + std::to_string(uid) + "/test");
  }
#endif
}

TEST_CASE("resolve_environment_variables() variable patterns") {
  const char* env_var_name  = "PATHLIB_TEST";
  const char* env_var_value = "pathlib_test_value";

#ifdef __linux__
  setenv(env_var_name, env_var_value, 1);
#else
  _putenv_s(env_var_name, env_var_value);
#endif

  // dollar brackets
  {
    PathLib::Path path(std::string("${") + env_var_name + "}");
    path.resolve_environment_variables();
    REQUIRE(path.string() == env_var_value);
    REQUIRE(path.good());
  }

  // dollar brackets with default (var is set)
  {
    PathLib::Path path(std::string("${") + env_var_name + ":-a different default}");
    path.resolve_environment_variables();
    REQUIRE(path.string() == env_var_value);
    REQUIRE(path.good());
  }

  // dollar brackets with default (var is not set -> default value)
  {
    PathLib::Path path(std::string("${NOT_THE_VAR_NAME:-") + env_var_value + "}");
    path.resolve_environment_variables();
    REQUIRE(path.string() == env_var_value);
    REQUIRE(path.good());
  }

  // dollar brackets with empty default
  {
    PathLib::Path path("${NOT_THE_VAR_NAME:-}");
    path.resolve_environment_variables();
    REQUIRE(path.string().empty());
    REQUIRE(path.good());
  }

  // percent
  {
    PathLib::Path path(std::string("%") + env_var_name + "%");
    path.resolve_environment_variables();
    REQUIRE(path.string() == env_var_value);
    REQUIRE(path.good());
  }

#ifdef __linux__
  unsetenv(env_var_name);
#else
  _putenv_s(env_var_name, "");
#endif
}

TEST_CASE("resolve_environment_variables() recursion") {
  const char* env_var_name  = "PATHLIB_TEST";
  const char* env_var_value = "$PATHLIB_TEST$PATHLIB_TEST";

#ifdef __linux__
  setenv(env_var_name, env_var_value, 1);
#else
  _putenv_s(env_var_name, env_var_value);
#endif

  std::string pattern = std::string("$") + env_var_name;
  PathLib::Path path(pattern.c_str());
  path.resolve_environment_variables(2);
  REQUIRE(path.string() == "$PATHLIB_TEST$PATHLIB_TEST$PATHLIB_TEST");
  REQUIRE(path.good());
}

TEST_CASE("resolve_environment_variables() variable boundary detection") {
#ifdef __linux__
  setenv("VAR", "X", 1);
  setenv("VAR_1", "Y", 1);
  setenv("VAR2", "Z", 1);
#else
  _putenv_s("VAR", "X");
  _putenv_s("VAR_1", "Y");
  _putenv_s("VAR2", "Z");
#endif

  // underscore is valid
  {
    PathLib::Path path("$VAR_1");
    path.resolve_environment_variables();
    REQUIRE(path.string() == "Y");
  }

  // alphanumeric continuation
  {
    PathLib::Path path("$VAR2");
    path.resolve_environment_variables();
    REQUIRE(path.string() == "Z");
  }

  // stops before invalid char '-'
  {
    PathLib::Path path("$VAR-TEST");
    path.resolve_environment_variables();
    REQUIRE(path.string() == "X-TEST");
  }

  // stops correctly before dot
  {
    PathLib::Path path("$VAR.txt");
    path.resolve_environment_variables();
    REQUIRE(path.string() == "X.txt");
  }

  // adjacent variables
  {
    PathLib::Path path("$VAR$VAR");
    path.resolve_environment_variables();
    REQUIRE(path.string() == "XX");
  }

  // braces avoid ambiguity
  {
    PathLib::Path path("${VAR}_suffix");
    path.resolve_environment_variables();
    REQUIRE(path.string() == "X_suffix");
  }

#ifdef __linux__
  unsetenv("VAR");
  unsetenv("VAR_1");
  unsetenv("VAR2");
#else
  _putenv_s("VAR", "");
  _putenv_s("VAR_1", "");
  _putenv_s("VAR2", "");
#endif
}

TEST_CASE("resolve_environment_variables() non-destructive") {
  const char* env_var_name  = "PATHLIB_TEST";
  const char* env_var_value = "pathlib_test_value";

#ifdef __linux__
  setenv(env_var_name, env_var_value, 1);
#else
  _putenv_s(env_var_name, env_var_value);
#endif

  std::string pattern = std::string("$") + env_var_name;
  PathLib::Path original_path(pattern.c_str());

  PathLib::Path result_path = original_path.resolved_environment_variables_copy();

  REQUIRE(original_path.string() == pattern);
  REQUIRE(result_path.string() == env_var_value);

  REQUIRE(original_path.good());
  REQUIRE(result_path.good());
}
