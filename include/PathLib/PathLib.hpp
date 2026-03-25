#pragma once

#ifdef _WIN32
// #include <windows.h>
#else
#include <fcntl.h>
#include <grp.h>
#include <pwd.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

#include <bitset>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <utility>
#include <variant>
#include <vector>

namespace fs = std::filesystem;

namespace {
#ifdef __linux__
bool is_process_file_group_owner(gid_t file_group_id, gid_t process_group_id) noexcept {
  // direct group id match
  bool group_id_matches = file_group_id == process_group_id;

  // check secondary groups
  if (!group_id_matches) {
    int group_count = getgroups(0, nullptr);
    std::vector<gid_t> process_groups(group_count);
    getgroups(group_count, process_groups.data());

    for (auto group_id : process_groups) {
      // secondary group id match
      if (group_id == file_group_id) group_id_matches = true;
    }
  }

  return group_id_matches;
}
#endif

std::pair<fs::perms, fs::perm_options> symbolic_permission_string_to_fs_perms_and_options(const std::string& permission) {
  size_t i = 0;
  fs::perm_options option;
  fs::perms perms = fs::perms::none;

  bool u = false, g = false, o = false;

  // parse who (user, group, other)

  while (i < permission.size()) {
    char target_char = std::tolower(permission[i]);

    if (target_char == 'u')
      u = true;
    else if (target_char == 'g')
      g = true;
    else if (target_char == 'o')
      o = true;
    else if (target_char == 'a')
      u = g = o = true;
    else
      break;

    ++i;
  }

  if (!(u || g || o)) u = g = o = true;

  // parse operator (add, remove, replace)

  if (i >= permission.size()) throw std::runtime_error("error while parsing permission string: missing operator");

  char op = permission[i++];
  if (op == '+')
    option = fs::perm_options::add;
  else if (op == '-')
    option = fs::perm_options::remove;
  else if (op == '=')
    option = fs::perm_options::replace;
  else
    throw std::runtime_error("error while parsing permission string: invalid operator");

  // parse permissions

  while (i < permission.size()) {
    char permission_char = std::tolower(permission[i++]);

    switch (permission_char) {
      case 'r':
        if (u) perms |= fs::perms::owner_read;
        if (g) perms |= fs::perms::group_read;
        if (o) perms |= fs::perms::others_read;
        break;

      case 'w':
        if (u) perms |= fs::perms::owner_write;
        if (g) perms |= fs::perms::group_write;
        if (o) perms |= fs::perms::others_write;
        break;

      case 'x':
        if (u) perms |= fs::perms::owner_exec;
        if (g) perms |= fs::perms::group_exec;
        if (o) perms |= fs::perms::others_exec;
        break;

      case 's':
        if (u) perms |= fs::perms::set_uid;
        if (g) perms |= fs::perms::set_gid;
        break;

      case 't':
        perms |= fs::perms::sticky_bit;
        break;

      default:
        throw std::runtime_error("error while parsing permission string: invalid permission character");
    }
  }

  return {perms, option};
}
}  // namespace

namespace env {

inline const char* HOME_UNIX    = "$HOME";
inline const char* HOME_WINDOWS = "%%USERPROFILE%%";

class Options {
 public:
  enum OptionsFlag : unsigned {
    None                      = 0,
    ResolveTilde              = 1 << 0,
    TildeWindowsHomeStyle     = 1 << 1,
    ResolveXDGPaths           = 1 << 2,
    ResolveDollar             = 1 << 3,
    ResolveDollarBrace        = 1 << 4,
    ResolveDollarBraceDefault = 1 << 5,
    ResolvePercent            = 1 << 6,
    Default                   = ResolveTilde | ResolveXDGPaths | ResolveDollar | ResolveDollarBrace | ResolveDollarBraceDefault | ResolvePercent,
  };

  Options() : bits_(std::to_underlying(OptionsFlag::None)) {}
  Options(OptionsFlag flag) : bits_(std::to_underlying(flag)) {}

  Options operator|(OptionsFlag flag) const {
    Options result;
    result.bits_ = this->bits_ | std::to_underlying(flag);
    return result;
  }

  Options& operator|=(OptionsFlag flag) {
    this->bits_ |= std::to_underlying(flag);
    return *this;
  }

  bool has(OptionsFlag flag) const { return (this->bits_ & std::to_underlying(flag)) != 0; }

 private:
  unsigned bits_;
};

inline std::string get(const std::string& name, const std::string& default_value = {}, Options options = Options::Default) {
  const char* value = std::getenv(name.c_str());
  if (value && *value) return value;

  if (!default_value.empty()) return default_value;

  if (options.has(Options::ResolveXDGPaths)) {
    if (name == "XDG_CACHE_HOME") return "$HOME/.cache";
    if (name == "XDG_CONFIG_DIRS") return "/etc/xdg";
    if (name == "XDG_CONFIG_HOME") return "$HOME/.config";
    if (name == "XDG_DATA_DIRS") return "/usr/local/share:/usr/share";
    if (name == "XDG_DATA_HOME") return "$HOME/.local/share";
    if (name == "XDG_RUNTIME_DIR") return "/run/user/" + std::to_string(getuid());
    if (name == "XDG_STATE_HOME") return "$HOME/.local/state";
  }
  return {};
}

inline std::string resolve(const char* input, size_t max_iterations = 3, Options options = Options::Default) {
  if (!input) return {};

  std::string current(input);

  // replace ~ with HOME_WINDOWS or HOME_UNIX

  if (options.has(Options::ResolveTilde) && current.starts_with('~')) {
    std::string home_variable_name = options.has(Options::TildeWindowsHomeStyle) ? HOME_WINDOWS : HOME_UNIX;

    if (current.size() == 1) {
      current = home_variable_name;
    } else if (current[1] == '/' || current[1] == '\\') {
      current = home_variable_name + current.substr(1);
    }
  }

  // iteratively resolve environment variables

  for (size_t i = 0; i < max_iterations; ++i) {
    std::string next = current;

    size_t start = std::string::npos;
    size_t end   = std::string::npos;

    std::string var_name;
    std::string default_value;

    // find a environment variable pattern

    if ((start = next.find("${")) != std::string::npos) {  // pattern: ${VAR:-default}
      size_t close = next.find('}', start);
      if (close == std::string::npos) break;

      std::string expr = next.substr(start + 2, close - start - 2);
      size_t pos       = expr.find(":-");

      if (pos != std::string::npos) {
        var_name      = expr.substr(0, pos);
        default_value = expr.substr(pos + 2);
      } else {
        var_name = expr;
      }

      end = close + 1;
    } else if ((start = next.find('$')) != std::string::npos) {  // pattern: $VAR
      size_t var_start = start + 1;
      size_t var_end   = var_start;

      while (var_end < next.size() && (std::isalnum(static_cast<unsigned char>(next[var_end])) || next[var_end] == '_')) {
        ++var_end;
      }

      if (var_end == var_start) break;

      var_name = next.substr(var_start, var_end - var_start);
      end      = var_end;
    } else if ((start = next.find('%')) != std::string::npos) {  // pattern: %VAR%
      size_t close = next.find('%', start + 1);
      if (close == std::string::npos) break;

      var_name = next.substr(start + 1, close - start - 1);
      end      = close + 1;
    } else {
      // nothing left to replace
      break;
    }

    // resolve value
    std::string replacement = get(var_name, default_value, options);

    // rebuild string
    next.replace(start, end - start, replacement);

    if (next == current) break;

    current = std::move(next);
  }

  return current;
}
inline std::string resolve(const std::string& input, size_t max_iterations = 3, Options options = Options::Default) {
  return resolve(input.c_str(), max_iterations, options);
}
}  // namespace env

