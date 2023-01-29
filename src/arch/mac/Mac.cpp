#include <libproc.h>
#include <mach/mach.h>

#include "Mac.hpp"

namespace arch {
std::unique_ptr<Helpers> make_helpers() {
    return std::make_unique<arch::MacHelpers>();
}

std::unique_ptr<Process> open_process(uint32_t process_id) {
    return std::make_unique<arch::MacProcess>(process_id);
}

MacProcess::MacProcess(uint32_t process_id) : Process{} {
    if (task_for_pid(mach_task_self(), process_id, &m_process) != KERN_SUCCESS) {
        return;
    }

    // Iterate regions.
    vm_address_t address = 1;
    vm_size_t size = 0;
    vm_region_basic_info_data_64_t info;
    mach_msg_type_number_t info_count = VM_REGION_BASIC_INFO_COUNT_64;
    mach_port_t object_name = MACH_PORT_NULL;

    while (vm_region_64(m_process, &address, &size, VM_REGION_BASIC_INFO_64, (vm_region_info_t)&info, &info_count,
               &object_name) == KERN_SUCCESS) {
        Allocation a{};

        a.start = address;
        a.size = size;
        a.end = a.start + a.size;
        a.read = info.protection & VM_PROT_READ;
        a.write = info.protection & VM_PROT_WRITE;
        a.execute = info.protection & VM_PROT_EXECUTE;

        m_allocations.emplace_back(std::move(a));

        address += size;
    }
}

bool MacProcess::handle_read(uintptr_t address, void* buffer, size_t size) {
    vm_size_t vm_size = size;
    const auto read_status = vm_read_overwrite(m_process, address, size, (vm_address_t)buffer, &vm_size);
    const auto valid_read = read_status == KERN_SUCCESS;
    const auto full_read = vm_size == size;

    return valid_read && full_read;
}

std::map<uint32_t, std::string> MacHelpers::processes() {
    std::map<uint32_t, std::string> processes{};

    int pid_list[1024];
    int num_processes = proc_listallpids(pid_list, sizeof(pid_list));

    for (int i = 0; i < num_processes; i++) {
        char path_buffer[PROC_PIDPATHINFO_MAXSIZE];
        proc_name(pid_list[i], path_buffer, sizeof(path_buffer));

        processes[pid_list[i]] = path_buffer;
    }

    return processes;
}
} // namespace arch