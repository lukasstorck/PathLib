#include <doctest/doctest.h>

#include <PathLib/PathLib.hpp>
#include <filesystem>

namespace fs = std::filesystem;

TEST_CASE("fs_path basic creation and string conversion match std::filesystem") {
  PathLib::Path p1;  // default constructor
  REQUIRE(p1.fs_empty());

  fs::path native = "test/path/file.txt";
  PathLib::Path p2(native);
  REQUIRE(p2.string() == native.string());

  PathLib::Path p3("test/path/file.txt");
  REQUIRE(p3.string() == native.string());

  PathLib::Path p4(std::string_view("test/path/file.txt"));
  REQUIRE(p4.string() == native.string());

  REQUIRE(p1.good());
  REQUIRE(p2.good());
  REQUIRE(p3.good());
  REQUIRE(p4.good());
}

#pragma region fs_ functions
TEST_CASE("path resolution function match std::filesystem") {
  // ===== fs_absolute =====
  PathLib::Path p(".");
  fs::path native_abs = fs::absolute(".");

  p.fs_absolute();
  REQUIRE(p.good());
  REQUIRE(p.string() == native_abs.string());

  // ===== fs_canonical =====
  fs::path existing = fs::temp_directory_path();
  PathLib::Path q(existing);

  fs::path canonical_native = fs::canonical(existing);
  q.fs_canonical();

  REQUIRE(q.good());
  REQUIRE(q.string() == canonical_native.string());

  // ===== fs_weakly_canonical =====
  PathLib::Path r("nonexistent_path/..");
  fs::path weak_native = fs::weakly_canonical("nonexistent_path/..");

  r.fs_weakly_canonical();

  REQUIRE(r.good());
  REQUIRE(r.string() == weak_native.string());

  // ===== fs_relative (no base overload) =====
  fs::path current = fs::current_path();
  fs::path target  = current / "some_child_dir";

  PathLib::Path relative1(target);
  fs::path native_relative1 = fs::relative(target, current);

  relative1.fs_relative();  // internally uses current_path()
  REQUIRE(relative1.good());
  REQUIRE(relative1.string() == native_relative1.string());

  // ===== fs_relative (explicit base overload) =====
  fs::path base  = current;
  fs::path child = base / "nested/file.txt";

  PathLib::Path relative2(child);
  PathLib::Path base_path(base);

  fs::path native_relative2 = fs::relative(child, base);

  relative2.fs_relative(base_path);
  REQUIRE(relative2.good());
  REQUIRE(relative2.string() == native_relative2.string());

  // ===== fs_proximate (no base overload) =====
  PathLib::Path proximate1(target);
  fs::path native_proximate1 = fs::proximate(target, current);

  relative1.fs_proximate();  // internally uses current_path()
  REQUIRE(relative1.good());
  REQUIRE(relative1.string() == native_relative1.string());

  // ===== fs_proximate (explicit base overload) =====
  PathLib::Path proximate2(child);
  fs::path native_proximate2 = fs::proximate(child, base);

  relative2.fs_proximate(base_path);
  REQUIRE(relative2.good());
  REQUIRE(relative2.string() == native_relative2.string());
}

TEST_CASE("environment and status functions match std::filesystem") {
  // --- current_path ---
  PathLib::Path p = PathLib::Path::fs_current_path();
  fs::path native = fs::current_path();
  REQUIRE(p.good());
  REQUIRE(p.string() == native.string());

  // --- temp_directory_path ---
  PathLib::Path temp   = PathLib::Path::fs_temp_directory_path();
  fs::path temp_native = fs::temp_directory_path();
  REQUIRE(temp.good());
  REQUIRE(temp.string() == temp_native.string());

  // --- exists (existing) ---
  PathLib::Path existing(temp_native);
  REQUIRE(existing.fs_exists());
  REQUIRE(existing.good());

  // --- exists (non-existing) ---
  PathLib::Path nonexisting(temp_native / "nonexistent_file.txt");
  REQUIRE_FALSE(nonexisting.fs_exists());
  REQUIRE(nonexisting.good());

  // --- status / symlink_status ---
  fs::file_status status_native  = fs::status(temp_native);
  fs::file_status symlink_native = fs::symlink_status(temp_native);

  fs::file_status status_wrapped  = existing.fs_status();
  fs::file_status symlink_wrapped = existing.fs_symlink_status();

  REQUIRE(existing.good());
  REQUIRE(status_wrapped.type() == status_native.type());
  REQUIRE(symlink_wrapped.type() == symlink_native.type());

  // --- equivalent ---
  PathLib::Path p1(temp_native);
  PathLib::Path p2(temp_native);
  REQUIRE(p1.fs_equivalent(p2) == fs::equivalent(temp_native, temp_native));
  REQUIRE(p1.good());
}

