#include <doctest/doctest.h>

#include <PathLib/PathLib.hpp>
#include <filesystem>

#include "test_environment.hpp"

namespace fs = std::filesystem;

#ifdef __linux__
TEST_CASE("owner()") {
  TestEnvironment environment;

  PathLib::Path path(environment.file_in_directory_a);

  uid_t owner = path.owner();

  REQUIRE(owner == geteuid());
  REQUIRE(path.good());
}

TEST_CASE("group()") {
  TestEnvironment environment;

  PathLib::Path path(environment.file_in_directory_a);
  gid_t group = path.group();

  REQUIRE(group == getegid());
  REQUIRE(path.good());
}

TEST_CASE("owner() and group() of tmp directory") {
  TestEnvironment environment;

  PathLib::Path path(environment.tmp_directory);

  uid_t owner = path.owner();
  gid_t group = path.group();

  REQUIRE(owner == environment.root_uid);
  REQUIRE(group == environment.root_uid);
  REQUIRE(path.good());
}
#endif

TEST_CASE("status()") {
  TestEnvironment environment;

  PathLib::Path path(environment.file_in_directory_a);
  PathLib::Status status = path.status();

  REQUIRE(status.has_all(PathLib::Exists | PathLib::IsFile));
  REQUIRE(path.good());
}

TEST_CASE("check()") {
  TestEnvironment environment;

  PathLib::Path path(environment.file_in_directory_a);
  // check() is true if path exists and is a file
  REQUIRE(path.check(PathLib::Exists | PathLib::IsFile));
  REQUIRE(path.good());
}

TEST_CASE("check_any()") {
  TestEnvironment environment;

  PathLib::Path file_path(environment.file_in_directory_a);
  PathLib::Path dir_path(environment.directory_a);
  // check_any() is true if path exists and is a file or path exists and is a directory
  REQUIRE(file_path.check_any({PathLib::Exists | PathLib::IsFile, PathLib::Exists | PathLib::IsDirectory}));
  REQUIRE(dir_path.check_any({PathLib::Exists | PathLib::IsFile, PathLib::Exists | PathLib::IsDirectory}));
  // if none of the combinations is true, check_any() returns false
  REQUIRE_FALSE(file_path.check_any({PathLib::NotFound | PathLib::IsFile, PathLib::IsDirectory | PathLib::IsFile}));
  // only one combination is equal to .check(...)
  REQUIRE_FALSE(dir_path.check_any({PathLib::NotFound}));

  REQUIRE(file_path.good());
  REQUIRE(dir_path.good());
}

TEST_CASE("exists()") {
  TestEnvironment environment;
  PathLib::Path file_path(environment.file_in_directory_a);
  PathLib::Path missing_path(environment.non_existant_file);

  REQUIRE(file_path.exists());
  REQUIRE_FALSE(missing_path.exists());
  REQUIRE(file_path.good());
  REQUIRE(missing_path.good());
}

TEST_CASE("not_found()") {
  TestEnvironment environment;
  PathLib::Path file_path(environment.file_in_directory_a);
  PathLib::Path missing_path(environment.non_existant_file);

  REQUIRE(missing_path.not_found());
  REQUIRE_FALSE(file_path.not_found());
  REQUIRE(file_path.good());
  REQUIRE(missing_path.good());
}

TEST_CASE("is_file()") {
  TestEnvironment environment;
  PathLib::Path file_path(environment.file_in_directory_a);
  PathLib::Path dir_path(environment.directory_a);

  REQUIRE(file_path.is_file());
  REQUIRE_FALSE(dir_path.is_file());
  REQUIRE(file_path.good());
  REQUIRE(dir_path.good());
}

TEST_CASE("is_directory()") {
  TestEnvironment environment;
  PathLib::Path dir_path(environment.directory_a);
  PathLib::Path file_path(environment.file_in_directory_a);

  REQUIRE(dir_path.is_directory());
  REQUIRE_FALSE(file_path.is_directory());
  REQUIRE(dir_path.good());
  REQUIRE(file_path.good());
}

#ifdef __linux__
TEST_CASE("is_symlink()") {
  TestEnvironment environment;
  PathLib::Path symlink_path(environment.symlink_to_file_a);
  PathLib::Path file_path(environment.file_in_directory_a);

  REQUIRE_FALSE(file_path.is_symlink(/* true by default */));

  // without following symlinks, symlink is detected
  REQUIRE(symlink_path.is_symlink(false));
  // following symlinks, symlink resolves to file
  REQUIRE_FALSE(symlink_path.is_symlink(true));
  REQUIRE(symlink_path.good());
  REQUIRE(file_path.good());
}
#endif

TEST_CASE("is_empty()") {
  TestEnvironment environment;
  PathLib::Path file_path(environment.file_in_directory_a);  // created, but empty
  PathLib::Path dir_path(environment.directory_b);           // has file_b inside
#ifdef __linux__
  PathLib::Path symlink_path(environment.symlink_to_file_a);
#endif

  REQUIRE(file_path.is_empty());
  REQUIRE_FALSE(dir_path.is_empty());
#ifdef __linux__
  // symlink is not empty when not resolving
  REQUIRE_FALSE(symlink_path.is_empty(false));
#endif
  REQUIRE(file_path.good());
  REQUIRE(dir_path.good());
#ifdef __linux__
  REQUIRE(symlink_path.good());
#endif
}

