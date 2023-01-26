#pragma once

#include <memory>

#include "Helpers.hpp"
#include "Process.hpp"

namespace arch {
std::unique_ptr<Helpers> make_helpers();
std::unique_ptr<Process> open_process(uint32_t process_id);
} // namespace arch

#ifdef _WIN32
#include "win/Windows.hpp"
#endif

#ifdef __APPLE__
#include "mac/Mac.hpp"
#endif