namespace PathLib {

enum class PermissionMode : unsigned {
  /// @brief  Replace existing permissions
  Replace = 0,
  /// @brief  Add permissions to existing
  Add = 1,
  /// @brief  Remove permissions from existing
  Remove = 2,
};

enum class SymlinkMode : unsigned {
  /// \brief Do not follow symlinks
  NeverFollow = 0,
  /// @brief Report the status of the symlink target, not the symlink itself (for status operations)
  FollowForStatus = 1 << 0,
  /// @brief Replace symlinks with their targets within the path (for resolution operations)
  FollowForResolution = 1 << 1,
  AlwaysFollow        = FollowForStatus | FollowForResolution,
};

constexpr SymlinkMode operator|(SymlinkMode a, SymlinkMode b) { return static_cast<SymlinkMode>(std::to_underlying(a) | std::to_underlying(b)); }

class SymlinkPolicy {
 public:
  using underlying = unsigned;

  constexpr SymlinkPolicy() noexcept = default;
  constexpr SymlinkPolicy(SymlinkMode mode) noexcept : bits_(std::to_underlying(mode)) {}

  constexpr bool has(SymlinkMode flag) const noexcept { return (bits_ & std::to_underlying(flag)) != 0; }
  constexpr void set(SymlinkMode flag) noexcept { bits_ |= std::to_underlying(flag); }
  constexpr void clear(SymlinkMode flag) noexcept { bits_ &= ~std::to_underlying(flag); }

 private:
  underlying bits_ = 0;
};

enum class StatusFlag : unsigned {
  None = 0,

  // file system object type

  NotFound    = 1 << 0,  // path (file system object) does not exist
  Exists      = 1 << 1,  // path (file system object) exists
  IsFile      = 1 << 2,  // path is regular file
  IsDirectory = 1 << 3,
  IsSymlink   = 1 << 4,
  IsBlock     = 1 << 5,  // path is block device
  IsCharacter = 1 << 6,  // path is character device
  IsFifo      = 1 << 7,  // path is named IPC pipe
  IsSocket    = 1 << 8,  // path is named IPC socket
  IsUnknown   = 1 << 9,  // path is of unknown type

  // file system object permissions

  HasOwnerRead    = 1 << 10,
  HasOwnerWrite   = 1 << 11,
  HasOwnerExecute = 1 << 12,
  HasGroupRead    = 1 << 13,
  HasGroupWrite   = 1 << 14,
  HasGroupExecute = 1 << 15,
  HasOtherRead    = 1 << 16,
  HasOtherWrite   = 1 << 17,
  HasOtherExecute = 1 << 18,
  IsUidBitSet     = 1 << 19,
  IsGidBitSet     = 1 << 20,
  IsStickyBitSet  = 1 << 21,

  ParentExists = 1 << 22,
};

class Status {
 public:
  constexpr Status() noexcept = default;
  constexpr Status(StatusFlag flag) noexcept : bits_(static_cast<unsigned>(flag)) {}

  constexpr bool has_any(Status s) const noexcept { return (*this & s) != Status(StatusFlag::None); }
  constexpr bool has_all(Status s) const noexcept { return (*this & s) == s; }

  friend constexpr Status operator|(Status a, Status b) noexcept { return Status(a.bits_ | b.bits_); }
  friend constexpr Status operator&(Status a, Status b) noexcept { return Status(a.bits_ & b.bits_); }
  friend constexpr bool operator==(Status a, Status b) noexcept { return a.bits_ == b.bits_; }
  friend constexpr bool operator!=(Status a, Status b) noexcept { return a.bits_ != b.bits_; }
  friend std::ostream& operator<<(std::ostream& output_stream, const Status& status) {
    output_stream << "Status(0b" << std::bitset<32>(status.bits_) << ")";
    return output_stream;
  }

  fs::perms fs_perms() const noexcept {
    fs::perms perms = fs::perms::none;
    if (this->has_any(StatusFlag::HasOwnerRead)) perms |= fs::perms::owner_read;
    if (this->has_any(StatusFlag::HasOwnerWrite)) perms |= fs::perms::owner_write;
    if (this->has_any(StatusFlag::HasOwnerExecute)) perms |= fs::perms::owner_exec;
    if (this->has_any(StatusFlag::HasGroupRead)) perms |= fs::perms::group_read;
    if (this->has_any(StatusFlag::HasGroupWrite)) perms |= fs::perms::group_write;
    if (this->has_any(StatusFlag::HasGroupExecute)) perms |= fs::perms::group_exec;
    if (this->has_any(StatusFlag::HasOtherRead)) perms |= fs::perms::others_read;
    if (this->has_any(StatusFlag::HasOtherWrite)) perms |= fs::perms::others_write;
    if (this->has_any(StatusFlag::HasOtherExecute)) perms |= fs::perms::others_exec;
    if (this->has_any(StatusFlag::IsUidBitSet)) perms |= fs::perms::set_uid;
    if (this->has_any(StatusFlag::IsGidBitSet)) perms |= fs::perms::set_gid;
    if (this->has_any(StatusFlag::IsStickyBitSet)) perms |= fs::perms::sticky_bit;
    return perms;
  }

