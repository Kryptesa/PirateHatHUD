#include "app.hpp"
#include <windows.h>

namespace {
DWORD WINAPI worker(void* context) {
  const auto module = static_cast<HMODULE>(context);

  if (phi::run_app(module) == phi::AppExitDisposition::unload_allowed) {
    FreeLibraryAndExitThread(module, 0);
  }

  return 0;
}
} // namespace

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID) {
  if (reason == DLL_PROCESS_ATTACH) {
    DisableThreadLibraryCalls(module);

    // ASI loaders can also load this file into the game's crash handler.
    // Only the game process may start observation, graphics hooks or hotkeys.
    if (GetModuleHandleW(L"CrimsonDesert.exe") != GetModuleHandleW(nullptr)) {
      return TRUE;
    }

    if (HANDLE thread = CreateThread(nullptr, 0, worker, module, 0, nullptr)) {
      CloseHandle(thread);
    }
  }

  return TRUE;
}