TEST_CASE("metadata functions match std::filesystem") {
  fs::path temp_dir     = fs::temp_directory_path();
  fs::path file         = temp_dir / "pathlib_test_file.txt";
  fs::path symlink_file = temp_dir / "pathlib_symlink.txt";

  // --- create a file ---
  {
    std::ofstream ofs(file);
    ofs << "hello world";
  }

  PathLib::Path p(file);

  // --- file_size ---
  REQUIRE(p.fs_file_size() == fs::file_size(file));
  REQUIRE(p.good());

  // --- hard_link_count ---
  REQUIRE(p.fs_hard_link_count() == fs::hard_link_count(file));
  REQUIRE(p.good());

  // --- last_write_time getter & setter ---
  auto new_time = fs::file_time_type::clock::now();
  p.fs_last_write_time(new_time);
  REQUIRE(p.good());
  REQUIRE(fs::last_write_time(file) == new_time);

  // --- fs_permissions with default replace ---
  p.fs_permissions(fs::perms::owner_read | fs::perms::owner_write);
  REQUIRE(p.good());
  fs::perms perms_native = fs::status(file).permissions();
  REQUIRE((perms_native & (fs::perms::owner_read | fs::perms::owner_write)) == (fs::perms::owner_read | fs::perms::owner_write));

  // --- fs_permissions with explicit options (add) ---
  p.fs_permissions(fs::perms::group_read, fs::perm_options::add);
  REQUIRE(p.good());
  perms_native = fs::status(file).permissions();
  REQUIRE((perms_native & fs::perms::group_read) == fs::perms::group_read);

  // --- fs_permissions with explicit options (remove) ---
  p.fs_permissions(fs::perms::owner_write, fs::perm_options::remove);
  REQUIRE(p.good());
  perms_native = fs::status(file).permissions();
  REQUIRE((perms_native & fs::perms::owner_write) == fs::perms::none);

  // --- read_symlink ---
  fs::create_symlink(file, symlink_file);
  PathLib::Path link(symlink_file);
  link.fs_read_symlink();
  REQUIRE(link.good());
  REQUIRE(link.string() == file.string());

  // --- space ---
  PathLib::Path dir(temp_dir);
  fs::space_info native_space  = fs::space(temp_dir);
  fs::space_info wrapped_space = dir.fs_space();
  REQUIRE(dir.good());
  REQUIRE(wrapped_space.capacity == native_space.capacity);
  REQUIRE(wrapped_space.free == native_space.free);
  REQUIRE(wrapped_space.available == native_space.available);

  // --- cleanup ---
  fs::remove(symlink_file);
  fs::remove(file);
}