 private:
  constexpr explicit Status(unsigned bits) noexcept : bits_(bits) {}
  unsigned bits_ = 0;
};

// expose flags so that they are accessible as PathLib::IsFile

inline constexpr Status NotFound    = StatusFlag::NotFound;
inline constexpr Status Nonexistent = StatusFlag::NotFound;
inline constexpr Status Exists      = StatusFlag::Exists;
inline constexpr Status IsFile      = StatusFlag::IsFile;
inline constexpr Status IsRegular   = StatusFlag::IsFile;
inline constexpr Status IsDirectory = StatusFlag::IsDirectory;
inline constexpr Status IsSymlink   = StatusFlag::IsSymlink;
inline constexpr Status IsBlock     = StatusFlag::IsBlock;
inline constexpr Status IsCharacter = StatusFlag::IsCharacter;
inline constexpr Status IsFifo      = StatusFlag::IsFifo;
inline constexpr Status IsSocket    = StatusFlag::IsSocket;
inline constexpr Status IsUnknown   = StatusFlag::IsUnknown;

inline constexpr Status HasOwnerRead      = StatusFlag::HasOwnerRead;
inline constexpr Status HasOwnerWrite     = StatusFlag::HasOwnerWrite;
inline constexpr Status HasOwnerExecute   = StatusFlag::HasOwnerExecute;
inline constexpr Status HasOwnerReadWrite = HasOwnerRead | HasOwnerWrite;
inline constexpr Status HasOwnerAll       = HasOwnerReadWrite | HasOwnerExecute;

inline constexpr Status HasGroupRead      = StatusFlag::HasGroupRead;
inline constexpr Status HasGroupWrite     = StatusFlag::HasGroupWrite;
inline constexpr Status HasGroupExecute   = StatusFlag::HasGroupExecute;
inline constexpr Status HasGroupReadWrite = HasGroupRead | HasGroupWrite;
inline constexpr Status HasGroupAll       = HasGroupReadWrite | HasGroupExecute;

inline constexpr Status HasOtherRead      = StatusFlag::HasOtherRead;
inline constexpr Status HasOtherWrite     = StatusFlag::HasOtherWrite;
inline constexpr Status HasOtherExecute   = StatusFlag::HasOtherExecute;
inline constexpr Status HasOtherReadWrite = HasOtherRead | HasOtherWrite;
inline constexpr Status HasOtherAll       = HasOtherReadWrite | HasOtherExecute;

inline constexpr Status IsUidBitSet    = StatusFlag::IsUidBitSet;
inline constexpr Status IsGidBitSet    = StatusFlag::IsGidBitSet;
inline constexpr Status IsStickyBitSet = StatusFlag::IsStickyBitSet;

inline constexpr Status ParentExists = StatusFlag::ParentExists;

inline constexpr PermissionMode Replace = PermissionMode::Replace;
inline constexpr PermissionMode Add     = PermissionMode::Add;
inline constexpr PermissionMode Remove  = PermissionMode::Remove;

struct NoOwnerChangeType {};
inline constexpr NoOwnerChangeType NO_CHANGE{};

#ifdef __linux__
using UserType  = std::variant<uid_t, std::string, NoOwnerChangeType>;
using GroupType = std::variant<gid_t, std::string, NoOwnerChangeType>;
#endif

using PermissionType        = std::variant<Status, fs::perms, int, std::string>;
using PermissionOptionsType = std::variant<PermissionMode, fs::perm_options, std::string>;

struct safe_t {};
inline constexpr safe_t safe_tag{};

class Path {
 public:
  // constructors

  Path() noexcept {}
  Path(const fs::path& path) : path_(path) {}
  Path(const fs::path& path, safe_t) noexcept {
    this->safe([this, &path] { this->path_ = path; });
  }
  Path(const char* path_str) : path_(path_str ? path_str : "") {}

  [[nodiscard]] Path clone() const noexcept { return Path(this->path_, safe_tag); }
  [[nodiscard]] Path parent() const noexcept { return this->clone().fs_parent_path(); }

  [[nodiscard]] static Path current_path() noexcept { return Path::fs_current_path(); }
  [[nodiscard]] static Path temp_directory_path() noexcept { return Path::fs_temp_directory_path(); }

  // read path variable
  [[nodiscard]] const fs::path& path() const noexcept { return this->path_; }
  [[nodiscard]] std::string string() const { return this->path().string(); }

  // ======================
  // === symlink policy ===
  // ======================

  Path& follow_symlinks_for_status(bool enable = true) noexcept {
    if (enable) {
      symlink_mode.set(SymlinkMode::FollowForStatus);
    } else {
      symlink_mode.clear(SymlinkMode::FollowForStatus);
    }
    return *this;
  }

  Path& follow_symlinks_for_resolution(bool enable = true) noexcept {
    if (enable) {
      symlink_mode.set(SymlinkMode::FollowForResolution);
    } else {
      symlink_mode.clear(SymlinkMode::FollowForResolution);
    }
    return *this;
  }

  [[nodiscard]] const SymlinkPolicy& get_symlink_policy() const noexcept { return symlink_mode; }

  // =======================
  // === path resolution ===
  // =======================

  Path& absolute() noexcept { return this->absolute(this->symlink_mode.has(SymlinkMode::FollowForResolution)); }
  Path& absolute(bool resolve_symlinks) noexcept {
    this->fs_absolute();
    this->normalize(resolve_symlinks);
    return *this;
  }

  Path& relative() noexcept {
    const Path base = Path::current_path();
    this->safe([&base] { base.raise(); });
    return this->relative(base);
  }
  Path& relative(const Path& base) noexcept { return this->relative(base, this->symlink_mode.has(SymlinkMode::FollowForResolution)); }
  Path& relative(bool resolve_symlinks) noexcept {
    const Path base = Path::current_path();
    this->safe([&base] { base.raise(); });
    return this->relative(base, resolve_symlinks);
  }
  Path& relative(const Path& base, bool resolve_symlinks) noexcept {
    if (resolve_symlinks) {
      this->fs_relative(base);
    } else {
      this->fs_lexically_relative(base);
    }
    return *this;
  }

  Path& normalize() noexcept { return this->normalize(this->symlink_mode.has(SymlinkMode::FollowForResolution)); }
  Path& normalize(bool resolve_symlinks) noexcept {
    if (resolve_symlinks) {
      this->fs_weakly_canonical();
    } else {
      this->fs_lexically_normal();
    }

    // remove trailing separator (e.g. "/some/path/" -> "/some/path")
    if (!this->fs_has_filename()) {
      this->fs_parent_path();
    };
    return *this;
  }

  Path& resolve_environment_variables(size_t max_per_segment_substitutions = 3, env::Options options = env::Options::Default) noexcept {
    fs::path resolved_path;

    this->safe([this, &resolved_path, &max_per_segment_substitutions, &options] {
      for (fs::path::iterator path_segment = this->path_.begin(); path_segment != this->path_.end(); ++path_segment) {
        std::string resolved_path_segment = env::resolve(path_segment->string(), max_per_segment_substitutions, options);

        if (resolved_path.empty()) {
          resolved_path = fs::path(resolved_path_segment);
        } else {
          resolved_path /= fs::path(resolved_path_segment);
        }
      }

      this->path_ = resolved_path;
    });
    return *this;
  }

  // non-destructive variants

