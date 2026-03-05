#include <doctest/doctest.h>

#include <PathLib/PathLib.hpp>
#include <filesystem>

#include "test_environment.hpp"

namespace fs = std::filesystem;

TEST_CASE("copy()") {
  TestEnvironment environment;
  PathLib::Path src(environment.file_in_directory_a);
  PathLib::Path dst(environment.root_directory / "copy_of_file_a.txt");

  src.copy(dst);
  REQUIRE(dst.exists());
  REQUIRE(src.exists());  // source still exists
  REQUIRE(src.good());
  REQUIRE(dst.good());
}

TEST_CASE("copy() directory recursive") {
  TestEnvironment environment;
  PathLib::Path src(environment.directory_a);
  PathLib::Path dst(environment.root_directory / "copy_of_dir_a");

  src.copy(dst);
  REQUIRE(dst.exists());
  REQUIRE(dst.is_directory());
  REQUIRE(src.good());
  REQUIRE(dst.good());
}

TEST_CASE("create_directory()") {
  TestEnvironment environment;
  PathLib::Path dir(environment.root_directory / "new_dir");

  dir.create_directory();
  REQUIRE(dir.exists());
  REQUIRE(dir.is_directory());
  REQUIRE(dir.good());
}

TEST_CASE("create_directory() with permissions") {
  TestEnvironment environment;
  PathLib::Path dir(environment.root_directory / "new_dir_with_perms");
  fs::perms perms = fs::perms::owner_read | fs::perms::owner_write | fs::perms::owner_exec;

  dir.create_directory(perms);
  REQUIRE(dir.exists());
  REQUIRE(dir.is_directory());
  REQUIRE(dir.status().has_all(PathLib::HasOwnerRead | PathLib::HasOwnerWrite | PathLib::HasOwnerExecute));
  REQUIRE(dir.good());
}

TEST_CASE("create_parent()") {
  TestEnvironment environment;
  PathLib::Path path(environment.root_directory / "new_parent_dir" / "file.txt");

  path.create_parent();
  REQUIRE(path.parent_exists());
  REQUIRE(path.good());
}

TEST_CASE("create_hard_link()") {
  TestEnvironment environment;
  PathLib::Path hard_link(environment.root_directory / "hard_link_to_file_a.txt");

  hard_link.create_hard_link(PathLib::Path(environment.file_in_directory_a));
  REQUIRE(hard_link.exists());
  REQUIRE(hard_link.is_file());
  REQUIRE(hard_link.good());
}

TEST_CASE("create_symlink() to file") {
  TestEnvironment environment;
  PathLib::Path symlink(environment.root_directory / "new_symlink_to_file_a.txt");

  symlink.create_symlink(PathLib::Path(environment.file_in_directory_a));
  REQUIRE(symlink.is_symlink(false));
  REQUIRE(symlink.is_file(true));
  REQUIRE(symlink.good());
}

TEST_CASE("create_symlink() to directory") {
  TestEnvironment environment;
  PathLib::Path symlink(environment.root_directory / "new_symlink_to_dir_a");

  symlink.create_symlink(PathLib::Path(environment.directory_a));
  REQUIRE(symlink.is_symlink(false));
  REQUIRE(symlink.is_directory(true));
  REQUIRE(symlink.good());
}

TEST_CASE("create_symlink() overwrites existing") {
  TestEnvironment environment;
  PathLib::Path symlink(environment.root_directory / "overwrite_symlink.txt");

  symlink.create_symlink(PathLib::Path(environment.file_in_directory_a));
  REQUIRE(symlink.exists());
  // overwrite with symlink to different target
  symlink.create_symlink(PathLib::Path(environment.file_in_directory_b));
  REQUIRE(symlink.is_symlink(false));
  REQUIRE(symlink.good());
}

TEST_CASE("remove() file") {
  TestEnvironment environment;
  PathLib::Path file(environment.file_in_directory_a);

  file.remove();
  REQUIRE_FALSE(file.exists());
  REQUIRE(file.good());
}

TEST_CASE("remove() recursive directory") {
  TestEnvironment environment;
  PathLib::Path dir(environment.directory_a);

  dir.remove(true);
  REQUIRE_FALSE(dir.exists());
  REQUIRE(dir.good());
}

TEST_CASE("remove() non-recursive directory fails") {
  TestEnvironment environment;
  PathLib::Path dir(environment.directory_a);  // contains files

  dir.remove(false);
  REQUIRE_FALSE(dir.good());  // should fail, directory is not empty
}

TEST_CASE("rename()") {
  TestEnvironment environment;
  PathLib::Path src(environment.file_in_directory_a);
  PathLib::Path dst(environment.root_directory / "renamed_file_a.txt");
  PathLib::Path original_src_path = src.clone();

  src.rename(dst);
  REQUIRE(dst.exists());
  // source is renamed to dst
  REQUIRE(src.exists());
  REQUIRE(src.path() == dst.path());
  // original src file is moved away
  REQUIRE_FALSE(original_src_path.exists());

  REQUIRE(src.good());
  REQUIRE(dst.good());
  REQUIRE(original_src_path.good());
}

TEST_CASE("rename() overwrites existing") {
  TestEnvironment environment;
  PathLib::Path src(environment.file_in_directory_a);
  PathLib::Path dst(environment.file_in_directory_b);
  PathLib::Path original_src_path = src.clone();

  src.rename(dst);
  REQUIRE(dst.exists());
  // source is renamed to dst
  REQUIRE(src.exists());
  REQUIRE(src.path() == dst.path());
  // original src file is moved away
  REQUIRE_FALSE(original_src_path.exists());

  REQUIRE(src.good());
  REQUIRE(dst.good());
  REQUIRE(original_src_path.good());
}

TEST_CASE("permissions()") {
  TestEnvironment environment;
  PathLib::Path file(environment.file_in_directory_a);
  fs::perms perms = fs::perms::owner_read | fs::perms::owner_write;

  file.permissions(perms);
  REQUIRE(file.status().has_all(PathLib::HasOwnerRead | PathLib::HasOwnerWrite));
  REQUIRE_FALSE(file.status().has_any(PathLib::HasOwnerExecute));
  REQUIRE(file.good());
}
