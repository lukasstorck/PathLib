#pragma once

#include <bitset>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <utility>

namespace PathLib {

namespace fs = std::filesystem;

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

 private:
  constexpr explicit Status(unsigned bits) noexcept : bits_(bits) {}
  unsigned bits_ = 0;
};

// expose flags so that they are accessible as PathLib::IsFile

inline constexpr Status NotFound        = StatusFlag::NotFound;
inline constexpr Status Nonexistent     = StatusFlag::NotFound;
inline constexpr Status Exists          = StatusFlag::Exists;
inline constexpr Status IsFile          = StatusFlag::IsFile;
inline constexpr Status IsRegular       = StatusFlag::IsFile;
inline constexpr Status IsDirectory     = StatusFlag::IsDirectory;
inline constexpr Status IsSymlink       = StatusFlag::IsSymlink;
inline constexpr Status IsBlock         = StatusFlag::IsBlock;
inline constexpr Status IsCharacter     = StatusFlag::IsCharacter;
inline constexpr Status IsFifo          = StatusFlag::IsFifo;
inline constexpr Status IsSocket        = StatusFlag::IsSocket;
inline constexpr Status IsUnknown       = StatusFlag::IsUnknown;
inline constexpr Status HasOwnerRead    = StatusFlag::HasOwnerRead;
inline constexpr Status HasOwnerWrite   = StatusFlag::HasOwnerWrite;
inline constexpr Status HasOwnerExecute = StatusFlag::HasOwnerExecute;
inline constexpr Status HasOwnerAll     = HasOwnerRead | HasOwnerWrite | HasOwnerExecute;
inline constexpr Status HasGroupRead    = StatusFlag::HasGroupRead;
inline constexpr Status HasGroupWrite   = StatusFlag::HasGroupWrite;
inline constexpr Status HasGroupExecute = StatusFlag::HasGroupExecute;
inline constexpr Status HasGroupAll     = HasGroupRead | HasGroupWrite | HasGroupExecute;
inline constexpr Status HasOtherRead    = StatusFlag::HasOtherRead;
inline constexpr Status HasOtherWrite   = StatusFlag::HasOtherWrite;
inline constexpr Status HasOtherExecute = StatusFlag::HasOtherExecute;
inline constexpr Status HasOtherAll     = HasOtherRead | HasOtherWrite | HasOtherExecute;
inline constexpr Status IsUidBitSet     = StatusFlag::IsUidBitSet;
inline constexpr Status IsGidBitSet     = StatusFlag::IsGidBitSet;
inline constexpr Status IsStickyBitSet  = StatusFlag::IsStickyBitSet;
inline constexpr Status ParentExists    = StatusFlag::ParentExists;

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
  Path(std::string_view path_view) : path_(path_view) {}
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

  Path& resolve_environment_variables() noexcept {
    // TODO
    this->safe([] { throw std::runtime_error("not implemented"); });
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
  [[nodiscard]] Path resolved_environment_variables_copy() const noexcept { return this->clone().resolve_environment_variables(); }

  // =================================
  // === file system object status ===
  // =================================

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

  bool is_writable() noexcept {
    // TODO
    this->safe([] { throw std::runtime_error("not implemented"); });
    return false;
    // std::ofstream file_stream(this->path_, std::ios::app);
    // return file_stream.is_open();
  }

  bool is_readable() noexcept {
    // TODO
    this->safe([] { throw std::runtime_error("not implemented"); });
    return false;
  }

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

  // TODO: add tests, also add function to check permissions
  Path& permissions() noexcept {
    fs::perms permissions = fs::perms::owner_read | fs::perms::owner_write | fs::perms::group_read | fs::perms::others_read;
    return this->permissions(permissions);
  }

  Path& permissions(fs::perms permissions) noexcept {
    fs::perm_options options = fs::perm_options::replace | fs::perm_options::nofollow;
    return this->permissions(permissions, options);
  }

  Path& permissions(fs::perms permissions, fs::perm_options options) noexcept { return this->fs_permissions(permissions, options); }

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