TEST_CASE("fs_copy / fs_create_* / fs_create_hard_link / fs_create_symlink") {
  fs::path temp_dir = fs::temp_directory_path() / "pathlib_test_dir";
  fs::create_directories(temp_dir);

  PathLib::Path dir(temp_dir);
  PathLib::Path file(temp_dir / "file.txt");

  // create a test file
  {
    std::ofstream ofs(file.path());
    ofs << "hello world";
  }

  // --- fs_create_directory ---
  PathLib::Path new_dir = dir.clone().fs_create_directory();  // already exists, no error
  REQUIRE(new_dir.good());

  // --- fs_create_directories ---
  PathLib::Path nested_dir(temp_dir / "nested/dir");
  nested_dir.fs_create_directories();
  REQUIRE(nested_dir.good());
  REQUIRE(fs::exists(nested_dir.path()));

  // --- fs_copy ---
  PathLib::Path copy_file(temp_dir / "file_copy.txt");
  file.fs_copy(copy_file);
  REQUIRE(copy_file.good());
  REQUIRE(fs::exists(copy_file.path()));

  // --- fs_create_hard_link ---
  PathLib::Path hardlink_file(temp_dir / "file_hardlink.txt");
  hardlink_file.fs_create_hard_link(file);
  REQUIRE(hardlink_file.good());
  REQUIRE(fs::exists(hardlink_file.path()));
  REQUIRE(fs::hard_link_count(file.path()) >= 2);

  // --- fs_create_symlink ---
  PathLib::Path symlink_file(temp_dir / "file_symlink.txt");
  symlink_file.fs_create_symlink(file);
  REQUIRE(symlink_file.good());
  REQUIRE(fs::is_symlink(symlink_file.path()));

  // --- fs_create_directory_symlink ---
  PathLib::Path dir_symlink(temp_dir / "dir_symlink");
  dir_symlink.fs_create_directory_symlink(dir);
  REQUIRE(dir_symlink.good());
  REQUIRE(fs::is_symlink(dir_symlink.path()));

  // cleanup
  fs::remove_all(temp_dir);
}

TEST_CASE("fs_rename") {
  fs::path temp_dir = fs::temp_directory_path() / "pathlib_rename_test";
  fs::create_directories(temp_dir);

  PathLib::Path original(temp_dir / "original.txt");
  {
    std::ofstream ofs(original.path());
    ofs << "rename test";
  }

  PathLib::Path renamed(temp_dir / "renamed.txt");
  original.fs_rename(renamed);
  REQUIRE(original.good());
  REQUIRE(fs::exists(renamed.path()));
  REQUIRE_FALSE(fs::exists(temp_dir / "original.txt"));

  fs::remove_all(temp_dir);
}

TEST_CASE("fs_resize_file") {
  fs::path temp_file = fs::temp_directory_path() / "pathlib_resize_test.txt";
  {
    std::ofstream ofs(temp_file);
    ofs << "1234567890";
  }

  PathLib::Path file(temp_file);
  file.fs_resize_file(5);
  REQUIRE(file.good());
  REQUIRE(fs::file_size(file.path()) == 5);

  fs::remove(temp_file);
}

TEST_CASE("fs_remove / fs_remove_all") {
  fs::path temp_dir = fs::temp_directory_path() / "pathlib_remove_test";
  fs::create_directories(temp_dir);

  PathLib::Path dir(temp_dir);
  PathLib::Path file(temp_dir / "file.txt");

  {
    std::ofstream ofs(file.path());
    ofs << "delete me";
  }

  // --- fs_remove ---
  file.fs_remove();
  REQUIRE(file.good());
  REQUIRE_FALSE(fs::exists(file.path()));

  // --- fs_remove_all ---
  dir.fs_remove_all();
  REQUIRE(dir.good());
  REQUIRE_FALSE(fs::exists(dir.path()));
}