TEST_CASE("is_other()") {
  TestEnvironment environment;
  PathLib::Path file_path(environment.file_in_directory_a);
  PathLib::Path dir_path(environment.directory_a);
  PathLib::Path symlink_path(environment.symlink_to_file_a);

  REQUIRE_FALSE(file_path.is_other());
  REQUIRE_FALSE(dir_path.is_other());
  REQUIRE_FALSE(symlink_path.is_other());
  REQUIRE(file_path.good());
  REQUIRE(dir_path.good());
  REQUIRE(symlink_path.good());
}

TEST_CASE("is_root()") {
  PathLib::Path root_path(fs::path("/"));
  PathLib::Path non_root_path(fs::path("/usr"));

  REQUIRE(root_path.is_root());
  REQUIRE_FALSE(non_root_path.is_root());
  REQUIRE(root_path.good());
  REQUIRE(non_root_path.good());
}

TEST_CASE("parent_exists()") {
  TestEnvironment environment;
  PathLib::Path file_path(environment.file_in_directory_a);
  PathLib::Path deeply_missing(environment.root_directory / "nonexistent" / "file.txt");

  REQUIRE(file_path.parent_exists());
  REQUIRE_FALSE(deeply_missing.parent_exists());
  REQUIRE(file_path.good());
  REQUIRE(deeply_missing.good());
}

#ifdef __linux__
TEST_CASE("is_readable() basic cases") {
  TestEnvironment environment;
  PathLib::Path file_path(environment.file_in_directory_a);
  PathLib::Path non_existant_file(environment.non_existant_file);

  REQUIRE(file_path.is_readable());
  REQUIRE_FALSE(non_existant_file.is_readable());

  REQUIRE(file_path.good());
  REQUIRE(non_existant_file.good());
}

TEST_CASE("is_readable() no permissions") {
  TestEnvironment environment;
  PathLib::Path file_path(environment.file_in_directory_a);

  file_path.set_permissions(fs::perms::none);

  REQUIRE_FALSE(file_path.is_readable());
  REQUIRE(file_path.good());
}

TEST_CASE("is_readable() owner read") {
  TestEnvironment environment;
  PathLib::Path file_path(environment.file_in_directory_a);

  file_path.set_permissions(fs::perms::owner_read);

  REQUIRE(file_path.is_readable());
  REQUIRE(file_path.good());
}

TEST_CASE("is_readable() group read") {
  REQUIRE_ROOT();
  TestEnvironment environment;
  PathLib::Path file_path(environment.other_owner_file);
  file_path.set_owner(PathLib::NO_CHANGE, geteuid());

  file_path.set_permissions(fs::perms::group_read);

  REQUIRE(file_path.is_readable());
  REQUIRE(file_path.good());
}

TEST_CASE("is_readable() other read") {
  REQUIRE_ROOT();
  TestEnvironment environment;
  PathLib::Path file_path(environment.other_owner_file);

  file_path.set_permissions(fs::perms::others_read);

  REQUIRE(file_path.is_readable());
  REQUIRE(file_path.good());
}

TEST_CASE("is_readable() owner match does not fall back") {
  TestEnvironment environment;
  PathLib::Path file_path(environment.file_in_directory_a);

  file_path.set_permissions(fs::perms::others_read);

  REQUIRE_FALSE(file_path.is_readable());
}

TEST_CASE("is_readable() non-existing file") {
  TestEnvironment environment;
  PathLib::Path non_existant_file(environment.non_existant_file);

  REQUIRE_FALSE(non_existant_file.is_readable());
  REQUIRE(non_existant_file.good());
}

TEST_CASE("is_writable() no permissions") {
  TestEnvironment environment;
  PathLib::Path file_path(environment.file_in_directory_a);

  file_path.set_permissions(fs::perms::none);

  REQUIRE_FALSE(file_path.is_writable());
  REQUIRE(file_path.good());
}

TEST_CASE("is_writable() owner write") {
  TestEnvironment environment;
  PathLib::Path file_path(environment.file_in_directory_a);

  REQUIRE(file_path.is_writable());
  REQUIRE(file_path.good());
}

TEST_CASE("is_writable() group write") {
  REQUIRE_ROOT();
  TestEnvironment environment;
  PathLib::Path file_path(environment.other_owner_file);
  file_path.set_owner(PathLib::NO_CHANGE, geteuid());

  file_path.set_permissions(fs::perms::group_write);

  REQUIRE(file_path.is_writable());
  REQUIRE(file_path.good());
}

TEST_CASE("is_writable() other write") {
  REQUIRE_ROOT();
  TestEnvironment environment;
  PathLib::Path file_path(environment.other_owner_file);

  file_path.set_permissions(fs::perms::others_write);

  REQUIRE(file_path.is_writable());
  REQUIRE(file_path.good());
}

TEST_CASE("is_writable() non-existing file") {
  TestEnvironment environment;
  PathLib::Path non_existant_file(environment.non_existant_file);

  REQUIRE(non_existant_file.is_writable());
}
#endif