  [[nodiscard]] Path absolute_copy() const noexcept { return this->clone().absolute(); }
  [[nodiscard]] Path absolute_copy(bool resolve_symlinks) const noexcept { return this->clone().absolute(resolve_symlinks); }
  [[nodiscard]] Path relative_copy() const noexcept { return this->clone().relative(); }
  [[nodiscard]] Path relative_copy(const Path& base) const noexcept { return this->clone().relative(base); }
  [[nodiscard]] Path relative_copy(bool resolve_symlinks) const noexcept { return this->clone().relative(resolve_symlinks); }
  [[nodiscard]] Path relative_copy(const Path& base, bool resolve_symlinks) const noexcept { return this->clone().relative(base, resolve_symlinks); }
  [[nodiscard]] Path normalized_copy() const noexcept { return this->clone().normalize(); }
  [[nodiscard]] Path normalized_copy(bool resolve_symlinks) const noexcept { return this->clone().normalize(resolve_symlinks); }
  [[nodiscard]] Path resolved_environment_variables_copy(size_t max_per_segment_substitutions = 3,
                                                         env::Options options                 = env::Options::Default) const noexcept {
    return this->clone().resolve_environment_variables(max_per_segment_substitutions, options);
  }

  // =================================
  // === file system object status ===
  // =================================

#ifdef __linux__

  uid_t owner() noexcept { return this->owner(this->symlink_mode.has(SymlinkMode::FollowForStatus)); }
  uid_t owner(bool resolve_symlinks) noexcept {
    struct stat file_status;

    if (resolve_symlinks) {
      stat(this->fs_c_str(), &file_status);
    } else {
      lstat(this->fs_c_str(), &file_status);
    }

    return file_status.st_uid;
  }

  gid_t group() noexcept { return this->group(this->symlink_mode.has(SymlinkMode::FollowForStatus)); }
  gid_t group(bool resolve_symlinks) noexcept {
    struct stat file_status;

    if (resolve_symlinks) {
      stat(this->fs_c_str(), &file_status);
    } else {
      lstat(this->fs_c_str(), &file_status);
    }

    return file_status.st_gid;
  }
#endif

  Status status() noexcept { return this->status(this->symlink_mode.has(SymlinkMode::FollowForStatus)); }
  Status status(bool resolve_symlinks) noexcept {
    fs::file_status file_status;
    if (resolve_symlinks) {
      file_status = this->fs_status();
    } else {
      file_status = this->fs_symlink_status();
    }

    Status flags;

    switch (file_status.type()) {
      case fs::file_type::not_found:
        flags = flags | NotFound;
        break;
      case fs::file_type::regular:
        flags = flags | Exists | IsRegular;
        break;
      case fs::file_type::directory:
        flags = flags | Exists | IsDirectory;
        break;
      case fs::file_type::symlink:
        flags = flags | Exists | IsSymlink;
        break;
      case fs::file_type::block:
        flags = flags | Exists | IsBlock;
        break;
      case fs::file_type::character:
        flags = flags | Exists | IsCharacter;
        break;
      case fs::file_type::fifo:
        flags = flags | Exists | IsFifo;
        break;
      case fs::file_type::socket:
        flags = flags | Exists | IsSocket;
        break;
      case fs::file_type::unknown:
      default:
        flags = flags | Exists | IsUnknown;
        break;
    }

#ifdef __linux__
    fs::perms permissions = file_status.permissions();

    auto apply_permission_flag_if_present = [&permissions, &flags](fs::perms permission, Status flag) {
      if ((permissions & permission) != fs::perms::none) flags = flags | flag;
    };

    apply_permission_flag_if_present(fs::perms::owner_read, HasOwnerRead);
    apply_permission_flag_if_present(fs::perms::owner_write, HasOwnerWrite);
    apply_permission_flag_if_present(fs::perms::owner_exec, HasOwnerExecute);
    apply_permission_flag_if_present(fs::perms::group_read, HasGroupRead);
    apply_permission_flag_if_present(fs::perms::group_write, HasGroupWrite);
    apply_permission_flag_if_present(fs::perms::group_exec, HasGroupExecute);
    apply_permission_flag_if_present(fs::perms::others_read, HasOtherRead);
    apply_permission_flag_if_present(fs::perms::others_write, HasOtherWrite);
    apply_permission_flag_if_present(fs::perms::others_exec, HasOtherExecute);
    apply_permission_flag_if_present(fs::perms::set_uid, IsUidBitSet);
    apply_permission_flag_if_present(fs::perms::set_gid, IsGidBitSet);
    apply_permission_flag_if_present(fs::perms::sticky_bit, IsStickyBitSet);
#endif

    Path parent        = this->parent();
    bool parent_exists = parent.fs_exists();
    this->safe([&parent] { parent.raise(); });

    if (parent_exists) flags = flags | ParentExists;

    return flags;
  }

  // check multiple status bits and combinations at once
  [[nodiscard]] bool check(Status flags) noexcept { return this->check(flags, this->symlink_mode.has(SymlinkMode::FollowForStatus)); }
  [[nodiscard]] bool check(Status flags, bool resolve_symlinks) noexcept { return this->check_any({flags}, resolve_symlinks); }
  [[nodiscard]] bool check_any(std::initializer_list<Status> combinations) noexcept {
    return this->check_any(combinations, this->symlink_mode.has(SymlinkMode::FollowForStatus));
  }
  [[nodiscard]] bool check_any(std::initializer_list<Status> combinations, bool resolve_symlinks) noexcept {
    Status current_status = this->status(resolve_symlinks);

    // return true if any one flag combination matches is fully supported by the current path status
    for (auto flags : combinations) {
      if (current_status.has_all(flags)) return true;
    }
    return false;
  }

  bool exists() noexcept { return this->exists(this->symlink_mode.has(SymlinkMode::FollowForStatus)); }
  bool not_found() noexcept { return this->not_found(this->symlink_mode.has(SymlinkMode::FollowForStatus)); }
  bool is_block_file() noexcept { return this->is_block_file(this->symlink_mode.has(SymlinkMode::FollowForStatus)); }
  bool is_character_file() noexcept { return this->is_character_file(this->symlink_mode.has(SymlinkMode::FollowForStatus)); }
  bool is_directory() noexcept { return this->is_directory(this->symlink_mode.has(SymlinkMode::FollowForStatus)); }
  bool is_empty() noexcept { return this->is_empty(this->symlink_mode.has(SymlinkMode::FollowForStatus)); }
  bool is_fifo() noexcept { return this->is_fifo(this->symlink_mode.has(SymlinkMode::FollowForStatus)); }
  bool is_other() noexcept { return this->is_other(this->symlink_mode.has(SymlinkMode::FollowForStatus)); }
  bool is_file() noexcept { return this->is_file(this->symlink_mode.has(SymlinkMode::FollowForStatus)); }
  bool is_socket() noexcept { return this->is_socket(this->symlink_mode.has(SymlinkMode::FollowForStatus)); }
  bool is_symlink() noexcept { return this->is_symlink(this->symlink_mode.has(SymlinkMode::FollowForStatus)); }
  bool is_unknown() noexcept { return this->is_unknown(this->symlink_mode.has(SymlinkMode::FollowForStatus)); }

