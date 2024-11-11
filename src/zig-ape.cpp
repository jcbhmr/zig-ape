#include <iostream>
#include <vector>
#include <filesystem>
#include <utility>
#include <string>
#include <cstdlib>
#include <stdexcept>
#include <sstream>
#include <cosmo.h>
#include <spawn.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <signal.h>

__static_yoink("zipos");

int main(int argc, char *argv[]);
std::filesystem::path user_cache_dir();
[[noreturn]] void my_exec(const std::filesystem::path& path, char *const argv[]);

int main(int argc, char *argv[]) {
  auto const cache_dir = user_cache_dir();
  if (!std::filesystem::exists(cache_dir)) {
    std::filesystem::create_directories(cache_dir);
    try {
      std::filesystem::copy("/zip/zig-common", cache_dir, std::filesystem::copy_options::recursive);
#if defined(__x86_64__)
      if (IsFreebsd()) {
        std::filesystem::copy_file("/zip/zig-freebsd-x86_64", cache_dir / "zig");
      } else if (IsXnu()) {
        std::filesystem::copy_file("/zip/zig-macos-x86_64", cache_dir / "zig");
      } else if (IsLinux()) {
        std::filesystem::copy_file("/zip/zig-linux-x86_64", cache_dir / "zig");
      } else if (IsWindows()) {
        std::filesystem::copy_file("/zip/zig-windows-x86_64.exe", cache_dir / "zig.exe");
      } else {
        throw std::runtime_error("unsupported os");
      }
#elif defined(__aarch64__)
      if (IsXnu()) {
        std::filesystem::copy_file("/zip/zig-macos-aarch64", cache_dir / "zig");
      } else if (IsLinux()) {
        std::filesystem::copy_file("/zip/zig-linux-aarch64", cache_dir / "zig");
      } else if (IsWindows()) {
        std::filesystem::copy_file("/zip/zig-windows-aarch64.exe", cache_dir / "zig.exe");
      } else {
        throw std::runtime_error("unsupported os");
      }
#else
#error "unsupported arch"
#endif
    } catch (...) {
      std::filesystem::remove_all(cache_dir);
      throw;
    }
  }
  my_exec(cache_dir / (IsWindows() ? "zig.exe" : "zig"), argv);
}

std::filesystem::path user_cache_dir() {
  auto const appname = std::string("zig");
  auto const appauthor = std::string("ziglang");
  auto const version = std::string(TAGVER);
  if (IsWindows()) {
    auto const localappdata_c_str = std::getenv("LOCALAPPDATA");
    if (!localappdata_c_str) {
      throw std::runtime_error("LOCALAPPDATA not set");
    }
    auto const localappdata = std::filesystem::path(localappdata_c_str);
    return localappdata / appauthor / appname / "Cache" / version;
  } else if (IsXnu()) {
    auto const home_c_str = std::getenv("HOME");
    if (!home_c_str) {
      throw std::runtime_error("HOME not set");
    }
    auto const home = std::filesystem::path(home_c_str);
    return home / "Library/Caches" / appname / version;
  } else {
    std::filesystem::path xdg_cache_home;
    if (auto const xdg_cache_home_c_str = std::getenv("XDG_CACHE_HOME")) {
      xdg_cache_home = std::filesystem::path(xdg_cache_home_c_str);
    } else {
      auto const home_c_str = std::getenv("HOME");
      if (!home_c_str) {
        throw std::runtime_error("HOME not set");
      }
      auto const home = std::filesystem::path(home_c_str);
      xdg_cache_home = home / ".cache";
    }
    return xdg_cache_home / appname / version;
  }
}

[[noreturn]] void my_exec(const std::filesystem::path& path, char *const argv[]) {
  if (IsWindows()) {
    pid_t pid;
    auto const err = posix_spawn(&pid, path.c_str(), nullptr, nullptr, argv, nullptr);
    if (err) {
      std::ostringstream oss;
      oss << "posix_spawn() " << std::strerror(err);
      throw std::runtime_error(oss.str());
    }

    int status;
    while (waitpid(pid, &status, 0) != -1);
    // TODO: Does this handle everything properly?
    if (WIFEXITED(status)) {
      std::exit(WEXITSTATUS(status));
    } else {
      std::ostringstream oss;
      oss << "waitpid() " << status;
      throw std::runtime_error(oss.str());
    }
  } else {
    auto const err = execv(path.c_str(), argv);
    if (err) {
      std::ostringstream oss;
      oss << "execv() " << std::strerror(err);
      throw std::runtime_error(oss.str());
    }
    std::unreachable();
  }
}
