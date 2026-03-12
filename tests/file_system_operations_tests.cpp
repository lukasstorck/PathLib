#include <doctest/doctest.h>
#include <grp.h>
#include <pwd.h>
#include <sys/types.h>
#include <unistd.h>

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

TEST_CASE("set_owner() already owned file") {
  TestEnvironment environment;
  PathLib::Path file_path(environment.file_in_directory_a);

  // Set by UID only
  file_path.set_owner(environment.user_uid);
  REQUIRE(file_path.owner() == environment.user_uid);
  REQUIRE(file_path.good());

  // Set by UID and GID
  file_path.set_owner(environment.user_uid, environment.user_gid);
  REQUIRE(file_path.owner() == environment.user_uid);
  REQUIRE(file_path.group() == environment.user_gid);
  REQUIRE(file_path.good());
}

TEST_CASE("set_owner() by UID/GID") {
  REQUIRE_ROOT();
  TestEnvironment environment;
  PathLib::Path file_path(environment.other_owner_file);

  // Set by UID only
  file_path.set_owner(environment.user_uid);
  REQUIRE(file_path.owner() == environment.user_uid);
  REQUIRE(file_path.group() == environment.other_gid);
  REQUIRE(file_path.good());

  // Set by UID and GID
  file_path.set_owner(environment.user_uid, environment.user_gid);
  REQUIRE(file_path.owner() == environment.user_uid);
  REQUIRE(file_path.group() == environment.user_gid);
  REQUIRE(file_path.good());
}

TEST_CASE("set_owner() group only") {
  REQUIRE_ROOT();
  TestEnvironment environment;
  PathLib::Path file_path(environment.other_owner_file);

  file_path.set_owner(PathLib::NO_CHANGE, environment.user_uid);
  REQUIRE(file_path.owner() == environment.other_uid);
  REQUIRE(file_path.group() == environment.user_uid);
  REQUIRE(file_path.good());
}

TEST_CASE("set_owner() by username/groupname") {
  REQUIRE_ROOT();
  TestEnvironment environment;
  PathLib::Path file_path(environment.other_owner_file);

  file_path.set_owner(environment.user_name);
  REQUIRE(file_path.owner() == environment.user_uid);
  REQUIRE(file_path.group() == environment.other_gid);
  REQUIRE(file_path.good());

  file_path.set_owner(environment.user_name, environment.user_group_name);
  REQUIRE(file_path.owner() == environment.user_uid);
  REQUIRE(file_path.group() == environment.user_gid);
  REQUIRE(file_path.good());
}

TEST_CASE("set_owner() recursive on directory") {
  REQUIRE_ROOT();
  TestEnvironment environment;
  PathLib::Path dir_path(environment.nested_directory);

  dir_path.set_owner(environment.user_uid, environment.user_gid, true);

  REQUIRE(dir_path.owner() == environment.user_uid);
  REQUIRE(dir_path.group() == environment.user_gid);
  REQUIRE(dir_path.good());

  // Verify ownership recursively
  for (const auto& entry : std::filesystem::recursive_directory_iterator(dir_path.path())) {
    PathLib::Path child(entry.path());
    REQUIRE(child.owner() == environment.user_uid);
    REQUIRE(child.group() == environment.user_gid);
  }
}

TEST_CASE("set_owner() symlink handling - change target") {
  REQUIRE_ROOT();
  TestEnvironment environment;
  PathLib::Path symlink_path(environment.symlink_to_other_owner_file);
  PathLib::Path custom_owner_file(environment.other_owner_file);

  symlink_path.set_owner(environment.user_uid, environment.user_gid, false, true);
  CHECK(symlink_path.owner(false) == environment.other_uid);
  CHECK(symlink_path.group(false) == environment.other_gid);
  CHECK(custom_owner_file.owner() == environment.user_uid);
  CHECK(custom_owner_file.group() == environment.user_gid);
  CHECK(symlink_path.good());
  CHECK(custom_owner_file.good());
}

