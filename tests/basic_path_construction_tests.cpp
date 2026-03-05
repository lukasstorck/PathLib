#include <doctest/doctest.h>

#include <PathLib/PathLib.hpp>
#include <filesystem>

namespace fs = std::filesystem;

TEST_CASE("Path default constructor creates empty path") {
  PathLib::Path p;

  REQUIRE(p.path().empty());
  REQUIRE(p.string().empty());
  REQUIRE(p.good());
}

TEST_CASE("Path construct from fs::path") {
  fs::path raw = "test.txt";
  PathLib::Path p(raw);

  REQUIRE(p.path() == raw);
  REQUIRE(p.string() == raw.string());
  REQUIRE(p.good());
}

TEST_CASE("Path construct from fs::path safe constructor") {
  fs::path raw = "safe.txt";
  PathLib::Path p(raw, PathLib::safe_tag);

  REQUIRE(p.path() == raw);
  REQUIRE(p.string() == raw.string());
  REQUIRE(p.good());
}

TEST_CASE("Path construct from std::string_view") {
  std::string_view sv = "hello/world";
  PathLib::Path p(sv);

  REQUIRE(p.string() == fs::path(sv).string());
  REQUIRE(p.good());
}

TEST_CASE("Path construct from const char*") {
  PathLib::Path p("abc.txt");

  REQUIRE(p.string() == fs::path("abc.txt").string());
  REQUIRE(p.good());
}

TEST_CASE("Path construct from null const char* results in empty path") {
  const char* null_str = nullptr;
  PathLib::Path p(null_str);

  REQUIRE(p.path().empty());
  REQUIRE(p.good());
}

TEST_CASE("Path clone produces identical path") {
  PathLib::Path original("clone_me.txt");
  PathLib::Path copy = original.clone();

  REQUIRE(copy.path() == original.path());
  REQUIRE(copy.string() == original.string());
  REQUIRE(copy.good());
  REQUIRE(original.good());
}

TEST_CASE("Path parent returns correct parent path") {
  PathLib::Path p("dir/sub/file.txt");
  PathLib::Path parent = p.parent();

  REQUIRE(parent.path() == fs::path("dir/sub"));
  REQUIRE(p.good());
  REQUIRE(parent.good());
}

TEST_CASE("Path current_path returns non-empty path") {
  PathLib::Path p = PathLib::Path::current_path();

  REQUIRE_FALSE(p.path().empty());
  REQUIRE(p.good());
}

TEST_CASE("Path temp_directory_path returns non-empty path") {
  PathLib::Path p = PathLib::Path::temp_directory_path();

  REQUIRE_FALSE(p.path().empty());
  REQUIRE(p.good());
}

TEST_CASE("Path symlink policy toggling") {
  PathLib::Path p;

  p.follow_symlinks_for_status(false);
  p.follow_symlinks_for_resolution(false);

  REQUIRE_FALSE(p.get_symlink_policy().has(PathLib::SymlinkMode::FollowForStatus));
  REQUIRE_FALSE(p.get_symlink_policy().has(PathLib::SymlinkMode::FollowForResolution));

  p.follow_symlinks_for_status();
  REQUIRE(p.get_symlink_policy().has(PathLib::SymlinkMode::FollowForStatus));

  p.follow_symlinks_for_resolution();
  REQUIRE(p.get_symlink_policy().has(PathLib::SymlinkMode::FollowForResolution));

  REQUIRE(p.good());
}
