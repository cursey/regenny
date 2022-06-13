# Overview: This script will take
# Input: A dumped executable file
# and will manually map the file into memory using the PE sections
# so we can view it with runtime analysis tools

import pefile
import os
import fire
import ctypes

VirtualAlloc = ctypes.windll.kernel32.VirtualAlloc
VirtualAlloc.restype = ctypes.c_uint64
VirtualAlloc.argtypes = [
    ctypes.c_uint64, ctypes.c_uint64, ctypes.wintypes.DWORD, ctypes.wintypes.DWORD
]

VirtualAllocEx = ctypes.windll.kernel32.VirtualAllocEx
VirtualAllocEx.restype = ctypes.c_uint64
VirtualAllocEx.argtypes = [
    ctypes.wintypes.HANDLE,
    ctypes.c_uint64, ctypes.c_uint64, ctypes.wintypes.DWORD, ctypes.wintypes.DWORD
]

OpenProcess = ctypes.windll.kernel32.OpenProcess
OpenProcess.restype = ctypes.wintypes.HANDLE
OpenProcess.argtypes = [ctypes.wintypes.DWORD, ctypes.wintypes.BOOL, ctypes.c_int]

WriteProcessMemory = ctypes.windll.kernel32.WriteProcessMemory
WriteProcessMemory.restype = ctypes.wintypes.BOOL
WriteProcessMemory.argtypes = [
    ctypes.wintypes.HANDLE, ctypes.c_uint64, ctypes.c_void_p, ctypes.c_uint64, ctypes.POINTER(ctypes.c_uint64)
]

PROCESS_ALL_ACCESS = 0x101ffb

MEM_COMMIT = 0x1000
MEM_RESERVE = 0x2000
MEM_COMMIT_RESERVE = MEM_COMMIT | MEM_RESERVE

PAGE_READWRITE = 0x04
PAGE_READWRITE_EXECUTE = 0x40
PAGE_READ_EXECUTE = 0x20

def main(input=None, pid=None):
    # Check if the file exists
    if input is None or not os.path.isfile(input):
        print("[-] File does not exist")
        return

    has_process = False

    if pid is not None:
        process = OpenProcess(PROCESS_ALL_ACCESS, False, int(pid))

        if process is None or process == 0 or process == -1:
            print("[-] Failed to open process")
            return
        else:
            print("[+] Opened process " + str(pid) + " with handle " + str(hex(process)))
            has_process = True


    print("[+] Parsing file")

    # Parse the file
    pe = pefile.PE(input)

    # Check if the file is a PE file
    if not pe.is_exe() and not pe.is_dll():
        print("[-] File is not a PE file")
        return

    # Check if the file is a 64 bit file
    if pe.OPTIONAL_HEADER.Magic != 0x20b:
        print("[-] File is not a 64 bit file, not supported. Magic: " + str(hex(pe.OPTIONAL_HEADER.Magic)))
        return

    print(pe.NT_HEADERS)
    print(pe.OPTIONAL_HEADER)
    print(pe.FILE_HEADER)

    imagebase = pe.OPTIONAL_HEADER.ImageBase

    total_virtual_size = pe.OPTIONAL_HEADER.SizeOfHeaders

    for section  in pe.sections:
        total_virtual_size += section.Misc_VirtualSize

    print("[+] Total virtual size: " + str(hex(total_virtual_size)))

    # Align the virtual size to the section alignment in pe.OPTIONAL_HEADER.SectionAlignment
    alignment = pe.OPTIONAL_HEADER.SectionAlignment
    total_virtual_size = (total_virtual_size + alignment - 1) & ~(alignment - 1)

    print("[+] Aligned virtual size: " + str(hex(total_virtual_size)))

    if (total_virtual_size < pe.OPTIONAL_HEADER.SizeOfImage):
        total_virtual_size = pe.OPTIONAL_HEADER.SizeOfImage
        print("[+] Fixed total virtual size to: " + str(hex(total_virtual_size)))

    if not has_process:
        addr = VirtualAlloc(imagebase, total_virtual_size, MEM_COMMIT_RESERVE, PAGE_READWRITE_EXECUTE)
    else:
        addr = VirtualAllocEx(process, imagebase, total_virtual_size, MEM_COMMIT_RESERVE, PAGE_READWRITE_EXECUTE)

    if addr == 0:
        print("[-] Failed to allocate memory for " + input)
        return

    print("[+] Allocated memory at: " + str(hex(addr)))

    # Write the headers to the start of the allocated space
    # TODO

    # Get the sections, and write them to memory
    for section in pe.sections:
        print("[+] Writing section: " + section.Name.decode("utf-8"))
        print("[+]  Section virtual address: " + str(hex(section.VirtualAddress)))
        print("[+]  Section virtual size: " + str(hex(section.Misc_VirtualSize)))
        print("[+]  Section raw size: " + str(hex(section.SizeOfRawData)))
        print("[+]  Buffer address: " + str(hex(addr + section.VirtualAddress)))

        size = section.SizeOfRawData

        # if the size is 0, then we continue
        # because it's uninitialized data
        if size == 0:
            continue

        section_data = pe.get_data(section.VirtualAddress, size)

        # Copy the section data into buf (aka the allocated space)
        print("[+]  Writing section " + section.Name.decode("utf-8") + " to: " + str(hex(addr + section.VirtualAddress)) + " to " + str(hex(addr + section.VirtualAddress + size)))

        if not has_process:
            ctypes.memmove(addr + section.VirtualAddress, section_data, size)
        else:
            written = ctypes.c_uint64(0)
            WriteProcessMemory(process, addr + section.VirtualAddress, section_data, size, ctypes.byref(written))


    print("[+] Memory is allocated at " + str(hex(addr)))
    os.system("pause")


if __name__ == '__main__':
    fire.Fire(main)