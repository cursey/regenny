#include "Linux.hpp"

#include <algorithm>
#include <array>
#include <cassert>
#include <charconv>
#include <csignal>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <fcntl.h>
#include <filesystem>
#include <fstream>
#include <map>
#include <optional>
#include <signal.h>
#include <string>
#include <string_view>
#include <sys/types.h>
#include <system_error>
#include <unistd.h>
#include <utility>
#include <vector>

namespace arch {

LinuxProcess::LinuxProcess(pid_t process_id) : m_process(process_id) {
    std::string path = "/proc/" + std::to_string(process_id) + "/mem";
    rw_handle = open(path.c_str(), O_RDWR);
    assert(rw_handle >= 0); // TODO better error handling

    std::fstream maps{"/proc/" + std::to_string(process_id) + "/maps", std::fstream::in};
    assert(maps);

    for (std::string line; std::getline(maps, line);) {
        if (line.empty())
            continue; // ?

        std::uintptr_t begin = 0;
        std::uintptr_t end = 0;
        std::array<char, 3> perms{};
        char shared = 0;
        std::string name;

        int offset = -1;

        sscanf(line.c_str(), "%zx-%zx %c%c%c%c %*x %*x:%*x %*x%n", &begin, &end, &perms[0], &perms[1], &perms[2],
            &shared, &offset);

        while (offset < line.length() && line[offset] == ' ')
            offset++;
        if (offset < line.length())
            name = line.c_str() + offset;

        if (!name.empty()) {
            constexpr static const char* DELETED_TAG = " (deleted)";
            constexpr static std::size_t DELETED_TAG_LEN = std::char_traits<char>::length(DELETED_TAG);
            if (name.ends_with(DELETED_TAG)) {
                name = name.substr(0, name.length() - DELETED_TAG_LEN);
            }

            if (name[0] == '[') {
                perms[0] = '-';
            }
        }

        if (!name.empty()) {
            auto it = std::ranges::find_if(m_modules, [&name](Module& module) { return module.name == name; });

            if (it == m_modules.end()) {
                m_modules.emplace_back(name, begin, end, end - begin);
            } else {
                it->start = std::min(it->start, begin);
                it->end = std::min(it->end, end);
                it->size = it->end - it->start;
            }
        }

        m_allocations.emplace_back(begin, end, end - begin, perms[0] == 'r', perms[1] == 'w', perms[2] == 'x');

        if (perms[0] == 'r') {
            ReadOnlyAllocation ro_alloc{
                Allocation(begin, end, end - begin, perms[0] == 'r', perms[1] == 'w', perms[2] == 'x'), {}};

            ro_alloc.mem.reserve(end - begin);

            if (read(begin, ro_alloc.mem.data(), end - begin)) {
                m_read_only_allocations.emplace_back(std::move(ro_alloc));
            }
        }
    }

    maps.close();
}

uint32_t LinuxProcess::process_id() {
    return m_process;
}

bool LinuxProcess::ok() {
    return kill(m_process, 0) == 0;
}

std::optional<std::string> LinuxProcess::get_typename(uintptr_t ptr) {
    // TODO
    return {};
}
std::optional<std::string> LinuxProcess::get_typename_from_vtable(uintptr_t ptr) {
    // TODO
    return {};
}

bool LinuxProcess::handle_write(uintptr_t address, const void* buffer, size_t size) {
    return pwrite(rw_handle, buffer, size, address) == static_cast<int>(size);
}
bool LinuxProcess::handle_read(uintptr_t address, void* buffer, size_t size) {
    return pread(rw_handle, buffer, size, address) == static_cast<int>(size);
}
std::optional<uint64_t> LinuxProcess::handle_protect(uintptr_t address, size_t size, uint64_t flags) {
    // TODO
    return {};
}
std::optional<uintptr_t> LinuxProcess::handle_allocate(uintptr_t address, size_t size, uint64_t flags) {
    // TODO
    return {};
}

std::map<uint32_t, std::string> LinuxHelpers::processes() {
    std::map<std::uint32_t, std::string> processes;
    for (const std::filesystem::path& it : std::filesystem::directory_iterator{"/proc"}) {
        if (!std::filesystem::is_directory(it))
            continue;

        const std::string& str = it.filename();

        pid_t pid;
        auto result = std::from_chars(str.data(), str.data() + str.size(), pid);

        if (result.ec != std::errc{})
            continue;

        std::ifstream f{"/proc/" + str + "/status"};

        std::string name;

        for (std::string line; std::getline(f, line);) {
            static constexpr std::string_view PATTERN = "Name:";

            if (line.starts_with(PATTERN)) {
                name = line.substr(line.find_first_not_of(' ', PATTERN.length() + 1));
                break;
            }
        }

        f.close();
        if (name.empty())
            continue;

        processes.insert({pid, name});
    }
    return processes;
}

} // namespace arch
