#include <doctest/doctest.h>

#include <PathLib/PathLib.hpp>
#include <filesystem>

namespace fs = std::filesystem;

TEST_CASE("Status default state is None") {
  PathLib::Status s;

  REQUIRE_FALSE(s.has_any(PathLib::Exists));
  REQUIRE_FALSE(s.has_all(PathLib::Exists));
  REQUIRE(s == PathLib::Status(PathLib::StatusFlag::None));
}

TEST_CASE("Status single flag construction") {
  PathLib::Status s(PathLib::StatusFlag::Exists);

  REQUIRE(s.has_any(PathLib::Exists));
  REQUIRE(s.has_all(PathLib::Exists));
  REQUIRE_FALSE(s.has_any(PathLib::IsFile));
}

TEST_CASE("Status bitwise OR combines flags") {
  PathLib::Status s = PathLib::Exists | PathLib::IsFile;

  REQUIRE(s.has_all(PathLib::Exists | PathLib::IsFile));
  REQUIRE(s.has_any(PathLib::Exists));
  REQUIRE(s.has_any(PathLib::IsFile));
  REQUIRE_FALSE(s.has_all(PathLib::Exists | PathLib::IsDirectory));
}

TEST_CASE("Status bitwise AND isolates flags") {
  PathLib::Status s = PathLib::Exists | PathLib::IsDirectory;

  PathLib::Status masked = s & PathLib::IsDirectory;

  REQUIRE(masked == PathLib::IsDirectory);
  REQUIRE_FALSE(masked.has_any(PathLib::IsFile));
}

TEST_CASE("Status has_any vs has_all distinction") {
  PathLib::Status s = PathLib::HasOwnerRead | PathLib::HasOwnerWrite;

  REQUIRE(s.has_any(PathLib::HasOwnerRead));
  REQUIRE_FALSE(s.has_any(PathLib::HasOwnerExecute));
  REQUIRE_FALSE(s.has_all(PathLib::HasOwnerAll));
  REQUIRE(s.has_all(PathLib::HasOwnerRead | PathLib::HasOwnerWrite));
}

TEST_CASE("Status combined permission aliases") {
  PathLib::Status s = PathLib::HasGroupAll;

  REQUIRE(s.has_all(PathLib::HasGroupRead));
  REQUIRE(s.has_all(PathLib::HasGroupWrite));
  REQUIRE(s.has_all(PathLib::HasGroupExecute));
}

TEST_CASE("Status flag aliases refer to same bit") {
  REQUIRE(PathLib::IsFile == PathLib::IsRegular);
  REQUIRE(PathLib::NotFound == PathLib::Nonexistent);
}

TEST_CASE("Status equality and inequality") {
  PathLib::Status a = PathLib::Exists | PathLib::IsFile;
  PathLib::Status b = PathLib::Exists | PathLib::IsFile;
  PathLib::Status c = PathLib::Exists | PathLib::IsDirectory;

  REQUIRE(a == b);
  REQUIRE(a != c);
}