  bool exists(bool resolve_symlinks) noexcept { return this->check(Exists, resolve_symlinks); }
  bool not_found(bool resolve_symlinks) noexcept { return this->check(NotFound, resolve_symlinks); }
  bool is_block_file(bool resolve_symlinks) noexcept { return this->check(IsBlock, resolve_symlinks); }
  bool is_character_file(bool resolve_symlinks) noexcept { return this->check(IsCharacter, resolve_symlinks); }
  bool is_directory(bool resolve_symlinks) noexcept { return this->check(IsDirectory, resolve_symlinks); }
  bool is_empty(bool resolve_symlinks) noexcept {
    // when not resolving symlinks, symlinks are not empty but a symlink
    if (!resolve_symlinks && this->is_symlink(resolve_symlinks)) return false;
    // otherwise, follow symlinks with default behavior
    return this->fs_is_empty();
  }
  bool is_fifo(bool resolve_symlinks) noexcept { return this->check(IsFifo, resolve_symlinks); }
  bool is_other(bool resolve_symlinks) noexcept {
    return this->exists(resolve_symlinks) && !this->check_any({IsFile, IsDirectory, IsSymlink}, resolve_symlinks);
  }
  bool is_file(bool resolve_symlinks) noexcept { return this->check(IsFile, resolve_symlinks); }
  bool is_socket(bool resolve_symlinks) noexcept { return this->check(IsSocket, resolve_symlinks); }
  bool is_symlink(bool resolve_symlinks) noexcept { return this->check(IsSymlink, resolve_symlinks); }
  bool is_unknown(bool resolve_symlinks) noexcept { return this->check(IsUnknown, resolve_symlinks); }

#ifdef __linux__
  bool is_readable() noexcept { return this->is_readable(this->symlink_mode.has(SymlinkMode::FollowForStatus)); }
  bool is_readable(bool resolve_symlinks) noexcept {
    Status status = this->status(resolve_symlinks);
    if (!status.has_all(Exists)) return false;

    uid_t file_owner_id = this->owner(resolve_symlinks);
    gid_t file_group_id = this->group(resolve_symlinks);

    uid_t process_user_id  = geteuid();
    gid_t process_group_id = getegid();

    // check file owner
    bool owner_id_matches = file_owner_id == process_user_id;
    if (owner_id_matches) return status.has_all(HasOwnerRead);

    // check file group
    bool group_id_matches = is_process_file_group_owner(file_group_id, process_group_id);
    if (group_id_matches) return status.has_all(HasGroupRead);

    // check file other permission
    return status.has_all(HasOtherRead);
  }

  bool is_writable() noexcept { return this->is_writable(this->symlink_mode.has(SymlinkMode::FollowForStatus)); }
  bool is_writable(bool resolve_symlinks) noexcept {
    Status status = this->status(resolve_symlinks);
    if (!status.has_all(Exists)) {
      Path parent             = this->parent();
      bool parent_is_writable = parent.is_writable(resolve_symlinks);
      this->safe([&parent] { parent.raise(); });
      return parent_is_writable;
    }

    uid_t file_owner_id = this->owner(resolve_symlinks);
    gid_t file_group_id = this->group(resolve_symlinks);

    uid_t process_user_id  = geteuid();
    gid_t process_group_id = getegid();

    // check file owner
    bool owner_id_matches = file_owner_id == process_user_id;
    if (owner_id_matches) return status.has_all(HasOwnerWrite);

    // check file group
    bool group_id_matches = is_process_file_group_owner(file_group_id, process_group_id);
    if (group_id_matches) return status.has_all(HasGroupWrite);

    // check file other permission
    return status.has_all(HasOtherWrite);
  }
#endif

  bool is_root() noexcept {
    bool result = false;
    this->safe([this, &result] { result = this->path() == this->path().root_directory(); });
    return result;
  }

  bool parent_exists() noexcept { return this->parent_exists(this->symlink_mode.has(SymlinkMode::FollowForStatus)); }
  bool parent_exists(bool resolve_symlinks) noexcept { return this->check(ParentExists, resolve_symlinks); }

  // =============================
  // === filesystem operations ===
  // =============================

  Path& copy(const Path& destination) noexcept {
    fs::copy_options options = fs::copy_options::overwrite_existing | fs::copy_options::recursive | fs::copy_options::copy_symlinks;
    return this->copy(destination, options);
  }

  Path& copy(const Path& destination, fs::copy_options options) noexcept { return this->fs_copy(destination, options); }

  Path& create_directory() noexcept { return this->fs_create_directories(); }

  Path& create_directory(fs::perms permissions) noexcept {
    this->create_directory();
    this->fs_permissions(permissions);
    return *this;
  }

  Path& create_parent() noexcept {
    Path parent = this->parent();
    parent.create_directory();
    safe([&parent] { parent.raise(); });
    return *this;
  }

  Path& create_parent(fs::perms permissions) noexcept {
    Path parent = this->parent();
    parent.create_directory(permissions);
    safe([&parent] { parent.raise(); });
    return *this;
  }

  Path& create_hard_link(const Path& destination) noexcept {
    Path resolved_destination = destination.clone();
    this->safe([&resolved_destination] { resolved_destination.raise(); });

    // overwrite existing file/directory at file link path
    this->remove(true);
    this->fs_create_hard_link(resolved_destination);

    return *this;
  }

#ifdef __linux__
  Path& create_symlink(const Path& destination) noexcept { return this->create_symlink(destination, false); }

  Path& create_symlink(const Path& destination, bool force_directory) noexcept {
    Path resolved_destination = destination.clone();
    this->safe([&resolved_destination] { resolved_destination.raise(); });

    // overwrite existing file/directory at the file link path
    this->remove(true);

    if (force_directory || resolved_destination.is_directory()) {
      this->fs_create_directory_symlink(resolved_destination);
    } else {
      this->fs_create_symlink(resolved_destination);
    }

    return *this;
  }
#endif

  Path& remove() noexcept { return this->remove(false); }
  Path& remove(bool recursive) noexcept {
    if (recursive) {
      this->fs_remove_all();
    } else {
      this->fs_remove();
    }

    return *this;
  }

