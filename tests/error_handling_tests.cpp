#include <doctest/doctest.h>

#include <PathLib/PathLib.hpp>
#include <filesystem>

namespace fs = std::filesystem;

TEST_CASE("fs_canonical on non-existent path spoils Path") {
  PathLib::Path p("definitely_does_not_exist_123456.txt");

  REQUIRE(p.good());

  p.fs_canonical();  // should internally catch and spoil

  REQUIRE_FALSE(p.good());
  REQUIRE_FALSE(p.error().empty());
}

TEST_CASE("raise() throws after spoil") {
  PathLib::Path p("definitely_does_not_exist_987654.txt");

  p.fs_canonical();

  REQUIRE_FALSE(p.good());

  REQUIRE_THROWS_AS(p.raise(), std::runtime_error);
}

TEST_CASE("clear() restores good state") {
  PathLib::Path p("definitely_does_not_exist_clear_test.txt");

  p.fs_canonical();
  REQUIRE_FALSE(p.good());

  p.clear();

  REQUIRE(p.good());
  REQUIRE(p.error().empty());
}

TEST_CASE("error is not overwritten once spoiled") {
  PathLib::Path p("does_not_exist_first.txt");

  p.fs_canonical();
  REQUIRE_FALSE(p.good());

  std::string first_error = std::string(p.error());

  // second failure attempt
  p.fs_canonical();

  REQUIRE_FALSE(p.good());
  REQUIRE(std::string(p.error()) == first_error);
}

TEST_CASE("error message is truncated if too long") {
  PathLib::Path p;

  // Create artificial long exception message via custom safe() call
  p = PathLib::Path("dummy");

  // Force spoil manually via fs_canonical using an absurd path
  std::string very_long_path(1000, 'a');
  PathLib::Path long_p((std::string_view)very_long_path);

  long_p.fs_canonical();

  REQUIRE_FALSE(long_p.good());

  std::string err = std::string(long_p.error());
  REQUIRE(err.size() <= 255);

  if (err.size() == 255) {
    REQUIRE(err.find("... (truncated)") != std::string::npos);
  }
}