TEST_CASE("set_owner() symlink handling - change symlink") {
  REQUIRE_ROOT();
  TestEnvironment environment;
  PathLib::Path symlink_path(environment.symlink_to_other_owner_file);
  PathLib::Path custom_owner_file(environment.other_owner_file);

  symlink_path.set_owner(environment.user_uid, environment.user_gid, false, false);
  CHECK(symlink_path.owner(false) == environment.user_uid);
  CHECK(symlink_path.group(false) == environment.user_gid);
  CHECK(custom_owner_file.owner() == environment.other_uid);
  CHECK(custom_owner_file.group() == environment.other_gid);
  CHECK(symlink_path.good());
  CHECK(custom_owner_file.good());
}

TEST_CASE("set_owner() nonexistent user/group throws") {
  TestEnvironment environment;
  PathLib::Path directory_path(environment.directory_a);

  directory_path.set_owner("nonexistentuser", true);
  REQUIRE_FALSE(directory_path.good());
  REQUIRE(directory_path.error() == "Unknown user: nonexistentuser");

  directory_path.clear();

  directory_path.set_owner(PathLib::NO_CHANGE, "nonexistentgroup", true);
  REQUIRE_FALSE(directory_path.good());
  REQUIRE(directory_path.error() == "Unknown group: nonexistentgroup");
}

TEST_CASE("set_permissions()") {
  TestEnvironment environment;
  PathLib::Path file(environment.file_in_directory_a);

  file.set_permissions();
  INFO("file.status(): " << file.status());
  REQUIRE(file.status().has_all(PathLib::HasOwnerReadWrite | PathLib::HasGroupReadWrite));
  REQUIRE_FALSE(file.status().has_any(PathLib::HasOtherWrite));
  REQUIRE(file.good());
}

TEST_CASE("set_permissions() using PathLib::Status") {
  TestEnvironment environment;
  PathLib::Path file(environment.file_in_directory_a);

  PathLib::Status perms = PathLib::HasOwnerReadWrite;

  file.set_permissions(perms);
  REQUIRE(file.status().has_all(perms));
  REQUIRE_FALSE(file.status().has_any(PathLib::HasGroupReadWrite));
  REQUIRE(file.good());
}

TEST_CASE("set_permissions() using fs::perms") {
  TestEnvironment environment;
  PathLib::Path file(environment.file_in_directory_a);

  fs::perms perms = fs::perms::owner_read | fs::perms::owner_write;

  file.set_permissions(perms);
  REQUIRE(file.status().has_all(PathLib::HasOwnerReadWrite));
  REQUIRE_FALSE(file.status().has_any(PathLib::HasGroupReadWrite));
  REQUIRE(file.good());
}

TEST_CASE("set_permissions() using integer numeric mode") {
  TestEnvironment environment;
  PathLib::Path file(environment.file_in_directory_a);

  // numeric 600 == owner read/write
  file.set_permissions(0600);
  REQUIRE(file.status().has_all(PathLib::HasOwnerReadWrite));
  REQUIRE_FALSE(file.status().has_any(PathLib::HasGroupReadWrite));
  REQUIRE(file.good());
}

TEST_CASE("set_permissions() using symbolic string") {
  TestEnvironment environment;
  PathLib::Path file(environment.file_in_directory_a);

  file.set_permissions("u=rw");
  REQUIRE(file.status().has_all(PathLib::HasOwnerReadWrite));
  REQUIRE_FALSE(file.status().has_any(PathLib::HasGroupReadWrite));
  REQUIRE(file.good());
}

