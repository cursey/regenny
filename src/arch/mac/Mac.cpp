#include "Mac.hpp"

namespace arch {
std::unique_ptr<Helpers> make_helpers() {
    return std::make_unique<arch::MacHelpers>();
}

std::unique_ptr<Process> open_process(uint32_t process_id) {
    return std::make_unique<arch::MacProcess>(process_id);
}
}