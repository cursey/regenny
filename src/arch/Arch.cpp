#ifdef _WIN32
#include "Windows.hpp"
#endif

#include "Arch.hpp"

std::unique_ptr<Helpers> arch::make_helpers() {
#ifdef _WIN32
    return std::make_unique<arch::WindowsHelpers>();
#endif
}

std::unique_ptr<Process> arch::open_process(uint32_t process_id) {
#ifdef _WIN32
    return std::make_unique<arch::WindowsProcess>(process_id);
#endif
}
