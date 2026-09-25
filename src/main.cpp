#include "pattern_scan.hpp"
#include "patterns.hpp"
#include <windows.h>
#include <d3d11.h>
#include <dxgi.h>
#include <wrl/client.h>
#include <safetyhook.hpp>
#include <imgui.h>
#include <backends/imgui_impl_dx11.h>
#include <backends/imgui_impl_win32.h>
#include <atomic>
#include <cstdint>
#include <fstream>
#include <mutex>
#include <sstream>
#include <string>
#include <utility>

namespace {
using Microsoft::WRL::ComPtr;
constexpr char kVersion[] = "0.2.0-diagnostic-preview";

HMODULE g_self{};
std::wstring g_folder;
std::ofstream g_log;
std::mutex g_log_mutex;
safetyhook::MidHook g_enter, g_leave;
safetyhook::InlineHook g_present;
std::atomic<std::uintptr_t> g_state_base{0};
std::atomic<std::uint64_t> g_hook_events{0};
std::atomic<std::uint32_t> g_pre_state{UINT32_MAX};
std::atomic<bool> g_enabled{true}, g_stopping{false};
std::atomic<unsigned> g_in_present{0};
std::atomic_flag g_rendering = ATOMIC_FLAG_INIT;
int g_x = 40, g_y = 140;
float g_scale = 1.0f;
int g_toggle_key = VK_F9, g_unload_key = VK_F10;
bool g_imgui_ready = false;
HWND g_hwnd{};
IDXGISwapChain* g_swap{};
ComPtr<ID3D11Device> g_device;
ComPtr<ID3D11DeviceContext> g_context;

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
  g_x = GetPrivateProfileIntW(L"indicator", L"x", 40, path.c_str());
  g_y = GetPrivateProfileIntW(L"indicator", L"y", 140, path.c_str());
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

void draw_icon() {
  if (!g_enabled.load(std::memory_order_relaxed) || !detected()) return;
  auto* draw = ImGui::GetForegroundDrawList();
  const float x = static_cast<float>(g_x), y = static_cast<float>(g_y), s = g_scale;
  const ImVec2 a{x, y}, b{x + 44 * s, y + 44 * s};
  draw->AddRectFilled(a, b, IM_COL32(12, 20, 28, 210), 8 * s);
  draw->AddRect(a, b, IM_COL32(238, 193, 85, 255), 8 * s, 0, 2 * s);
  draw->AddRectFilled(ImVec2(x + 9*s, y + 19*s), ImVec2(x + 35*s, y + 34*s), IM_COL32(181, 113, 39, 255), 2*s);
  draw->AddRect(ImVec2(x + 9*s, y + 13*s), ImVec2(x + 35*s, y + 24*s), IM_COL32(246, 209, 112, 255), 3*s, 0, 2*s);
  draw->AddLine(ImVec2(x + 22*s, y + 19*s), ImVec2(x + 22*s, y + 34*s), IM_COL32(255, 224, 127, 255), 3*s);
  draw->AddCircleFilled(ImVec2(x + 22*s, y + 26*s), 2.5f*s, IM_COL32(39, 30, 20, 255));
}

HRESULT WINAPI on_present(IDXGISwapChain* swap, UINT interval, UINT flags) {
  g_in_present.fetch_add(1, std::memory_order_acq_rel);
  const bool render_this_frame = !g_rendering.test_and_set(std::memory_order_acquire);
  if (render_this_frame && !g_stopping.load(std::memory_order_acquire) && !(flags & DXGI_PRESENT_TEST)) {
    if (!g_device) {
      DXGI_SWAP_CHAIN_DESC desc{};
      if (SUCCEEDED(swap->GetDevice(__uuidof(ID3D11Device), reinterpret_cast<void**>(g_device.GetAddressOf()))) &&
          SUCCEEDED(swap->GetDesc(&desc)) && desc.OutputWindow) {
        g_device->GetImmediateContext(g_context.GetAddressOf());
        g_hwnd = desc.OutputWindow;
        g_swap = swap;
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGui_ImplWin32_Init(g_hwnd);
        ImGui_ImplDX11_Init(g_device.Get(), g_context.Get());
        g_imgui_ready = true;
        log("DX11 overlay initialized");
      }
    }
    if (g_imgui_ready && g_context && swap == g_swap) {
      ComPtr<ID3D11Texture2D> backbuffer;
      ComPtr<ID3D11RenderTargetView> target;
      if (SUCCEEDED(swap->GetBuffer(0, IID_PPV_ARGS(backbuffer.GetAddressOf()))) &&
          SUCCEEDED(g_device->CreateRenderTargetView(backbuffer.Get(), nullptr, target.GetAddressOf()))) {
        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();
        draw_icon();
        ImGui::Render();
        ComPtr<ID3D11RenderTargetView> old_target;
        ComPtr<ID3D11DepthStencilView> old_depth;
        g_context->OMGetRenderTargets(1, old_target.GetAddressOf(), old_depth.GetAddressOf());
        auto* view = target.Get();
        g_context->OMSetRenderTargets(1, &view, nullptr);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
        auto* restore = old_target.Get();
        g_context->OMSetRenderTargets(1, &restore, old_depth.Get());
      }
    }
  }
  const auto result = g_present.call<HRESULT>(swap, interval, flags);
  if (render_this_frame) g_rendering.clear(std::memory_order_release);
  g_in_present.fetch_sub(1, std::memory_order_acq_rel);
  return result;
}

void* present_address() {
  const auto instance = GetModuleHandleW(nullptr);
  const wchar_t* name = L"PirateHatHUDProbe";
  WNDCLASSW wc{}; wc.lpfnWndProc = DefWindowProcW; wc.hInstance = instance; wc.lpszClassName = name;
  if (!RegisterClassW(&wc)) return nullptr;
  const HWND window = CreateWindowW(name, name, WS_OVERLAPPEDWINDOW, 0, 0, 64, 64, nullptr, nullptr, instance, nullptr);
  if (!window) { UnregisterClassW(name, instance); return nullptr; }
  DXGI_SWAP_CHAIN_DESC desc{};
  desc.BufferCount = 1; desc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
  desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT; desc.OutputWindow = window;
  desc.SampleDesc.Count = 1; desc.Windowed = TRUE; desc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
  ComPtr<IDXGISwapChain> swap; ComPtr<ID3D11Device> device; ComPtr<ID3D11DeviceContext> context;
  D3D_FEATURE_LEVEL level{};
  const auto hr = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0,
    nullptr, 0, D3D11_SDK_VERSION, &desc, swap.GetAddressOf(), device.GetAddressOf(), &level, context.GetAddressOf());
  void* address = nullptr;
  if (SUCCEEDED(hr)) address = (*reinterpret_cast<void***>(swap.Get()))[8];
  swap.Reset(); context.Reset(); device.Reset();
  DestroyWindow(window); UnregisterClassW(name, instance);
  return address;
}

DWORD WINAPI worker(void*) {
  wchar_t path[MAX_PATH]{};
  GetModuleFileNameW(g_self, path, MAX_PATH);
  g_folder = path;
  g_folder.resize(g_folder.find_last_of(L"\\/") + 1);
  g_log.open(g_folder + L"PirateHatHUD.log", std::ios::app);
  log(kVersion);
  read_config();
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
        log("DX11 overlay disabled in diagnostic preview");
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
      last_state = state;
      last_base = base;
    }
    if (GetAsyncKeyState(g_toggle_key) & 1) g_enabled.store(!g_enabled.load());
    if (GetAsyncKeyState(g_unload_key) & 1) break;
    Sleep(30);
  }
  g_stopping.store(true);
  if (g_present) (void)g_present.disable();
  while (g_in_present.load(std::memory_order_acquire)) Sleep(1);
  g_enter.reset(); g_leave.reset(); g_present.reset();
  if (g_imgui_ready) { ImGui_ImplDX11_Shutdown(); ImGui_ImplWin32_Shutdown(); ImGui::DestroyContext(); }
  g_context.Reset(); g_device.Reset();
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
