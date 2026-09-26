#include "app.hpp"
#include <windows.h>
namespace {
DWORD WINAPI worker(void* context) {
  const auto module = static_cast<HMODULE>(context);
  phi::run_app(module);
  FreeLibraryAndExitThread(module, 0);
}
} // namespace
BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID) {
  if (reason == DLL_PROCESS_ATTACH) {
    DisableThreadLibraryCalls(module);
    if (HANDLE thread = CreateThread(nullptr, 0, worker, module, 0, nullptr)) {
      CloseHandle(thread);
    }
  }
  return TRUE;
}