TEST_CASE("set_permissions() using PermissionMode enum") {
  TestEnvironment environment;
  PathLib::Path file(environment.file_in_directory_a);

  // replace
  file.set_permissions(PathLib::HasOwnerReadWrite, PathLib::Replace);
  REQUIRE(file.status().has_all(PathLib::HasOwnerReadWrite));
  REQUIRE_FALSE(file.status().has_any(PathLib::HasGroupReadWrite));

  // remove
  file.set_permissions(PathLib::HasOwnerWrite, PathLib::Remove);
  REQUIRE(file.status().has_all(PathLib::HasOwnerRead));
  REQUIRE_FALSE(file.status().has_any(PathLib::HasOwnerWrite));

  // add
  file.set_permissions(PathLib::HasOwnerWrite, PathLib::Add);
  REQUIRE(file.status().has_all(PathLib::HasOwnerReadWrite));

  REQUIRE(file.good());
}

TEST_CASE("set_permissions() using PermissionMode fs::perm_options") {
  TestEnvironment environment;
  PathLib::Path file(environment.file_in_directory_a);

  // replace
  file.set_permissions(PathLib::HasOwnerReadWrite, fs::perm_options::replace);
  REQUIRE(file.status().has_all(PathLib::HasOwnerReadWrite));
  REQUIRE_FALSE(file.status().has_any(PathLib::HasGroupReadWrite));

  // remove
  file.set_permissions(PathLib::HasOwnerWrite, fs::perm_options::remove);
  REQUIRE(file.status().has_all(PathLib::HasOwnerRead));
  REQUIRE_FALSE(file.status().has_any(PathLib::HasOwnerWrite));

  // add
  file.set_permissions(PathLib::HasOwnerWrite, fs::perm_options::add);
  REQUIRE(file.status().has_all(PathLib::HasOwnerReadWrite));

  REQUIRE(file.good());
}

TEST_CASE("set_permissions() using PermissionMode string") {
  TestEnvironment environment;
  PathLib::Path file(environment.file_in_directory_a);

  // replace
  file.set_permissions(PathLib::HasOwnerReadWrite, "replace");
  REQUIRE(file.status().has_all(PathLib::HasOwnerReadWrite));
  REQUIRE_FALSE(file.status().has_any(PathLib::HasGroupReadWrite));

  // remove
  file.set_permissions(PathLib::HasOwnerWrite, "remove");
  REQUIRE(file.status().has_all(PathLib::HasOwnerRead));
  REQUIRE_FALSE(file.status().has_any(PathLib::HasOwnerWrite));

  // add
  file.set_permissions(PathLib::HasOwnerWrite, "add");
  REQUIRE(file.status().has_all(PathLib::HasOwnerReadWrite));

  REQUIRE(file.good());
}

TEST_CASE("set_permissions() string input overwrites PermissionMode") {
  TestEnvironment environment;
  PathLib::Path file(environment.file_in_directory_a);

  // option "add" should be ignored and be overwritten by
  // symbolic permission string which specifies replace
  file.set_permissions("u=rw", PathLib::Add);

  REQUIRE(file.status().has_all(PathLib::HasOwnerReadWrite));
  REQUIRE_FALSE(file.status().has_any(PathLib::HasGroupReadWrite));
  REQUIRE(file.good());
}

TEST_CASE("set_permissions() on directory") {
  TestEnvironment environment;
  PathLib::Path dir(environment.directory_a);

  PathLib::Status perms = PathLib::HasOwnerAll;

  dir.set_permissions(perms, PathLib::Replace);

  REQUIRE(dir.status().has_all(perms));
  REQUIRE_FALSE(dir.status().has_any(PathLib::HasGroupReadWrite));
  REQUIRE(dir.good());
}

TEST_CASE("set_permissions() recursive on directory") {
  TestEnvironment environment;
  PathLib::Path dir(environment.directory_b);

  PathLib::Status perms = PathLib::HasOwnerAll;

  dir.set_permissions(perms, PathLib::Replace, true);

  REQUIRE(dir.status().has_all(perms));

  for (const auto& entry : fs::recursive_directory_iterator(dir.path())) {
    PathLib::Path child(entry.path());
    REQUIRE(child.status().has_all(perms));
    REQUIRE_FALSE(child.status().has_any(PathLib::HasGroupReadWrite));
    REQUIRE(child.good());
  }
}
