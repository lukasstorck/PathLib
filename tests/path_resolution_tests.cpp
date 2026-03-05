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

TEST_CASE("resolve_environment_variables() sets not good on unresolved variable") {
  PathLib::Path path(fs::path("$HOME/test.txt"));

  path.resolve_environment_variables();

  REQUIRE_FALSE(path.good());
}

TEST_CASE("resolve_environment_variables() non-destructive") {
  PathLib::Path original_path(fs::path("$HOME/test.txt"));

  PathLib::Path result_path = original_path.resolved_environment_variables_copy();

  REQUIRE(original_path.good());
  REQUIRE_FALSE(result_path.good());
}
