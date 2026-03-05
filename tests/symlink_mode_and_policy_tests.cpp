#include <doctest/doctest.h>

#include <PathLib/PathLib.hpp>
#include <filesystem>

namespace fs = std::filesystem;

TEST_CASE("SymlinkPolicy default state") {
  PathLib::SymlinkPolicy policy;

  REQUIRE_FALSE(policy.has(PathLib::SymlinkMode::FollowForStatus));
  REQUIRE_FALSE(policy.has(PathLib::SymlinkMode::FollowForResolution));
  REQUIRE_FALSE(policy.has(PathLib::SymlinkMode::AlwaysFollow));
}

TEST_CASE("SymlinkPolicy construct from single mode") {
  PathLib::SymlinkPolicy policy(PathLib::SymlinkMode::FollowForStatus);

  REQUIRE(policy.has(PathLib::SymlinkMode::FollowForStatus));
  REQUIRE_FALSE(policy.has(PathLib::SymlinkMode::FollowForResolution));
}

TEST_CASE("SymlinkMode bitwise OR") {
  PathLib::SymlinkMode combined = PathLib::SymlinkMode::FollowForStatus | PathLib::SymlinkMode::FollowForResolution;

  PathLib::SymlinkPolicy policy(combined);

  REQUIRE(policy.has(PathLib::SymlinkMode::FollowForStatus));
  REQUIRE(policy.has(PathLib::SymlinkMode::FollowForResolution));
  REQUIRE(policy.has(PathLib::SymlinkMode::AlwaysFollow));
}

TEST_CASE("SymlinkMode AlwaysFollow flag") {
  PathLib::SymlinkPolicy policy(PathLib::SymlinkMode::AlwaysFollow);

  REQUIRE(policy.has(PathLib::SymlinkMode::FollowForStatus));
  REQUIRE(policy.has(PathLib::SymlinkMode::FollowForResolution));
}

TEST_CASE("SymlinkPolicy set flag") {
  PathLib::SymlinkPolicy policy;

  policy.set(PathLib::SymlinkMode::FollowForResolution);

  REQUIRE(policy.has(PathLib::SymlinkMode::FollowForResolution));
  REQUIRE_FALSE(policy.has(PathLib::SymlinkMode::FollowForStatus));
}

TEST_CASE("SymlinkPolicy clear flag") {
  PathLib::SymlinkPolicy policy(PathLib::SymlinkMode::AlwaysFollow);

  policy.clear(PathLib::SymlinkMode::FollowForStatus);

  REQUIRE_FALSE(policy.has(PathLib::SymlinkMode::FollowForStatus));
  REQUIRE(policy.has(PathLib::SymlinkMode::FollowForResolution));
}