  Path& rename(const Path& new_path) noexcept {
    Path resolved_new_path = new_path.clone();

    // overwrite existing file/directory at new path
    resolved_new_path.remove(true);

    this->safe([&resolved_new_path] { resolved_new_path.raise(); });

    this->fs_rename(resolved_new_path);
    return *this;
  }

#ifdef __linux__
  Path& set_owner(UserType user = NO_CHANGE, GroupType group = NO_CHANGE, bool recursive = false) noexcept {
    return this->set_owner(user, group, recursive, this->symlink_mode.has(SymlinkMode::FollowForStatus));
  }
  Path& set_owner(UserType user, GroupType group, bool recursive, bool resolve_symlinks) noexcept {
    uid_t uid = static_cast<uid_t>(-1);
    gid_t gid = static_cast<gid_t>(-1);

    // resolve user and group from ids, strings or NO_CHANGE (default: -1)
    this->safe([&user, &uid, &group, &gid] {
      if (std::holds_alternative<uid_t>(user)) {
        uid = std::get<uid_t>(user);
      } else if (std::holds_alternative<std::string>(user)) {
        const std::string& user_name = std::get<std::string>(user);

        if (!user_name.empty()) {
          auto* user_entry = getpwnam(user_name.c_str());
          if (!user_entry) throw std::runtime_error("Unknown user: " + user_name);
          uid = user_entry->pw_uid;
        }
      } else if (std::holds_alternative<NoOwnerChangeType>(user)) {
        // do nothing
      } else {
        throw std::runtime_error("Unknown user type");
      }

      if (std::holds_alternative<gid_t>(group)) {
        gid = std::get<gid_t>(group);
      } else if (std::holds_alternative<std::string>(group)) {
        const std::string& group_name = std::get<std::string>(group);

        if (!group_name.empty()) {
          auto* group_entry = getgrnam(group_name.c_str());
          if (!group_entry) throw std::runtime_error("Unknown group: " + group_name);
          gid = group_entry->gr_gid;
        }
      } else if (std::holds_alternative<NoOwnerChangeType>(group)) {
        // do nothing
      } else {
        throw std::runtime_error("Unknown group type");
      }
    });

    // if both user and group are NO_CHANGE, return
    if (uid == static_cast<uid_t>(-1) && gid == static_cast<gid_t>(-1)) return *this;

    int flags = resolve_symlinks ? 0 : AT_SYMLINK_NOFOLLOW;

    auto apply = [&uid, &gid, &flags](const char* path) { fchownat(AT_FDCWD, path, uid, gid, flags); };

    apply(this->path().c_str());

    if (!recursive) return *this;

    // apply recursively

    fs::directory_options options = resolve_symlinks ? fs::directory_options::follow_directory_symlink : fs::directory_options::none;

    this->safe([this, &apply, &options] {
      for (const auto& entry : fs::recursive_directory_iterator(this->path(), options)) {
        apply(entry.path().c_str());
      }
    });

    return *this;
  }
#endif

  /*
   * Set filesystem permissions on the path.
   *
   * Notes:
   * - When applying permissions recursively, do not forget that directories require executable permissions for traversing.
   * - The symbolic permission string does not support multiple operator groups like "u=rwx,g=rwx,o=rwx".
   * - The = operator of the symbolic permission string sets any permissions of non-specified targets to zero.
   */
  Path& set_permissions(PermissionType permission = 0664, PermissionOptionsType options = Replace, bool recursive = false) noexcept {
    fs::perms resolved_permissions    = fs::perms::none;
    fs::perm_options resolved_options = fs::perm_options::replace;

    // resolve permission options from PermissionMode, fs::perm_options or string

    if (std::holds_alternative<PermissionMode>(options)) {
      switch (std::get<PermissionMode>(options)) {
        case Replace:
          resolved_options = fs::perm_options::replace;
          break;
        case Add:
          resolved_options = fs::perm_options::add;
          break;
        case Remove:
          resolved_options = fs::perm_options::remove;
          break;
        default:
          this->spoil(std::runtime_error("Unknown permission mode"));
          break;
      }
    } else if (std::holds_alternative<fs::perm_options>(options)) {
      resolved_options = std::get<fs::perm_options>(options);
    } else if (std::holds_alternative<std::string>(options)) {
      const std::string& option = std::get<std::string>(options);
      if (option == "replace") {
        resolved_options = fs::perm_options::replace;
      } else if (option == "add") {
        resolved_options = fs::perm_options::add;
      } else if (option == "remove") {
        resolved_options = fs::perm_options::remove;
      }
    } else {
      this->spoil(std::runtime_error("Unknown permission options type"));
    }

    // resolve permission from Status, StatusFlag, fs::perms or string

    if (std::holds_alternative<Status>(permission)) {
      resolved_permissions = std::get<Status>(permission).fs_perms();
    } else if (std::holds_alternative<fs::perms>(permission)) {
      resolved_permissions = std::get<fs::perms>(permission);
    } else if (std::holds_alternative<int>(permission)) {
      resolved_permissions = static_cast<fs::perms>(std::get<int>(permission));
    } else if (std::holds_alternative<std::string>(permission)) {
      const std::string& permission_string = std::get<std::string>(permission);

      auto parsed          = symbolic_permission_string_to_fs_perms_and_options(permission_string);
      resolved_permissions = parsed.first;
      resolved_options     = parsed.second;
    } else {
      this->spoil(std::runtime_error("Unknown permission type"));
    }

    auto apply = [&resolved_permissions, &resolved_options](Path& path) { path.fs_permissions(resolved_permissions, resolved_options); };

    apply(*this);

    if (!recursive) return *this;

    // apply recursively

    this->safe([this, &apply] {
      for (const auto& entry : fs::recursive_directory_iterator(this->path())) {
        Path path = Path(entry.path());
        apply(path);
        path.raise();
      }
    });

    return *this;
  }

#pragma region fs_ functions
  //  ====================================================
  //  === wrapped std::filesystem (in-place) functions ===
  //  ====================================================

  Path& fs_absolute() noexcept {
    this->safe([this] { this->path_ = fs::absolute(this->path()); });
    return *this;
  }

  Path& fs_canonical() noexcept {
    this->safe([this] { this->path_ = fs::canonical(this->path()); });
    return *this;
  }

  Path& fs_weakly_canonical() noexcept {
    this->safe([this] { this->path_ = fs::weakly_canonical(this->path()); });
    return *this;
  }

  Path& fs_relative() noexcept {
    const Path base = Path::fs_current_path();
    this->safe([&base] { base.raise(); });
    return this->fs_relative(base);
  }

  Path& fs_relative(const Path& base) noexcept {
    this->safe([this, &base] { this->path_ = fs::relative(this->path(), base.path()); });
    return *this;
  }

  Path& fs_proximate() noexcept {
    const Path base = Path::fs_current_path();
    this->safe([&base] { base.raise(); });
    return this->fs_proximate(base);
  }