#pragma endregion fs_ wrapper
#pragma region fs_ type queries
TEST_CASE("type queries match std::filesystem") {
  // prepare temp directory and files
  fs::path temp_dir = fs::temp_directory_path() / "pathlib_type_test";
  fs::create_directories(temp_dir);

  PathLib::Path dir(temp_dir);
  PathLib::Path empty_file(temp_dir / "empty.txt");
  PathLib::Path non_empty_file(temp_dir / "file.txt");
  PathLib::Path symlink_file(temp_dir / "symlink.txt");

  // create files
  {
    std::ofstream ofs(non_empty_file.path());
    ofs << "content";
  }
  {
    std::ofstream ofs(empty_file.path());
  }

  // create symlink (if possible)
  try {
    symlink_file.fs_create_symlink(non_empty_file);
  } catch (...) {
  }  // ignore if symlink creation fails

  // --- directory ---
  REQUIRE(dir.fs_is_directory());
  REQUIRE_FALSE(dir.fs_is_regular_file());
  REQUIRE_FALSE(dir.fs_is_symlink());

  // --- regular files ---
  REQUIRE(empty_file.fs_is_regular_file());
  REQUIRE(empty_file.fs_is_empty());
  REQUIRE_FALSE(empty_file.fs_is_directory());
  REQUIRE_FALSE(empty_file.fs_is_symlink());

  REQUIRE(non_empty_file.fs_is_regular_file());
  REQUIRE_FALSE(non_empty_file.fs_is_empty());

  // --- symlink ---
  if (fs::exists(symlink_file.path())) {
    REQUIRE(symlink_file.fs_is_symlink());
    REQUIRE(symlink_file.fs_status_known());
  }

  // --- negative tests  ---
  REQUIRE_FALSE(dir.fs_is_block_file());
  REQUIRE_FALSE(dir.fs_is_character_file());
  REQUIRE_FALSE(dir.fs_is_fifo());
  REQUIRE_FALSE(dir.fs_is_socket());

  REQUIRE_FALSE(non_empty_file.fs_is_block_file());
  REQUIRE_FALSE(non_empty_file.fs_is_character_file());
  REQUIRE_FALSE(non_empty_file.fs_is_fifo());
  REQUIRE_FALSE(non_empty_file.fs_is_socket());

  // --- empty path ---
  PathLib::Path empty_path;
  REQUIRE(empty_path.fs_status_known());  // path exists? false
  REQUIRE_FALSE(empty_path.fs_is_regular_file());
  REQUIRE_FALSE(empty_path.fs_is_directory());
  REQUIRE_FALSE(empty_path.fs_is_block_file());
  REQUIRE_FALSE(empty_path.fs_is_character_file());
  REQUIRE_FALSE(empty_path.fs_is_fifo());
  REQUIRE_FALSE(empty_path.fs_is_socket());

  // cleanup
  fs::remove_all(temp_dir);
}
#pragma endregion fs_ type queries
#pragma region fs_path functions
void test_path_components(const PathLib::Path& p, const fs::path& native) {
  auto cloned = p.clone().fs_root_name();
  REQUIRE(cloned.good());
  REQUIRE(cloned.string() == native.root_name().string());

  cloned = p.clone().fs_root_directory();
  REQUIRE(cloned.good());
  REQUIRE(cloned.string() == native.root_directory().string());

  cloned = p.clone().fs_root_path();
  REQUIRE(cloned.good());
  REQUIRE(cloned.string() == native.root_path().string());

  cloned = p.clone().fs_relative_path();
  REQUIRE(cloned.good());
  REQUIRE(cloned.string() == native.relative_path().string());

  cloned = p.clone().fs_parent_path();
  REQUIRE(cloned.good());
  REQUIRE(cloned.string() == native.parent_path().string());

  cloned = p.clone().fs_filename();
  REQUIRE(cloned.good());
  REQUIRE(cloned.string() == native.filename().string());

  cloned = p.clone().fs_stem();
  REQUIRE(cloned.good());
  REQUIRE(cloned.string() == native.stem().string());

  cloned = p.clone().fs_extension();
  REQUIRE(cloned.good());
  REQUIRE(cloned.string() == native.extension().string());
}

