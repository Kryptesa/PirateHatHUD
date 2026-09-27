#pragma once

#include <windows.h>

namespace phi {
enum class AppExitDisposition { unload_allowed, retain_module };

AppExitDisposition run_app(HMODULE module) noexcept;
} // namespace phi