  Path& fs_proximate(const Path& base) noexcept {
    this->safe([this, &base] { this->path_ = fs::proximate(this->path(), base.path()); });
    return *this;
  }

  Path& fs_copy(const Path& destination) noexcept { return this->fs_copy(destination, fs::copy_options::none); }
  Path& fs_copy(const Path& destination, fs::copy_options options) noexcept {
    this->safe([this, &destination, &options] { fs::copy(this->path(), destination.path(), options); });
    return *this;
  }

  Path& fs_create_directory() noexcept {
    this->safe([this] { fs::create_directory(this->path()); });
    return *this;
  }

  Path& fs_create_directories() noexcept {
    this->safe([this] { fs::create_directories(this->path()); });
    return *this;
  }

  Path& fs_create_hard_link(const Path& destination) noexcept {
    this->safe([this, &destination] { fs::create_hard_link(destination.path(), this->path()); });
    return *this;
  }

  Path& fs_create_symlink(const Path& destination) noexcept {
    this->safe([this, &destination] { fs::create_symlink(destination.path(), this->path()); });
    return *this;
  }

  Path& fs_create_directory_symlink(const Path& destination) noexcept {
    this->safe([this, &destination] { fs::create_directory_symlink(destination.path(), this->path()); });
    return *this;
  }

  [[nodiscard]] static Path fs_current_path() noexcept {
    Path path;
    path.safe([&path] { path.path_ = fs::current_path(); });
    return path;
  }

  [[nodiscard]] bool fs_exists() noexcept {
    bool result = false;
    this->safe([this, &result] { result = fs::exists(this->path()); });
    return result;
  }

  [[nodiscard]] bool fs_equivalent(Path& other) noexcept {
    bool result = false;
    this->safe([this, &other, &result] { result = fs::equivalent(this->path(), other.path()); });
    return result;
  }

  [[nodiscard]] std::uintmax_t fs_file_size() noexcept {
    std::uintmax_t size = static_cast<uintmax_t>(-1);
    this->safe([this, &size] { size = fs::file_size(this->path()); });
    return size;
  }

  [[nodiscard]] std::uintmax_t fs_hard_link_count() noexcept {
    std::uintmax_t count = static_cast<uintmax_t>(-1);
    this->safe([this, &count] { count = fs::hard_link_count(this->path()); });
    return count;
  }

  [[nodiscard]] fs::file_time_type fs_last_write_time() noexcept {
    fs::file_time_type time;
    this->safe([this, &time] { time = fs::last_write_time(this->path()); });
    return time;
  }

  Path& fs_last_write_time(fs::file_time_type time) noexcept {
    this->safe([this, &time] { fs::last_write_time(this->path(), time); });
    return *this;
  }

  Path& fs_permissions(fs::perms permissions) noexcept {
    fs::perm_options options = fs::perm_options::replace;
    return this->fs_permissions(permissions, options);
  }

  Path& fs_permissions(fs::perms permissions, fs::perm_options options) noexcept {
    this->safe([this, &permissions, &options] { fs::permissions(this->path(), permissions, options); });
    return *this;
  }

  Path& fs_read_symlink() noexcept {
    this->safe([this] { this->path_ = fs::read_symlink(this->path()); });
    return *this;
  }

  Path& fs_remove() noexcept {
    this->safe([this] { fs::remove(this->path()); });
    return *this;
  }

  Path& fs_remove_all() noexcept {
    this->safe([this] { fs::remove_all(this->path()); });
    return *this;
  }

  Path& fs_rename(const Path& new_path) noexcept {
    this->safe([this, &new_path] {
      fs::rename(this->path(), new_path.path());
      this->path_ = new_path.path();
    });
    return *this;
  }

  Path& fs_resize_file(uintmax_t size) noexcept {
    this->safe([this, size] { fs::resize_file(this->path(), size); });
    return *this;
  }

  [[nodiscard]] fs::space_info fs_space() noexcept {
    fs::space_info space_info;
    this->safe([this, &space_info] { space_info = fs::space(this->path()); });
    return space_info;
  }

  [[nodiscard]] fs::file_status fs_status() noexcept {
    fs::file_status status;
    this->safe([this, &status] { status = fs::status(this->path()); });
    return status;
  }

  [[nodiscard]] fs::file_status fs_symlink_status() noexcept {
    fs::file_status status;
    this->safe([this, &status] { status = fs::symlink_status(this->path()); });
    return status;
  }

  [[nodiscard]] static Path fs_temp_directory_path() noexcept {
    Path path;
    path.safe([&path] { path.path_ = fs::temp_directory_path(); });
    return path;
  }
#pragma endregion fs_ wrapper
#pragma region fs_ type queries
  // ============================================
  // === wrapped std::filesystem type queries ===
  // ============================================

  [[nodiscard]] bool fs_is_block_file() noexcept {
    bool result = false;
    this->safe([this, &result] { result = fs::is_block_file(this->path()); });
    return result;
  }

  [[nodiscard]] bool fs_is_character_file() noexcept {
    bool result = false;
    this->safe([this, &result] { result = fs::is_character_file(this->path()); });
    return result;
  }

  [[nodiscard]] bool fs_is_directory() noexcept {
    bool result = false;
    this->safe([this, &result] { result = fs::is_directory(this->path()); });
    return result;
  }

  [[nodiscard]] bool fs_is_empty() noexcept {
    bool result = false;
    this->safe([this, &result] { result = fs::is_empty(this->path()); });
    return result;
  }

  [[nodiscard]] bool fs_is_fifo() noexcept {
    bool result = false;
    this->safe([this, &result] { result = fs::is_fifo(this->path()); });
    return result;
  }

  [[nodiscard]] bool fs_is_other() noexcept {
    bool result = false;
    this->safe([this, &result] { result = fs::is_other(this->path()); });
    return result;
  }

  [[nodiscard]] bool fs_is_regular_file() noexcept {
    bool result = false;
    this->safe([this, &result] { result = fs::is_regular_file(this->path()); });
    return result;
  }

  [[nodiscard]] bool fs_is_socket() noexcept {
    bool result = false;
    this->safe([this, &result] { result = fs::is_socket(this->path()); });
    return result;
  }

  [[nodiscard]] bool fs_is_symlink() noexcept {
    bool result = false;
    this->safe([this, &result] { result = fs::is_symlink(this->path()); });
    return result;
  }

  [[nodiscard]] bool fs_status_known() noexcept {
    bool result = false;
    this->safe([this, &result] { result = fs::status_known(fs::status(this->path())); });
    return result;
  }
#pragma endregion fs_ type queries
#pragma region fs_path functions
  // ===============================================
  // === wrapped std::filesystem::path functions ===
  // ===============================================