TEST_CASE("path functions match std::filesystem") {
  // -----------------------------
  // fs_clear / fs_make_preferred / fs_remove_filename / fs_replace_filename / fs_replace_extension / fs_swap
  // -----------------------------
  {
    PathLib::Path p1("folder/sub/file.txt");
    PathLib::Path p2("other/path");

    // fs_clear
    p1.fs_clear();
    REQUIRE(p1.string().empty());

    // fs_make_preferred
    PathLib::Path p3("folder\\sub\\file.txt");
    std::string expected = fs::path("folder\\sub\\file.txt").make_preferred().string();
    p3.fs_make_preferred();
    REQUIRE(p3.string() == expected);

    // fs_remove_filename
    PathLib::Path p4("folder/sub/file.txt");
    p4.fs_remove_filename();
    REQUIRE(p4.string() == "folder/sub/");

    // fs_replace_filename
    PathLib::Path p5("folder/sub/file.txt");
    PathLib::Path replacement("newfile.bin");
    p5.fs_replace_filename(replacement);
    REQUIRE(p5.string() == "folder/sub/newfile.bin");

    // fs_replace_extension (default empty)
    PathLib::Path p6("folder/file.txt");
    p6.fs_replace_extension();
    REQUIRE(p6.string() == "folder/file");

    // fs_replace_extension (custom)
    PathLib::Path ext_replacement(".dat");
    PathLib::Path p7("folder/file.txt");
    p7.fs_replace_extension(ext_replacement);
    REQUIRE(p7.string() == "folder/file.dat");

    // fs_swap
    PathLib::Path a("a/b.txt");
    PathLib::Path b("c/d.txt");
    a.fs_swap(b);
    REQUIRE(a.string() == "c/d.txt");
    REQUIRE(b.string() == "a/b.txt");
  }

  // -----------------------------
  // fs_c_str / fs_native / fs_compare
  // -----------------------------
  {
    PathLib::Path p1("folder/file.txt");
    PathLib::Path p2("folder/file.txt");
    PathLib::Path p3("folder/other.txt");

    REQUIRE(std::string(p1.fs_c_str()) == p1.string());
    REQUIRE(p1.fs_native() == p1.path().native());

    REQUIRE(p1.fs_compare(p2) == 0);
    REQUIRE(p1.fs_compare(p3) != 0);
  }

  // -----------------------------
  // fs_lexically_normal / fs_lexically_relative / fs_lexically_proximate
  // -----------------------------
  {
    PathLib::Path p1("folder/../folder2/./file.txt");
    p1.fs_lexically_normal();
    REQUIRE(p1.string() == "folder2/file.txt");

    PathLib::Path base("folder2");
    PathLib::Path p2("folder2/file.txt");
    p2.fs_lexically_relative(base);
    REQUIRE(p2.string() == "file.txt");

    PathLib::Path base2(fs::current_path());
    PathLib::Path p3(fs::current_path() / "subdir/file.txt");
    p3.fs_lexically_proximate(base2);
    REQUIRE(p3.string() == "subdir/file.txt");
  }

  // -----------------------------
  // fs_root_name / fs_root_directory / fs_root_path / fs_relative_path / fs_parent_path / fs_filename / fs_stem / fs_extension
  // -----------------------------
  {
    fs::path native_windows_style("C:/folder/sub/file.txt");
    PathLib::Path path_windows_style(native_windows_style);
    test_path_components(path_windows_style, native_windows_style);

    fs::path native_unix_style("/home/user/sub/file.txt");
    PathLib::Path path_unix_style(native_unix_style);
    test_path_components(path_unix_style, native_unix_style);
  }
}
#pragma endregion fs_path functions
#pragma region fs_path queries
TEST_CASE("fs_path queries match std::filesystem") {
  fs::path native = "test/path/file.txt";
  PathLib::Path p(native);

  REQUIRE(p.fs_empty() == native.empty());
  REQUIRE(p.fs_has_root_path() == native.has_root_path());
  REQUIRE(p.fs_has_root_name() == native.has_root_name());
  REQUIRE(p.fs_has_root_directory() == native.has_root_directory());
  REQUIRE(p.fs_has_relative_path() == native.has_relative_path());
  REQUIRE(p.fs_has_parent_path() == native.has_parent_path());
  REQUIRE(p.fs_has_filename() == native.has_filename());
  REQUIRE(p.fs_has_stem() == native.has_stem());
  REQUIRE(p.fs_has_extension() == native.has_extension());
  REQUIRE(p.fs_is_absolute() == native.is_absolute());
  REQUIRE(p.fs_is_relative() == native.is_relative());
}
#pragma endregion fs_path queries
