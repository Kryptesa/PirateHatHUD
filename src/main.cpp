#include "pattern_scan.hpp"
#include "patterns.hpp"
#include "overlay.hpp"
#include <windows.h>

#include <safetyhook.hpp>

#include <atomic>
#include <cstdint>
#include <fstream>
#include <mutex>
#include <sstream>
#include <string>
#include <utility>

namespace {

constexpr char kVersion[] = "0.3.0-dx12-preview";

HMODULE g_self{};
std::wstring g_folder;
std::ofstream g_log;
std::mutex g_log_mutex;
safetyhook::MidHook g_enter, g_leave;

std::atomic<std::uintptr_t> g_state_base{0};
std::atomic<std::uint64_t> g_hook_events{0};
std::atomic<std::uint32_t> g_pre_state{UINT32_MAX};
std::atomic<bool> g_enabled{true}, g_stopping{false};

int g_x = 350, g_y = -310;
float g_scale = 1.0f;
int g_toggle_key = VK_F9, g_unload_key = VK_F10;

void log(const char* text) {
  std::lock_guard lock(g_log_mutex);
  if (g_log) { g_log << text << '\n'; g_log.flush(); }
}

int key_from_name(const wchar_t* value, int fallback) {
  if (_wcsicmp(value, L"F8") == 0) return VK_F8;
  if (_wcsicmp(value, L"F9") == 0) return VK_F9;
  if (_wcsicmp(value, L"F10") == 0) return VK_F10;
  if (_wcsicmp(value, L"F11") == 0) return VK_F11;
  return fallback;
}

void read_config() {
  const auto path = g_folder + L"config.ini";
  g_x = GetPrivateProfileIntW(L"indicator", L"x", 350, path.c_str());
  g_y = GetPrivateProfileIntW(L"indicator", L"y", -310, path.c_str());
  g_scale = GetPrivateProfileIntW(L"indicator", L"scale_percent", 100, path.c_str()) / 100.0f;
  if (g_scale < 0.25f || g_scale > 4.0f) g_scale = 1.0f;
  g_enabled = GetPrivateProfileIntW(L"indicator", L"enabled", 1, path.c_str()) != 0;
  wchar_t key[16]{};
  GetPrivateProfileStringW(L"hotkeys", L"toggle", L"F9", key, 16, path.c_str());
  g_toggle_key = key_from_name(key, VK_F9);
  GetPrivateProfileStringW(L"hotkeys", L"unload", L"F10", key, 16, path.c_str());
  g_unload_key = key_from_name(key, VK_F10);
}

void capture_state_before_instruction(safetyhook::Context& ctx) {
  std::uint32_t value = UINT32_MAX;
  if (ctx.rsi && ctx.rsi <= UINTPTR_MAX - phi::patterns::kStateOffset - sizeof(value)) {
    __try {
      value = *reinterpret_cast<volatile const std::uint32_t*>(ctx.rsi + phi::patterns::kStateOffset);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
      value = UINT32_MAX;
    }
  }
  g_pre_state.store(value, std::memory_order_relaxed);
  g_state_base.store(ctx.rsi, std::memory_order_release);
  g_hook_events.fetch_add(1, std::memory_order_release);
}
void __fastcall on_enter(safetyhook::Context& ctx) { capture_state_before_instruction(ctx); }
void __fastcall on_leave(safetyhook::Context& ctx) { capture_state_before_instruction(ctx); }

bool detected() {
  auto base = g_state_base.load(std::memory_order_acquire);
  if (!base || base > UINTPTR_MAX - phi::patterns::kStateOffset - sizeof(std::uint32_t)) return false;
  const auto address = base + phi::patterns::kStateOffset;
  MEMORY_BASIC_INFORMATION info{};
  if (!VirtualQuery(reinterpret_cast<void*>(address), &info, sizeof info) ||
      info.State != MEM_COMMIT || (info.Protect & (PAGE_GUARD | PAGE_NOACCESS)) ||
      address + sizeof(std::uint32_t) > reinterpret_cast<std::uintptr_t>(info.BaseAddress) + info.RegionSize) {
    g_state_base.compare_exchange_strong(base, 0);
    return false;
  }
  __try {
    return *reinterpret_cast<volatile const std::uint32_t*>(address) > 0;
  } __except (EXCEPTION_EXECUTE_HANDLER) {
    g_state_base.store(0);
    return false;
  }
}

DWORD WINAPI worker(void*) {
  wchar_t path[MAX_PATH]{};
  GetModuleFileNameW(g_self, path, MAX_PATH);
  g_folder = path;
  g_folder.resize(g_folder.find_last_of(L"\\/") + 1);
  g_log.open(g_folder + L"PirateHatHUD.log", std::ios::app);
  log(kVersion);
  read_config();
  if (!phi::prepare_overlay_icon((g_folder + L"icon.png").c_str())) {
    log("Required icon.png missing or invalid; mod not started");
    g_log.close();
    FreeLibraryAndExitThread(g_self, 0);
    return 0;
  }
  phi::set_overlay_enabled(g_enabled.load());
  phi::set_overlay_position(g_x, g_y, g_scale);
  const auto config_path = g_folder + L"config.ini";
  phi::set_overlay_force(GetPrivateProfileIntW(L"indicator", L"force_show", 0, config_path.c_str()) != 0);
  phi::set_overlay_log(log);
  const bool overlay_started = phi::start_overlay();
  log(overlay_started ? "DX12 hooks installed; waiting for swapchain" : "DX12 hooks unavailable; overlay disabled");
  const auto scan = phi::find_hook_sites(GetModuleHandleW(L"CrimsonDesert.exe"));
  bool observing = false;
  if (scan.status != phi::ScanStatus::found) {
    const char* reason = scan.status == phi::ScanStatus::ambiguous ? "ambiguous" :
      scan.status == phi::ScanStatus::no_match ? "no pair" : "invalid image";
    log((std::string("State hooks disabled: ") + reason).c_str());
  } else {
    std::ostringstream found;
    found << "Unique instruction pair at RVA 0x" << std::hex
          << (scan.sites.enter - reinterpret_cast<std::uintptr_t>(GetModuleHandleW(L"CrimsonDesert.exe")))
          << " and RVA 0x"
          << (scan.sites.leave - reinterpret_cast<std::uintptr_t>(GetModuleHandleW(L"CrimsonDesert.exe")));
    log(found.str().c_str());
    auto enter = safetyhook::MidHook::create(reinterpret_cast<void*>(scan.sites.enter), on_enter);
    if (enter) {
      g_enter = std::move(*enter);
      auto leave = safetyhook::MidHook::create(reinterpret_cast<void*>(scan.sites.leave), on_leave);
      if (leave) {
        g_leave = std::move(*leave);
        observing = true;
        log("State mid-hooks active; waiting for RSI/state transitions");

      } else log("Leave mid-hook failed");
    } else log("Enter mid-hook failed");
    if (!observing) { g_enter.reset(); g_leave.reset(); g_state_base.store(0); }
  }
  bool last_state = false;
  std::uint64_t last_events = 0;
  std::uintptr_t last_base = 0;
  ULONGLONG last_base_log = 0;
  while (!g_stopping.load()) {
    if (observing) {
      const auto state = detected();
      const auto base = g_state_base.load(std::memory_order_acquire);
      const auto events = g_hook_events.load(std::memory_order_acquire);
      const auto now = GetTickCount64();
      if (events != last_events) {
        const auto pre = g_pre_state.load(std::memory_order_relaxed);
        if (pre != UINT32_MAX) {
          std::ostringstream line;
          line << "Treasure pre-instruction state " << pre << " base=0x" << std::hex
               << base << std::dec << " hook_events=" << events;
          log(line.str().c_str());
        }
        last_events = events;
      }
      if (state != last_state || (base && base != last_base && now - last_base_log >= 1000)) {
        std::ostringstream line;
        line << "Treasure state " << (state ? 1 : 0) << " base=0x" << std::hex
             << base << " state_address=0x" << (base ? base + phi::patterns::kStateOffset : 0)
             << std::dec << " hook_events=" << events;
        log(line.str().c_str());
        last_base_log = now;
      }
      phi::set_overlay_state(state);
      last_state = state;
      last_base = base;
    }
    if (GetAsyncKeyState(g_toggle_key) & 1) { g_enabled.store(!g_enabled.load()); phi::set_overlay_enabled(g_enabled.load()); }
    if (GetAsyncKeyState(g_unload_key) & 1) break;
    Sleep(30);
  }
  g_stopping.store(true);
  if (overlay_started) phi::stop_overlay();
  g_enter.reset(); g_leave.reset();
  log("Unloaded");
  g_log.close();
  FreeLibraryAndExitThread(g_self, 0);
}
}

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID) {
  if (reason == DLL_PROCESS_ATTACH) {
    g_self = module;
    DisableThreadLibraryCalls(module);
    if (HANDLE thread = CreateThread(nullptr, 0, worker, nullptr, 0, nullptr)) CloseHandle(thread);
  }
  return TRUE;
}