  Path& fs_clear() noexcept {
    this->path_.clear();  // is already noexcept
    return *this;
  }

  Path& fs_make_preferred() noexcept {
    this->safe([this] { this->path_.make_preferred(); });
    return *this;
  }

  Path& fs_remove_filename() noexcept {
    this->safe([this] { this->path_.remove_filename(); });
    return *this;
  }

  Path& fs_replace_filename(const Path& replacement) noexcept {
    this->safe([this, &replacement] { this->path_.replace_filename(replacement.path()); });
    return *this;
  }

  Path& fs_replace_extension() noexcept {
    const Path replacement = Path();
    this->safe([&replacement] { replacement.raise(); });
    return this->fs_replace_extension(replacement);
  }

  Path& fs_replace_extension(const Path& replacement) noexcept {
    this->safe([this, &replacement] { this->path_.replace_extension(replacement.path()); });
    return *this;
  }

  Path& fs_swap(Path& other) noexcept {
    this->path_.swap(other.path_);  // is already noexcept
    return *this;
  }

  const fs::path::value_type* fs_c_str() const noexcept {
    return this->path().c_str();  // is already noexcept
  }

  const fs::path::string_type& fs_native() const noexcept {
    return this->path().native();  // is already noexcept
  }

  int fs_compare(const Path& other) const noexcept {
    return this->path().compare(other.path());  // is already noexcept
  }

  Path& fs_lexically_normal() noexcept {
    this->safe([this] { this->path_ = this->path().lexically_normal(); });
    return *this;
  }

  Path& fs_lexically_relative(const Path& base) noexcept {
    this->safe([this, &base] { this->path_ = this->path().lexically_relative(base.path()); });
    return *this;
  }

  Path& fs_lexically_proximate(const Path& base) noexcept {
    this->safe([this, &base] { this->path_ = this->path().lexically_proximate(base.path()); });
    return *this;
  }

  Path& fs_root_name() noexcept {
    this->safe([this] { this->path_ = this->path().root_name(); });
    return *this;
  }

  Path& fs_root_directory() noexcept {
    this->safe([this] { this->path_ = this->path().root_directory(); });
    return *this;
  }

  Path& fs_root_path() noexcept {
    this->safe([this] { this->path_ = this->path().root_path(); });
    return *this;
  }

  Path& fs_relative_path() noexcept {
    this->safe([this] { this->path_ = this->path().relative_path(); });
    return *this;
  }

  Path& fs_parent_path() noexcept {
    this->safe([this] { this->path_ = this->path().parent_path(); });
    return *this;
  }

  Path& fs_filename() noexcept {
    this->safe([this] { this->path_ = this->path().filename(); });
    return *this;
  }

  Path& fs_stem() noexcept {
    this->safe([this] { this->path_ = this->path().stem(); });
    return *this;
  }

  Path& fs_extension() noexcept {
    this->safe([this] { this->path_ = this->path().extension(); });
    return *this;
  }
#pragma endregion fs_path functions
#pragma region fs_path queries
  // =============================================
  // === wrapped std::filesystem::path queries ===
  // =============================================

  bool fs_empty() noexcept {
    return this->path().empty();  // is already noexcept
  }

  bool fs_has_root_path() noexcept {
    bool result = false;
    this->safe([this, &result] { result = this->path().has_root_path(); });
    return result;
  }

  bool fs_has_root_name() noexcept {
    bool result = false;
    this->safe([this, &result] { result = this->path().has_root_name(); });
    return result;
  }

  bool fs_has_root_directory() noexcept {
    bool result = false;
    this->safe([this, &result] { result = this->path().has_root_directory(); });
    return result;
  }

  bool fs_has_relative_path() noexcept {
    bool result = false;
    this->safe([this, &result] { result = this->path().has_relative_path(); });
    return result;
  }

  bool fs_has_parent_path() noexcept {
    bool result = false;
    this->safe([this, &result] { result = this->path().has_parent_path(); });
    return result;
  }

  bool fs_has_filename() noexcept {
    bool result = false;
    this->safe([this, &result] { result = this->path().has_filename(); });
    return result;
  }

  bool fs_has_stem() noexcept {
    bool result = false;
    this->safe([this, &result] { result = this->path().has_stem(); });
    return result;
  }

  bool fs_has_extension() noexcept {
    bool result = false;
    this->safe([this, &result] { result = this->path().has_extension(); });
    return result;
  }

  bool fs_is_absolute() noexcept {
    bool result = false;
    this->safe([this, &result] { result = this->path().is_absolute(); });
    return result;
  }

  bool fs_is_relative() noexcept {
    bool result = false;
    this->safe([this, &result] { result = this->path().is_relative(); });
    return result;
  }
#pragma endregion fs_path queries

  // ========================
  // === error management ===
  // ========================

  void clear() noexcept {
    this->good_     = true;
    this->error_[0] = '\0';
  }
  [[nodiscard]] std::string_view error() const noexcept { return error_; }
  [[nodiscard]] bool good() const noexcept { return this->good_; }
  void raise() const {
    if (!this->good()) throw std::runtime_error(this->error_);
  }

 private:
  fs::path path_;
  SymlinkPolicy symlink_mode = SymlinkMode::FollowForStatus;

  // ========================
  // === error management ===
  // ========================

  bool good_                                  = true;
  static constexpr std::size_t MAX_ERROR_SIZE = 255;
  char error_[MAX_ERROR_SIZE + 1]             = {};

  template <typename Func>
  void safe(Func&& func) noexcept {
    try {
      func();
    } catch (std::exception& error) {
      this->spoil(error);
    } catch (...) {
      this->spoil(std::runtime_error("unknown exception"));
    }
  }

  void spoil(const std::exception& error) noexcept {
    // do not overwrite existing error
    if (!this->good()) return;

    this->good_ = false;

    // set error message without throwing
    // retrieve error message
    const char* error_message = error.what();
    if (!error_message) {
      strncpy(this->error_, "unknown exception", MAX_ERROR_SIZE);
      this->error_[MAX_ERROR_SIZE] = '\0';
      return;
    }

    // scan error message length up to max size
    size_t message_length = strnlen(error_message, MAX_ERROR_SIZE);

    // copy error message, size fits
    if (message_length < MAX_ERROR_SIZE) {
      strncpy(this->error_, error_message, message_length);
      this->error_[message_length] = '\0';
      return;
    }

    // copy truncated error message
    const char* truncation_warning = "... (truncated)";
    size_t warning_len             = strlen(truncation_warning);
    size_t copy_len                = MAX_ERROR_SIZE - warning_len;
    strncpy(this->error_, error_message, copy_len);
    strcpy(this->error_ + copy_len, truncation_warning);
  }
};

}  // namespace PathLib
