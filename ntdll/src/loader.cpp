// SPDX-License-Identifier: LGPL-2.1-or-later
module;

#include "list.h"
#include <mutex>
#include <utility>
#include <string_view>
#include <optional>
#include <memory>
#include <span>
#include <string.h>
#include <stdlib.h>

export module loader;

import nt;
import ntdll;
import print;
import ntdll_private;
import unicode;
import unique_handle;
import pe;
import format;
import list;

using namespace std;

export PEB_LDR_DATA ldr_data; // zero-initialized

struct dll_entry {
    list_entry le;
    ldr_entry* ent;
    bool new_entry;
    unsigned int refs;
    size_t num_deps = 0;
    unique_ptr<dll_entry*[]> deps;
};

static void unload_dll_holding_lock(ldr_entry& ent);

extern "C"
void* RtlPcToFileHeader(const void* pc, void** address) {
    auto& teb = *NtCurrentTeb();
    auto& peb = *teb.ProcessEnvironmentBlock;

    {
        mn_loader_lock lock;
        lock_guard lg(lock);

        for (auto le = peb.Ldr->InMemoryOrderModuleList.Flink; le != &peb.Ldr->InMemoryOrderModuleList; le = le->Flink) {
            auto& mod = *CONTAINING_RECORD(le, LDR_DATA_TABLE_ENTRY, InMemoryOrderLinks);

            if (pc >= mod.DllBase && pc < (uint8_t*)mod.DllBase + mod.SizeOfImage) {
                *address = mod.DllBase;
                return mod.DllBase;
            }
        }
    }

    *address = nullptr;
    return nullptr;
}

extern "C"
nt_status LdrAddRefDll(uint32_t flags, HMODULE mod) {
    auto& teb = *NtCurrentTeb();
    auto& peb = *teb.ProcessEnvironmentBlock;

    // FIXME - LDR_ADDREF_DLL_PIN

    if (flags != 0) {
        print_stderr("FIXME - LdrAddRefDll {:x}\n", flags);
        return nt_status::NOT_IMPLEMENTED;
    }

    {
        mn_loader_lock lock;
        lock_guard lg(lock);

        for (auto le = peb.Ldr->InMemoryOrderModuleList.Flink; le != &peb.Ldr->InMemoryOrderModuleList; le = le->Flink) {
            auto& mod2 = *CONTAINING_RECORD(le, LDR_DATA_TABLE_ENTRY, InMemoryOrderLinks);

            if (mod2.DllBase == mod) {
                mod2.LoadCount++;
                return nt_status::SUCCESS;
            }
        }
    }

    return nt_status::INVALID_PARAMETER;
}

static void unload_imports(LDR_DATA_TABLE_ENTRY& ent) {
    auto& peb = *NtCurrentTeb()->ProcessEnvironmentBlock;
    auto& dos_header = *(IMAGE_DOS_HEADER*)ent.DllBase;
    auto& nt_header = *(IMAGE_NT_HEADERS*)((uint8_t*)ent.DllBase + dos_header.e_lfanew);

    IMAGE_DATA_DIRECTORY exp_dir;
    bool found = false;

    if (nt_header.OptionalHeader32.Magic == IMAGE_NT_OPTIONAL_HDR64_MAGIC) {
        if (nt_header.OptionalHeader64.NumberOfRvaAndSizes > IMAGE_DIRECTORY_ENTRY_IMPORT) {
            exp_dir = nt_header.OptionalHeader64.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
            found = true;
        }
    } else {
        if (nt_header.OptionalHeader32.NumberOfRvaAndSizes > IMAGE_DIRECTORY_ENTRY_IMPORT) {
            exp_dir = nt_header.OptionalHeader32.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
            found = true;
        }
    }

    if (!found || exp_dir.VirtualAddress == 0 || exp_dir.Size < sizeof(IMAGE_IMPORT_DESCRIPTOR))
        return;

    auto iids = span((IMAGE_IMPORT_DESCRIPTOR*)((uint8_t*)ent.DllBase + exp_dir.VirtualAddress),
                     exp_dir.Size / sizeof(IMAGE_IMPORT_DESCRIPTOR));

    for (auto& iid : iids) {
        if (iid.Characteristics == 0)
            break;

        auto name = string_view((char*)ent.DllBase + iid.Name);

        if (name.empty())
            continue;

        auto u16len = utf8_to_utf16_len(name);

        // FIXME - it would be nice if we could avoid an allocation here (have function to compare UTF-8 with UTF-16?)
        unique_ptr<char16_t[]> u16buf{new char16_t[u16len]};

        if (!u16buf) {
            print("new failed\n");
            return;
        }

        utf8_to_utf16(span(u16buf.get(), u16len), name);

        auto u16sv = u16string_view(u16buf.get(), u16len);

        bool done = false;

        for (auto le = peb.Ldr->InMemoryOrderModuleList.Flink; le != &peb.Ldr->InMemoryOrderModuleList; le = le->Flink) {
            auto& mod2 = *CONTAINING_RECORD(le, LDR_DATA_TABLE_ENTRY, InMemoryOrderLinks);

            auto u16sv2 = u16string_view(mod2.BaseDllName.Buffer, mod2.BaseDllName.Length / sizeof(char16_t));

            if (u16sv == u16sv2) {
                unload_dll_holding_lock(*CONTAINING_RECORD(&mod2, ldr_entry, ldr));
                done = true;
                break;
            }
        }

        if (!done)
            print_stderr("unload_imports: {} not found\n", name);
    }
}

static void unload_dll_holding_lock(ldr_entry& ent) {
    if (ent.ldr.LoadCount > 1) {
        ent.ldr.LoadCount--;
        return;
    }

    if (ent.ldr.ProcessAttachCalled)
        call_process_detach_img(ent.ldr, false);

    RemoveEntryList(&ent.ldr.InInitializationOrderLinks);
    RemoveEntryList(&ent.ldr.InLoadOrderLinks);
    RemoveEntryList(&ent.ldr.InMemoryOrderLinks);

    unload_imports(ent.ldr);

    NtUnmapViewOfSection(NtCurrentProcess(), ent.ldr.DllBase);

    free(&ent);
}

extern "C"
nt_status LdrUnloadDll(HMODULE mod) {
    auto& peb = *NtCurrentTeb()->ProcessEnvironmentBlock;

    // don't allow unloading of main image
    if (mod == peb.ImageBaseAddress)
        return nt_status::SUCCESS;

    {
        mn_loader_lock lock;
        lock_guard lg(lock);

        for (auto le = peb.Ldr->InMemoryOrderModuleList.Flink; le != &peb.Ldr->InMemoryOrderModuleList; le = le->Flink) {
            auto& mod2 = *CONTAINING_RECORD(le, LDR_DATA_TABLE_ENTRY, InMemoryOrderLinks);

            if (mod2.DllBase != mod)
                continue;

            unload_dll_holding_lock(*CONTAINING_RECORD(&mod2, ldr_entry, ldr));

            return nt_status::SUCCESS;
        }
    }

    return nt_status::DLL_NOT_FOUND;
}

extern "C"
nt_status LdrFindEntryForAddress(void* address, LDR_DATA_TABLE_ENTRY** entry) {
    auto& teb = *NtCurrentTeb();
    auto& peb = *teb.ProcessEnvironmentBlock;
    auto& ldr_data = *peb.Ldr;

    // loader lock must be held before calling this function

    for (auto le = ldr_data.InMemoryOrderModuleList.Flink; le != &ldr_data.InMemoryOrderModuleList; le = le->Flink) {
        auto& ent = *CONTAINING_RECORD(le, LDR_DATA_TABLE_ENTRY, InMemoryOrderLinks);

        if ((uintptr_t)address >= (uintptr_t)ent.DllBase &&
            (uintptr_t)address < (uintptr_t)ent.DllBase + ent.SizeOfImage) {
            *entry = &ent;
            return nt_status::SUCCESS;
        }
    }

    return nt_status::NO_MORE_ENTRIES;
}

extern "C"
nt_status LdrGetDllHandle(const char16_t* /*dll_path*/, uint32_t* /*dll_characteristics*/,
                          const UNICODE_STRING* dll_name, void** dll_handle) {
    auto& teb = *NtCurrentTeb();
    auto& peb = *teb.ProcessEnvironmentBlock;

    // FIXME - dll_path
    // FIXME - dll_characteristics

    if (!dll_name)
        return nt_status::INVALID_PARAMETER;

    auto search = u16string_view(dll_name->Buffer, dll_name->Length / sizeof(char16_t));

    mn_loader_lock lock;
    lock_guard lg(lock);

    for (auto le = peb.Ldr->InLoadOrderModuleList.Flink; le != &peb.Ldr->InLoadOrderModuleList; le = le->Flink) {
        auto& ldr = *(LDR_DATA_TABLE_ENTRY*)((uint8_t*)le - offsetof(LDR_DATA_TABLE_ENTRY, InLoadOrderLinks));

        auto sv = u16string_view(ldr.BaseDllName.Buffer, ldr.BaseDllName.Length / sizeof(char16_t));

        if (sv == search) {
            *dll_handle = ldr.DllBase;
            return nt_status::SUCCESS;
        }
    }

    return nt_status::DLL_NOT_FOUND;
}

static u16string_view get_env(u16string_view name) {
    mn_peb_lock lock;
    lock_guard lg(lock);

    auto peb = NtCurrentTeb()->ProcessEnvironmentBlock;
    auto env = u16string_view((char16_t*)peb->ProcessParameters->Environment,
                              peb->ProcessParameters->EnvironmentSize / sizeof(char16_t));

    while (true) {
        if (env.empty() || env[0] == 0)
            break;

        auto zero = env.find(u'\0');

        u16string_view bit;

        if (zero == u16string_view::npos)
            bit = env;
        else
            bit = u16string_view(env.data(), zero);

        if (auto eq = bit.find(u'='); eq != u16string_view::npos) {
            if (u16string_view(bit.data(), eq) == name)
                return u16string_view(bit.data() + eq + 1, bit.size() - eq - 1);
        }

        if (zero != u16string_view::npos)
            env = u16string_view(env.data() + zero + 1, env.size() - zero - 1);
        else
            break;
    }

    return u"";
}

static span<char> get_section_name(const IMAGE_SECTION_HEADER& s, const IMAGE_FILE_HEADER& file_header,
                                   HANDLE h, span<char> buf) {
    auto name = string_view(s.Name, sizeof(s.Name));

    while (!name.empty() && name.back() == 0) {
        name.remove_suffix(1);
    }

    if (name.empty() || name[0] != '/' || file_header.PointerToSymbolTable == 0)
        return format(buf, "{}", name);

    // decode long section names

    unsigned int num = 0;
    uint32_t string_table = file_header.PointerToSymbolTable + (file_header.NumberOfSymbols * 18);

    for (auto c : string_view(name.data() + 1, name.size() - 1)) {
        num *= 10;

        if (c < '0' || c > '9')
            return format(buf, "{}", name);

        num += (unsigned int)(c - '0');
    }

    LARGE_INTEGER off;
    IO_STATUS_BLOCK iosb;

    off.QuadPart = string_table + num;

    auto status = NtReadFile(h, 0, nullptr, nullptr, &iosb, buf.data(),
                             (uint32_t)buf.size(), &off, nullptr);
    if (!success(status))
        return format(buf, "{}", name);

    if (iosb.Information < buf.size())
        buf = span(buf.data(), iosb.Information);

    size_t end = buf.size();
    for (unsigned int i = 0; i < end; i++) {
        if (buf[i] == 0) {
            end = i;
            break;
        }
    }

    return buf.subspan(end);
}

static ldr_entry* add_ldr_entry(void* base, u16string_view full_name, u16string_view base_name,
                                bool no_dll_main, bool is_dll) {
    auto size = sizeof(ldr_entry) + ((full_name.size() + 1) * sizeof(char16_t)) + ((base_name.size() + 1) * sizeof(char16_t));

    auto ptr = malloc(size);
    if (!ptr) {
        print_stderr("malloc failed.\n");
        return nullptr;
    }

    memset(ptr, 0, size);

    auto& ent = *(ldr_entry*)ptr;
    auto& ldr = ent.ldr;

    auto& dos_header = *(IMAGE_DOS_HEADER*)base;
    auto& nt_header = *(IMAGE_NT_HEADERS*)((uint8_t*)base + dos_header.e_lfanew);

    ldr.DllBase = base;

    if (nt_header.OptionalHeader32.Magic == IMAGE_NT_OPTIONAL_HDR64_MAGIC) {
        ldr.EntryPoint = (uint8_t*)base + nt_header.OptionalHeader64.AddressOfEntryPoint;
        ldr.SizeOfImage = nt_header.OptionalHeader64.SizeOfImage;
    } else {
        ldr.EntryPoint = (uint8_t*)base + nt_header.OptionalHeader32.AddressOfEntryPoint;
        ldr.SizeOfImage = nt_header.OptionalHeader32.SizeOfImage;
    }

    ldr.FullDllName.Length = (uint16_t)(full_name.size() * sizeof(char16_t));
    ldr.FullDllName.MaximumLength = ldr.FullDllName.Length + sizeof(char16_t);
    ldr.FullDllName.Buffer = (char16_t*)((uint8_t*)ptr + sizeof(ldr_entry));

    ldr.BaseDllName.Length = (uint16_t)(base_name.size() * sizeof(char16_t));
    ldr.BaseDllName.MaximumLength = ldr.BaseDllName.Length + sizeof(char16_t);
    ldr.BaseDllName.Buffer = (char16_t*)((uint8_t*)ldr.FullDllName.Buffer + ldr.FullDllName.MaximumLength);

    ldr.LoadCount = 1;
    // FIXME - HashLinks

    if (no_dll_main)
        ldr.ProcessAttachCalled = 1;

    if (is_dll)
        ldr.ImageDll = 1;

    memcpy(ldr.FullDllName.Buffer, full_name.data(), ldr.FullDllName.Length);
    ldr.FullDllName.Buffer[full_name.size()] = 0;

    memcpy(ldr.BaseDllName.Buffer, base_name.data(), ldr.BaseDllName.Length);
    ldr.BaseDllName.Buffer[base_name.size()] = 0;

    InsertTailList(&ldr_data.InLoadOrderModuleList, &ldr.InLoadOrderLinks);

    ldr.TlsIndex = 0xffff;

    {
        auto le = ldr_data.InMemoryOrderModuleList.Flink;
        bool done = false;

        while (le != &ldr_data.InMemoryOrderModuleList) {
            auto ent2 = CONTAINING_RECORD(le, LDR_DATA_TABLE_ENTRY, InMemoryOrderLinks);

            if (ent2->DllBase > ldr.DllBase) {
                InsertHeadList(le->Blink, &ldr.InMemoryOrderLinks);
                done = true;
                break;
            }

            le = le->Flink;
        }

        if (!done)
            InsertTailList(&ldr_data.InMemoryOrderModuleList, &ldr.InMemoryOrderLinks);
    }

    return &ent;
}

static ldr_entry* try_load_dll(u16string_view dir, u16string_view name) {
    OBJECT_ATTRIBUTES oa;
    UNICODE_STRING us;
    NTSTATUS status;
    HANDLE h;
    unique_handle dirh, file, sect;
    IO_STATUS_BLOCK iosb;
    void* addr = nullptr;
    size_t size = 0;

    memset(&oa, 0, sizeof(oa));
    oa.Length = sizeof(OBJECT_ATTRIBUTES);
    oa.ObjectName = &us;

    us.Length = us.MaximumLength = (uint16_t)(dir.size() * sizeof(char16_t));
    us.Buffer = (char16_t*)dir.data();

    status = NtCreateFile(&h, FILE_TRAVERSE, &oa, &iosb, nullptr, 0, 0,
                          FILE_OPEN, FILE_DIRECTORY_FILE, nullptr, 0);
    if (!success(status))
        return nullptr;

    dirh.reset(h);

    us.Length = us.MaximumLength = (uint16_t)(name.size() * sizeof(char16_t));
    us.Buffer = (char16_t*)name.data();
    oa.RootDirectory = dirh.get();

    status = NtCreateFile(&h, FILE_READ_DATA, &oa, &iosb, nullptr, 0, 0,
                          FILE_OPEN, 0, nullptr, 0);

    if (!success(status))
        return nullptr;

    file.reset(h);

    status = NtCreateSection(&h, SECTION_MAP_READ, nullptr, nullptr,
                             PAGE_READONLY, SEC_IMAGE, file.get());

    if (!success(status))
        return nullptr;

    sect.reset(h);

    status = NtMapViewOfSection(sect.get(), NtCurrentProcess(), &addr, 0, 0,
                                nullptr, &size, SECTION_INHERIT::ViewUnmap, 0,
                                PAGE_READONLY);

    sect.reset();

    if (!success(status))
        return nullptr;

    auto& dos_header = *(IMAGE_DOS_HEADER*)addr;
    auto& nt_header = *(IMAGE_NT_HEADERS*)((uint8_t*)addr + dos_header.e_lfanew);

#ifdef __aarch64__
    static const uint16_t this_arch = IMAGE_FILE_MACHINE_ARM64;
#elifdef __x86_64__
    static const uint16_t this_arch = IMAGE_FILE_MACHINE_AMD64;
#endif

    if (nt_header.FileHeader.Machine != this_arch) {
        NtUnmapViewOfSection(NtCurrentProcess(), addr);
        return nullptr;
    }

    if (!(nt_header.FileHeader.Characteristics & IMAGE_FILE_DLL)) {
        NtUnmapViewOfSection(NtCurrentProcess(), addr);
        return nullptr;
    }

    auto* pent = add_ldr_entry(addr, u16string_view(us.Buffer, us.Length / sizeof(char16_t)),
                               name, false, true);
    if (!pent) {
        NtUnmapViewOfSection(NtCurrentProcess(), addr);
        return nullptr;
    }

    auto& ent = *pent;

    span<IMAGE_SECTION_HEADER> sections;

    if (nt_header.OptionalHeader32.Magic == IMAGE_NT_OPTIONAL_HDR64_MAGIC)
        sections = span((IMAGE_SECTION_HEADER*)((uint8_t*)&nt_header.OptionalHeader64 + nt_header.FileHeader.SizeOfOptionalHeader), nt_header.FileHeader.NumberOfSections);
    else
        sections = span((IMAGE_SECTION_HEADER*)((uint8_t*)&nt_header.OptionalHeader32 + nt_header.FileHeader.SizeOfOptionalHeader), nt_header.FileHeader.NumberOfSections);

    for (const auto& sect : sections) {
        char buf[50];
        span sp(buf, sizeof(buf));

        sp = get_section_name(sect, nt_header.FileHeader, file.get(), sp);

        auto name = string_view(buf, (size_t)(sp.data() - buf));

        if (name == ".debug_info") {
            ent.offset_debug_info = sect.VirtualAddress;
            ent.size_debug_info = sect.VirtualSize;
        } else if (name == ".debug_abbrev") {
            ent.offset_debug_abbrev = sect.VirtualAddress;
            ent.size_debug_abbrev = sect.VirtualSize;
        } else if (name == ".debug_rnglists") {
            ent.offset_debug_rnglists = sect.VirtualAddress;
            ent.size_debug_rnglists = sect.VirtualSize;
        } else if (name == ".debug_str") {
            ent.offset_debug_str = sect.VirtualAddress;
            ent.size_debug_str = sect.VirtualSize;
        } else if (name == ".debug_addr") {
            ent.offset_debug_addr = sect.VirtualAddress;
            ent.size_debug_addr = sect.VirtualSize;
        } else if (name == ".debug_str_offsets") {
            ent.offset_debug_str_offsets = sect.VirtualAddress;
            ent.size_debug_str_offsets = sect.VirtualSize;
        } else if (name == ".debug_line") {
            ent.offset_debug_line = sect.VirtualAddress;
            ent.size_debug_line = sect.VirtualSize;
        } else if (name == ".debug_line_str") {
            ent.offset_debug_line_str = sect.VirtualAddress;
            ent.size_debug_line_str = sect.VirtualSize;
        }
    }

    return &ent;
}

static ldr_entry* load_dll_u16(u16string_view u16sv) {
    {
        auto le = ldr_data.InLoadOrderModuleList.Flink;

        while (le != &ldr_data.InLoadOrderModuleList) {
            auto& ent = *CONTAINING_RECORD(le, LDR_DATA_TABLE_ENTRY, InLoadOrderLinks);
            auto base_name = u16string_view(ent.BaseDllName.Buffer, ent.BaseDllName.Length / sizeof(char16_t));

            if (base_name == u16sv) {
                ent.LoadCount++;
                return CONTAINING_RECORD(&ent, ldr_entry, ldr);
            }

            le = le->Flink;
        }
    }

    // FIXME - what if DLL name contains slashes?

    auto path = get_env(u"LD_LIBRARY_PATH");

    if (path.empty()) { // FIXME - is this right?
        if (auto ret = try_load_dll(u"/lib", u16sv); ret)
            return ret;
    } else {
        while (true) {
            u16string_view bit;
            auto col = path.find(u':');

            if (col != u16string_view::npos)
                bit = u16string_view(path.data(), col);
            else
                bit = path;

            if (auto ret = try_load_dll(bit, u16sv); ret)
                return ret;

            if (col == u16string_view::npos)
                break;

            path = u16string_view(path.data() + col + 1, path.size() - col - 1);
        }
    }

    return nullptr;
}

static ldr_entry* load_dll(string_view name) {
    auto u16len = utf8_to_utf16_len(name);

    if (u16len == 0)
        return nullptr;

    auto u16buf = (char16_t*)malloc(u16len * sizeof(char16_t));

    if (!u16buf) {
        NtTerminateProcess(NtCurrentProcess(), nt_status::INSUFFICIENT_RESOURCES);
        unreachable();
    }

    utf8_to_utf16(span(u16buf, u16len), name);

    // FIXME - should be case-insensitive?

    auto u16sv = u16string_view(u16buf, u16len);

    auto ret = load_dll_u16(u16sv);

    free(u16buf);

    return ret;
}

export void load_dlls_img(LDR_DATA_TABLE_ENTRY& ent) {
    auto& dos_header = *(IMAGE_DOS_HEADER*)ent.DllBase;
    auto& nt_header = *(IMAGE_NT_HEADERS*)((uint8_t*)ent.DllBase + dos_header.e_lfanew);

    IMAGE_DATA_DIRECTORY exp_dir;
    bool found = false;

    if (nt_header.OptionalHeader32.Magic == IMAGE_NT_OPTIONAL_HDR64_MAGIC) {
        if (nt_header.OptionalHeader64.NumberOfRvaAndSizes > IMAGE_DIRECTORY_ENTRY_IMPORT) {
            exp_dir = nt_header.OptionalHeader64.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
            found = true;
        }
    } else {
        if (nt_header.OptionalHeader32.NumberOfRvaAndSizes > IMAGE_DIRECTORY_ENTRY_IMPORT) {
            exp_dir = nt_header.OptionalHeader32.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
            found = true;
        }
    }

    if (!found || exp_dir.VirtualAddress == 0 || exp_dir.Size < sizeof(IMAGE_IMPORT_DESCRIPTOR))
        return;

    auto iids = span((IMAGE_IMPORT_DESCRIPTOR*)((uint8_t*)ent.DllBase + exp_dir.VirtualAddress),
                     exp_dir.Size / sizeof(IMAGE_IMPORT_DESCRIPTOR));

    for (auto& iid : iids) {
        if (iid.Characteristics == 0)
            break;

        auto name = string_view((char*)ent.DllBase + iid.Name);

        if (name.empty()) {
            print_stderr("DLL with empty name.\n");
            NtTerminateProcess(NtCurrentProcess(), nt_status::DLL_NOT_FOUND);
            unreachable();
        }

        if (!load_dll(name)) {
            print_stderr("Could not load {}.\n", name);
            NtTerminateProcess(NtCurrentProcess(), nt_status::DLL_NOT_FOUND);
            unreachable();
        }
    }
}

export void load_dlls() {
    auto le = ldr_data.InLoadOrderModuleList.Flink;

    while (le != &ldr_data.InLoadOrderModuleList) {
        auto& ent = *CONTAINING_RECORD(le, LDR_DATA_TABLE_ENTRY, InLoadOrderLinks);

        load_dlls_img(ent);

        le = le->Flink;
    }
}

extern "C"
NTSTATUS LdrGetProcedureAddress(void* dll_handle, const ANSI_STRING* procedure_name,
                                uint32_t /*procedure_number*/, void** procedure_address) {
    auto& dos_header = *(IMAGE_DOS_HEADER*)dll_handle;
    auto& nt_header = *(IMAGE_NT_HEADERS*)((uint8_t*)dll_handle + dos_header.e_lfanew);
    uintptr_t dir_addr;

    // FIXME - searching by procedure number

    if (!procedure_name)
        return nt_status::INVALID_PARAMETER;

    string_view name(procedure_name->Buffer, procedure_name->Length);

    if (name.empty())
        return nt_status::INVALID_PARAMETER;

    if (nt_header.OptionalHeader32.Magic == IMAGE_NT_OPTIONAL_HDR64_MAGIC) {
        if (nt_header.OptionalHeader64.NumberOfRvaAndSizes <= IMAGE_DIRECTORY_ENTRY_EXPORT)
            return nt_status::PROCEDURE_NOT_FOUND;

        dir_addr = dos_header.e_lfanew + offsetof(IMAGE_NT_HEADERS, OptionalHeader64.DataDirectory);
    } else {
        if (nt_header.OptionalHeader32.NumberOfRvaAndSizes <= IMAGE_DIRECTORY_ENTRY_EXPORT)
            return nt_status::PROCEDURE_NOT_FOUND;

        dir_addr = dos_header.e_lfanew + offsetof(IMAGE_NT_HEADERS, OptionalHeader32.DataDirectory);
    }

    dir_addr += IMAGE_DIRECTORY_ENTRY_EXPORT * sizeof(IMAGE_DATA_DIRECTORY);

    auto& data_dir = *(IMAGE_DATA_DIRECTORY*)((uint8_t*)dll_handle + dir_addr);

    if (data_dir.VirtualAddress == 0 || data_dir.Size == 0)
        return nt_status::PROCEDURE_NOT_FOUND;

    // search export directory

    const auto& ied = *(IMAGE_EXPORT_DIRECTORY*)((uint8_t*)dll_handle + data_dir.VirtualAddress);

    if (ied.NumberOfNames == 0)
        return nt_status::PROCEDURE_NOT_FOUND;

    auto names = span((uint32_t*)((uint8_t*)&ied + ied.AddressOfNames - data_dir.VirtualAddress),
                      ied.NumberOfNames);

    if ((uint8_t*)names.data() + names.size_bytes() > (uint8_t*)&ied + data_dir.Size)
        return nt_status::INTERNAL_ERROR;

    auto name_ordinals = span((uint16_t*)((uint8_t*)&ied + ied.AddressOfNameOrdinals - data_dir.VirtualAddress),
                              ied.NumberOfNames);

    if ((uint8_t*)name_ordinals.data() + name_ordinals.size_bytes() > (uint8_t*)&ied + data_dir.Size)
        return nt_status::INTERNAL_ERROR;

    auto funcs = span((uint32_t*)((uint8_t*)&ied + ied.AddressOfFunctions - data_dir.VirtualAddress),
                      ied.NumberOfFunctions);

    if ((uint8_t*)funcs.data() + funcs.size_bytes() > (uint8_t*)&ied + data_dir.Size)
        return nt_status::INTERNAL_ERROR;

    optional<uint16_t> ord;

    for (unsigned int i = 0; i < names.size(); i++) {
        auto n = names[i];

        if (n - data_dir.VirtualAddress > data_dir.Size)
            return nt_status::INTERNAL_ERROR;

        auto sv = string_view((char*)(uint8_t*)&ied + n - data_dir.VirtualAddress,
                              data_dir.Size + data_dir.VirtualAddress - n);

        auto nulpos = sv.find_first_of((char)0);

        if (nulpos != string_view::npos)
            sv = string_view(sv.data(), nulpos);

        if (sv == name) {
            ord = name_ordinals[i];
            break;
        }
    }

    if (!ord)
        return nt_status::PROCEDURE_NOT_FOUND;

    // ordinal out of range
    if (*ord >= ied.NumberOfFunctions)
        return nt_status::PROCEDURE_NOT_FOUND;

    auto rva = funcs[*ord];

    if (rva >= data_dir.VirtualAddress && rva < data_dir.VirtualAddress + data_dir.Size) {
        print_stderr("FIXME - resolving forwards in LdrGetProcedureAddress\n"); // FIXME
        return nt_status::INTERNAL_ERROR;
    }

    *procedure_address = (uint8_t*)dll_handle + rva;

    return nt_status::SUCCESS;
}

template<typename T>
static nt_status resolve_dll_forward(string_view fwd, T& val) {
    string_view dll, name;

    static const string_view suffix = ".dll";

    if (auto dot = fwd.find("."); dot == string_view::npos) {
        print_stderr("could not resolve DLL forward \"{}\"\n", fwd);
        return nt_status::PROCEDURE_NOT_FOUND;
    } else {
        dll = string_view(fwd.data(), dot);
        name = string_view(fwd.data() + dot + 1, fwd.size() - dot - 1);
    }

    if (!name.empty() && name.front() == '#') {
        print_stderr("resolve_dll_forward: FIXME - resolving DLL forwards by ordinal\n"); // FIXME
        return nt_status::INTERNAL_ERROR;
    }

    auto dll2 = (char*)malloc(dll.size() + suffix.size());
    if (!dll2) {
        print_stderr("resolve_dll_forward: malloc failed\n");
        return nt_status::INSUFFICIENT_RESOURCES;
    }

    memcpy(dll2, dll.data(), dll.size());
    memcpy(dll2 + dll.size(), suffix.data(), suffix.size());

    auto dllsv = string_view(dll2, dll.size() + suffix.size());

    auto pent = load_dll(dllsv);
    if (!pent) {
        free(dll2);
        print_stderr("resolve_dll_forward: could not load DLL {}\n", dll);
        return nt_status::PROCEDURE_NOT_FOUND;
    }

    free(dll2);

    auto& ent = *pent;
    void* addr;
    ANSI_STRING procname;

    procname.Length = procname.MaximumLength = (uint16_t)name.size();
    procname.Buffer = (char*)name.data();

    auto status = LdrGetProcedureAddress(ent.ldr.DllBase, &procname, 0, &addr);
    if (!success(status)) {
        print_stderr("resolve_dll_forward: LdrGetProcedureAddress returned {} for {} in {}\n",
                     status, name, dll);
        return status;
    }

    val = (T)(uintptr_t)addr;

    return nt_status::SUCCESS;
}

template<typename T>
static nt_status resolve_imports_dll(void* base, void* dll_base, string_view name, T* lookup_table, T* thunks) {
    IMAGE_DATA_DIRECTORY exp_dir;
    bool found = false;

    if (!*lookup_table)
        return nt_status::SUCCESS;

    const auto& dos_header = *(IMAGE_DOS_HEADER*)dll_base;
    const auto& nt_header = *(IMAGE_NT_HEADERS*)((uint8_t*)dll_base + dos_header.e_lfanew);

    if (nt_header.OptionalHeader32.Magic == IMAGE_NT_OPTIONAL_HDR64_MAGIC) {
        if (nt_header.OptionalHeader64.NumberOfRvaAndSizes > IMAGE_DIRECTORY_ENTRY_EXPORT) {
            exp_dir = nt_header.OptionalHeader64.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];
            found = true;
        }
    } else {
        if (nt_header.OptionalHeader32.NumberOfRvaAndSizes > IMAGE_DIRECTORY_ENTRY_EXPORT) {
            exp_dir = nt_header.OptionalHeader32.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];
            found = true;
        }
    }

    if (!found || exp_dir.VirtualAddress == 0) {
        print_stderr("no export directory in {}\n", name);
        return nt_status::PROCEDURE_NOT_FOUND;
    }

    if (exp_dir.Size < sizeof(IMAGE_EXPORT_DIRECTORY)) {
        print_stderr("invalid export directory in {}\n", name);
        return nt_status::PROCEDURE_NOT_FOUND;
    }

    const auto& ied = *(IMAGE_EXPORT_DIRECTORY*)((uint8_t*)dll_base + exp_dir.VirtualAddress);

    if (ied.NumberOfFunctions == 0) {
        print_stderr("no exports in {}\n", name);
        return nt_status::PROCEDURE_NOT_FOUND;
    }

    auto names = span((uint32_t*)((uint8_t*)dll_base + ied.AddressOfNames), ied.NumberOfNames);

    auto name_ordinals = span((uint16_t*)((uint8_t*)dll_base + ied.AddressOfNameOrdinals), ied.NumberOfNames);

    auto funcs = span((uint32_t*)((uint8_t*)dll_base + ied.AddressOfFunctions), ied.NumberOfFunctions);

    while (*lookup_table) {
        auto v = *lookup_table;

        if (v & ((T)1 << ((sizeof(T) * 8) - 1))) {
            auto ord = (uint16_t)(v & 0xffff);

            if (ord < ied.Base || ord >= ied.Base + ied.NumberOfFunctions || funcs[ord - ied.Base] == 0) {
                print_stderr("Ordinal {} in {} not found.\n", ord, name);
                return nt_status::ORDINAL_NOT_FOUND;
            }

            auto addr = funcs[ord - ied.Base];

            if (addr >= exp_dir.VirtualAddress && addr < exp_dir.VirtualAddress + exp_dir.Size) {
                auto status = resolve_dll_forward<T>(string_view((char*)((uintptr_t)dll_base + addr)), *thunks);
                if (!success(status))
                    return status;
            } else
                *thunks = (T)((uintptr_t)dll_base + addr);

            lookup_table++;
            thunks++;

            continue;
        }

        // FIXME - don't ignore hint
        auto func = string_view((char*)base + (v & 0x7fffffff) + sizeof(char16_t));

        optional<uint16_t> ord;

        for (unsigned int i = 0; i < names.size(); i++) {
            auto n = names[i];

            auto sv = string_view((char*)dll_base + n);

            if (sv == func) {
                ord = name_ordinals[i];
                break;
            }
        }

        if (!ord || funcs[*ord] == 0) {
            print_stderr("Could not resolve {} in {}.\n", func, name);
            return nt_status::PROCEDURE_NOT_FOUND;
        }

        auto addr = funcs[*ord];

        if (addr >= exp_dir.VirtualAddress && addr < exp_dir.VirtualAddress + exp_dir.Size) {
            auto status = resolve_dll_forward<T>(string_view((char*)((uintptr_t)dll_base + addr)), *thunks);
            if (!success(status))
                return status;
        } else
            *thunks = (T)((uintptr_t)dll_base + addr);

        lookup_table++;
        thunks++;
    }

    return nt_status::SUCCESS;
}

static nt_status resolve_imports_img(void* base) {
    nt_status status;
    auto& dos_header = *(IMAGE_DOS_HEADER*)base;
    auto& nt_header = *(IMAGE_NT_HEADERS*)((uint8_t*)base + dos_header.e_lfanew);

    IMAGE_DATA_DIRECTORY exp_dir;
    bool found = false;

    if (nt_header.OptionalHeader32.Magic == IMAGE_NT_OPTIONAL_HDR64_MAGIC) {
        if (nt_header.OptionalHeader64.NumberOfRvaAndSizes > IMAGE_DIRECTORY_ENTRY_IMPORT) {
            exp_dir = nt_header.OptionalHeader64.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
            found = true;
        }
    } else {
        if (nt_header.OptionalHeader32.NumberOfRvaAndSizes > IMAGE_DIRECTORY_ENTRY_IMPORT) {
            exp_dir = nt_header.OptionalHeader32.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
            found = true;
        }
    }

    if (!found || exp_dir.VirtualAddress == 0 || exp_dir.Size < sizeof(IMAGE_IMPORT_DESCRIPTOR))
        return nt_status::SUCCESS;

    auto iids = span((IMAGE_IMPORT_DESCRIPTOR*)((uint8_t*)base + exp_dir.VirtualAddress),
                     exp_dir.Size / sizeof(IMAGE_IMPORT_DESCRIPTOR));

    for (auto& iid : iids) {
        if (iid.Characteristics == 0)
            break;

        auto name = string_view((char*)base + iid.Name); // FIXME - to UTF-16
        LDR_DATA_TABLE_ENTRY* ent = nullptr;

        auto u16len = utf8_to_utf16_len(name);

        if (u16len == 0) {
            print_stderr("DLL name is empty.\n");
            return nt_status::DLL_NOT_FOUND;
        }

        auto u16buf = (char16_t*)malloc(u16len * sizeof(char16_t));

        if (!u16buf)
            return nt_status::INSUFFICIENT_RESOURCES;

        utf8_to_utf16(span(u16buf, u16len), name);

        auto u16sv = u16string_view(u16buf, u16len);

        {
            auto le = ldr_data.InLoadOrderModuleList.Flink;

            while (le != &ldr_data.InLoadOrderModuleList) {
                auto& ent2 = *CONTAINING_RECORD(le, LDR_DATA_TABLE_ENTRY, InLoadOrderLinks);
                auto base_name = u16string_view(ent2.BaseDllName.Buffer, ent2.BaseDllName.Length / sizeof(char16_t));

                if (base_name == u16sv) {
                    ent = &ent2;
                    break;
                }

                le = le->Flink;
            }
        }

        free(u16buf);

        if (!ent) {
            print_stderr("{} not loaded.\n", name);
            return nt_status::DLL_NOT_FOUND;
        }

        if (nt_header.OptionalHeader32.Magic == IMAGE_NT_OPTIONAL_HDR64_MAGIC) {
            status = resolve_imports_dll(base, ent->DllBase, name,
                                         (uint64_t*)((uint8_t*)base + iid.Characteristics),
                                         (uint64_t*)((uint8_t*)base + iid.FirstThunk));
        } else {
            status = resolve_imports_dll(base, ent->DllBase, name,
                                         (uint32_t*)((uint8_t*)base + iid.Characteristics),
                                         (uint32_t*)((uint8_t*)base + iid.FirstThunk));
        }

        if (!success(status))
            return status;
    }

    return nt_status::SUCCESS;
}

export nt_status resolve_imports() {
    nt_status status;
    auto le = ldr_data.InLoadOrderModuleList.Flink;

    while (le != &ldr_data.InLoadOrderModuleList) {
        auto& ent = *CONTAINING_RECORD(le, LDR_DATA_TABLE_ENTRY, InLoadOrderLinks);

        status = resolve_imports_img(ent.DllBase);
        if (!success(status))
            return status;

        le = le->Flink;
    }

    return nt_status::SUCCESS;
}

static nt_status resolve_relocations_img(void* base) {
    auto& dos_header = *(IMAGE_DOS_HEADER*)base;
    auto& nt_header = *(IMAGE_NT_HEADERS*)((uint8_t*)base + dos_header.e_lfanew);

    uint64_t header_base;
    span<uint8_t> dir;

    if (nt_header.OptionalHeader32.Magic == IMAGE_NT_OPTIONAL_HDR64_MAGIC) {
        if (nt_header.OptionalHeader64.NumberOfRvaAndSizes <= IMAGE_DIRECTORY_ENTRY_BASERELOC ||
            nt_header.OptionalHeader64.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC].VirtualAddress == 0 ||
            nt_header.OptionalHeader64.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC].Size < sizeof(IMAGE_BASE_RELOCATION)) {
            return nt_status::SUCCESS;
        }

        dir = span((uint8_t*)base + nt_header.OptionalHeader64.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC].VirtualAddress,
                   nt_header.OptionalHeader64.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC].Size);

        header_base = nt_header.OptionalHeader64.ImageBase;
    } else {
        if (nt_header.OptionalHeader32.NumberOfRvaAndSizes <= IMAGE_DIRECTORY_ENTRY_BASERELOC ||
            nt_header.OptionalHeader32.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC].VirtualAddress == 0 ||
            nt_header.OptionalHeader32.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC].Size < sizeof(IMAGE_BASE_RELOCATION)) {
            return nt_status::SUCCESS;
        }

        dir = span((uint8_t*)base + nt_header.OptionalHeader32.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC].VirtualAddress,
                   nt_header.OptionalHeader32.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC].Size);

        header_base = nt_header.OptionalHeader32.ImageBase;
    }

    if ((uintptr_t)base == header_base) // not relocated
        return nt_status::SUCCESS;

    do {
        auto& reloc = *(IMAGE_BASE_RELOCATION*)dir.data();

        if (dir.size() < reloc.SizeOfBlock)
            break;

        auto rs = span((uint16_t*)((uint8_t*)&reloc + sizeof(IMAGE_BASE_RELOCATION)), (reloc.SizeOfBlock - sizeof(IMAGE_BASE_RELOCATION)) / sizeof(uint16_t));

        for (const auto& r : rs) {
            uint16_t offset = r & 0xfff;
            uint16_t type = r >> 12;

            switch (type) {
                case IMAGE_REL_BASED_ABSOLUTE:
                    // nop
                break;

                case IMAGE_REL_BASED_HIGHLOW: {
                    uint32_t* ptr2 = (uint32_t*)((uint8_t*)base + reloc.VirtualAddress + offset);

                    *ptr2 = (uint32_t)(*ptr2 - header_base + (uintptr_t)base);
                    break;
                }

                case IMAGE_REL_BASED_DIR64: {
                    uint64_t* ptr2 = (uint64_t*)((uint8_t*)base + reloc.VirtualAddress + offset);

                    *ptr2 = *ptr2 - header_base + (uintptr_t)base;
                    break;
                }

                default:
                    print_stderr("Unsupported relocation type {:x}.\n", type);
                    return nt_status::ILLEGAL_DLL_RELOCATION;
            }
        }

        dir = dir.subspan(reloc.SizeOfBlock);

        if (dir.size() < sizeof(IMAGE_BASE_RELOCATION))
            break;
    } while (true);

    // FIXME - do we need to update the base in the header?

    return nt_status::SUCCESS;
}

export nt_status resolve_relocations() {
    nt_status status;
    auto le = ldr_data.InLoadOrderModuleList.Flink;

    while (le != &ldr_data.InLoadOrderModuleList) {
        auto& ent = *CONTAINING_RECORD(le, LDR_DATA_TABLE_ENTRY, InLoadOrderLinks);

        status = resolve_relocations_img(ent.DllBase);
        if (!success(status))
            return status;

        le = le->Flink;
    }

    return nt_status::SUCCESS;
}

static nt_status toggle_protections_img(void* base, bool enable) {
    auto& dos_header = *(IMAGE_DOS_HEADER*)base;
    auto& nt_header = *(IMAGE_NT_HEADERS*)((uint8_t*)base + dos_header.e_lfanew);
    span<IMAGE_SECTION_HEADER> sections;

    if (nt_header.OptionalHeader32.Magic == IMAGE_NT_OPTIONAL_HDR64_MAGIC)
        sections = span((IMAGE_SECTION_HEADER*)((uint8_t*)&nt_header.OptionalHeader64 + nt_header.FileHeader.SizeOfOptionalHeader), nt_header.FileHeader.NumberOfSections);
    else
        sections = span((IMAGE_SECTION_HEADER*)((uint8_t*)&nt_header.OptionalHeader32 + nt_header.FileHeader.SizeOfOptionalHeader), nt_header.FileHeader.NumberOfSections);

    for (const auto& sect : sections) {
        auto section_size = min(sect.VirtualSize, sect.SizeOfRawData);

        if (section_size == 0 || sect.PointerToRawData == 0)
            continue;

        void* addr = (uint8_t*)base + sect.VirtualAddress;
        size_t size = section_size;
        uint32_t new_prot = 0, old_prot;

        if (enable) {
            if (sect.Characteristics & IMAGE_SCN_MEM_WRITE && !(sect.Characteristics & IMAGE_SCN_MEM_EXECUTE))
                new_prot = PAGE_READWRITE;
            else if (sect.Characteristics & IMAGE_SCN_MEM_WRITE && sect.Characteristics & IMAGE_SCN_MEM_EXECUTE)
                new_prot = PAGE_EXECUTE_READWRITE;
            else if (!(sect.Characteristics & IMAGE_SCN_MEM_WRITE) && !(sect.Characteristics & IMAGE_SCN_MEM_EXECUTE))
                new_prot = PAGE_READONLY;
            else if (!(sect.Characteristics & IMAGE_SCN_MEM_WRITE) && sect.Characteristics & IMAGE_SCN_MEM_EXECUTE)
                new_prot = PAGE_EXECUTE_READ;
        } else
            new_prot = sect.Characteristics & IMAGE_SCN_MEM_EXECUTE ? PAGE_EXECUTE_READWRITE : PAGE_READWRITE;

        auto ret = NtProtectVirtualMemory(NtCurrentProcess(), &addr, &size, new_prot, &old_prot);

        if (!success(ret)) {
            print_stderr("NtProtectVirtualMemory returned {}.\n", ret);
            return ret;
        }
    }

    return nt_status::SUCCESS;
}

export nt_status toggle_protections(bool enable) {
    nt_status status;
    auto le = ldr_data.InLoadOrderModuleList.Flink;

    while (le != &ldr_data.InLoadOrderModuleList) {
        auto& ent = *CONTAINING_RECORD(le, LDR_DATA_TABLE_ENTRY, InLoadOrderLinks);

        status = toggle_protections_img(ent.DllBase, enable);
        if (!success(status))
            return status;

        le = le->Flink;
    }

    return nt_status::SUCCESS;
}

export void init_ldr_data(mn_image_info& ntdll_info, mn_image_info& img_info) {
    auto& peb = *NtCurrentTeb()->ProcessEnvironmentBlock;
    auto& proc_params = *peb.ProcessParameters;

    ldr_data.Length = sizeof(ldr_data);
    ldr_data.Initialized = true;
    InitializeListHead(&ldr_data.InLoadOrderModuleList);
    InitializeListHead(&ldr_data.InMemoryOrderModuleList);
    InitializeListHead(&ldr_data.InInitializationOrderModuleList);

    u16string_view image_path(proc_params.ImagePathName.Buffer,
                              proc_params.ImagePathName.Length / sizeof(char16_t));
    u16string_view base_name = image_path;

    if (auto pos = base_name.find_last_of('\\'); pos != u16string_view::npos)
        base_name = u16string_view(base_name.data() + pos + 1, base_name.size() - pos - 1);

    if (auto pos = base_name.find_last_of('/'); pos != u16string_view::npos)
        base_name = u16string_view(base_name.data() + pos + 1, base_name.size() - pos - 1);

    auto* pntdll = add_ldr_entry(ntdll_info.base, u"\\lib\\ntdll.dll", u"ntdll.dll",
                                 true, true); // FIXME - name and path
    if (!pntdll) {
        NtTerminateProcess(NtCurrentProcess(), nt_status::INSUFFICIENT_RESOURCES);
        unreachable();
    }

    auto& ntdll = *pntdll;

    ntdll.offset_debug_info = ntdll_info.offset_debug_info;
    ntdll.size_debug_info = ntdll_info.size_debug_info;
    ntdll.offset_debug_abbrev = ntdll_info.offset_debug_abbrev;
    ntdll.size_debug_abbrev = ntdll_info.size_debug_abbrev;
    ntdll.offset_debug_rnglists = ntdll_info.offset_debug_rnglists;
    ntdll.size_debug_rnglists = ntdll_info.size_debug_rnglists;
    ntdll.offset_debug_str = ntdll_info.offset_debug_str;
    ntdll.size_debug_str = ntdll_info.size_debug_str;
    ntdll.offset_debug_addr = ntdll_info.offset_debug_addr;
    ntdll.size_debug_addr = ntdll_info.size_debug_addr;
    ntdll.offset_debug_str_offsets = ntdll_info.offset_debug_str_offsets;
    ntdll.size_debug_str_offsets = ntdll_info.size_debug_str_offsets;
    ntdll.offset_debug_line = ntdll_info.offset_debug_line;
    ntdll.size_debug_line = ntdll_info.size_debug_line;
    ntdll.offset_debug_line_str = ntdll_info.offset_debug_line_str;
    ntdll.size_debug_line_str = ntdll_info.size_debug_line_str;

    auto* pimg = add_ldr_entry(img_info.base, image_path, base_name, true, false);

    if (!pimg) {
        NtTerminateProcess(NtCurrentProcess(), nt_status::INSUFFICIENT_RESOURCES);
        unreachable();
    }

    auto& img = *pimg;

    img.offset_debug_info = img_info.offset_debug_info;
    img.size_debug_info = img_info.size_debug_info;
    img.offset_debug_abbrev = img_info.offset_debug_abbrev;
    img.size_debug_abbrev = img_info.size_debug_abbrev;
    img.offset_debug_rnglists = img_info.offset_debug_rnglists;
    img.size_debug_rnglists = img_info.size_debug_rnglists;
    img.offset_debug_str = img_info.offset_debug_str;
    img.size_debug_str = img_info.size_debug_str;
    img.offset_debug_addr = img_info.offset_debug_addr;
    img.size_debug_addr = img_info.size_debug_addr;
    img.offset_debug_str_offsets = img_info.offset_debug_str_offsets;
    img.size_debug_str_offsets = img_info.size_debug_str_offsets;
    img.offset_debug_line = img_info.offset_debug_line;
    img.size_debug_line = img_info.size_debug_line;
    img.offset_debug_line_str = img_info.offset_debug_line_str;
    img.size_debug_line_str = img_info.size_debug_line_str;

    InsertTailList(&ldr_data.InInitializationOrderModuleList,
                   &ntdll.ldr.InInitializationOrderLinks);
    InsertTailList(&ldr_data.InInitializationOrderModuleList,
                   &img.ldr.InInitializationOrderLinks);
}

static nt_status do_process_attach(LDR_DATA_TABLE_ENTRY& ent) {
    call_tls_callbacks(ent, DLL_PROCESS_ATTACH);

    auto dll_main = (DLL_ENTRY_PROC)ent.EntryPoint;

    if (!dll_main(ent.DllBase, DLL_PROCESS_ATTACH, (void*)1))
        return nt_status::DLL_INIT_FAILED;

    InsertTailList(&ldr_data.InInitializationOrderModuleList,
                   &ent.InInitializationOrderLinks);

    ent.ProcessAttachCalled = 1;

    return nt_status::SUCCESS;
}

static nt_status call_process_attach_holding_lock() {
    for (auto le = ldr_data.InLoadOrderModuleList.Flink; le != &ldr_data.InLoadOrderModuleList; le = le->Flink) {
        auto& ent = *CONTAINING_RECORD(le, LDR_DATA_TABLE_ENTRY, InLoadOrderLinks);

        if (!ent.LoadInProgress && !ent.ProcessAttachCalled) {
            ent.LoadInProgress = 1;

            auto status = do_process_attach(ent);
            if (!success(status))
                return status;
        }
    }

    return nt_status::SUCCESS;
}

export nt_status call_process_attach() {
    mn_loader_lock lock;
    lock_guard lg(lock);

    return call_process_attach_holding_lock();
}

static nt_status add_imports_to_list(dll_entry& de, list_entry& dlls) {
    auto& ent = de.ent->ldr;
    auto& dos_header = *(IMAGE_DOS_HEADER*)ent.DllBase;
    auto& nt_header = *(IMAGE_NT_HEADERS*)((uint8_t*)ent.DllBase + dos_header.e_lfanew);

    IMAGE_DATA_DIRECTORY exp_dir;
    bool found = false;

    if (nt_header.OptionalHeader32.Magic == IMAGE_NT_OPTIONAL_HDR64_MAGIC) {
        if (nt_header.OptionalHeader64.NumberOfRvaAndSizes > IMAGE_DIRECTORY_ENTRY_IMPORT) {
            exp_dir = nt_header.OptionalHeader64.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
            found = true;
        }
    } else {
        if (nt_header.OptionalHeader32.NumberOfRvaAndSizes > IMAGE_DIRECTORY_ENTRY_IMPORT) {
            exp_dir = nt_header.OptionalHeader32.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
            found = true;
        }
    }

    if (!found || exp_dir.VirtualAddress == 0 || exp_dir.Size < sizeof(IMAGE_IMPORT_DESCRIPTOR))
        return nt_status::SUCCESS;

    auto iids = span((IMAGE_IMPORT_DESCRIPTOR*)((uint8_t*)ent.DllBase + exp_dir.VirtualAddress),
                     exp_dir.Size / sizeof(IMAGE_IMPORT_DESCRIPTOR));

    for (auto& iid : iids) {
        if (iid.Characteristics == 0)
            break;

        de.num_deps++;
    }

    if (de.num_deps == 0)
        return nt_status::SUCCESS;

    de.deps.reset(new dll_entry*[de.num_deps]);
    if (!de.deps)
        return nt_status::INSUFFICIENT_RESOURCES;

    size_t i = 0;

    for (auto& iid : iids) {
        if (iid.Characteristics == 0)
            break;

        auto name = string_view((char*)ent.DllBase + iid.Name);

        if (name.empty()) {
            print_stderr("DLL with empty name.\n");
            return nt_status::DLL_NOT_FOUND;
        }

        auto* ent2 = load_dll(name);
        if (!ent2) {
            print_stderr("Could not load {}.\n", name);
            return nt_status::DLL_NOT_FOUND;
        }

        bool already_there = false;

        for (auto le = dlls.next; le != &dlls; le = le->next) {
            auto& de2 = containing_record<dll_entry>(le);

            if (de2.ent == ent2) {
                de2.refs++;
                already_there = true;

                de.deps[i] = &de2;
                i++;

                break;
            }
        }

        if (!already_there) {
            auto de2 = new dll_entry;
            if (!de2) {
                if (ent2->ldr.LoadCount == 1) {
                    NtUnmapViewOfSection(NtCurrentProcess(), ent2->ldr.DllBase);
                    RemoveEntryList(&ent2->ldr.InLoadOrderLinks);
                    RemoveEntryList(&ent2->ldr.InMemoryOrderLinks);
                    free(ent2);
                } else
                    ent2->ldr.LoadCount--;

                return nt_status::INSUFFICIENT_RESOURCES;
            }

            de2->ent = ent2;
            de2->refs = 1;
            de2->new_entry = ent2->ldr.LoadCount == 1;
            insert_tail_list(dlls, de2->le);

            de.deps[i] = de2;
            i++;
        }
    }

    return nt_status::SUCCESS;
}

static nt_status dll_entry_attach(dll_entry& de) {
    nt_status status;

    if (de.ent->ldr.LoadInProgress || de.ent->ldr.ProcessAttachCalled)
        return nt_status::SUCCESS;

    de.ent->ldr.LoadInProgress = 1;

    for (size_t i = 0; i < de.num_deps; i++) {
        status = dll_entry_attach(*de.deps[i]);
        if (!success(status))
            return status;
    }

    status = do_process_attach(de.ent->ldr);
    if (!success(status))
        return status;

    return nt_status::SUCCESS;
}

static void dll_entry_detach(LIST_ENTRY* last_init_order_entry) {
    while (ldr_data.InInitializationOrderModuleList.Blink != last_init_order_entry) {
        auto& ent = *CONTAINING_RECORD(ldr_data.InInitializationOrderModuleList.Blink,
                                       LDR_DATA_TABLE_ENTRY, InInitializationOrderLinks);

        if (ent.ProcessAttachCalled)
            call_process_detach_img(ent, false);

        RemoveEntryList(&ent.InInitializationOrderLinks);
    }
}

static nt_status alloc_thread_tls_dynamic_mod(LDR_DATA_TABLE_ENTRY& ent) {
    auto& dos_header = *(IMAGE_DOS_HEADER*)ent.DllBase;
    auto& nt_header = *(IMAGE_NT_HEADERS*)((uint8_t*)ent.DllBase + dos_header.e_lfanew);
    uintptr_t dir_addr;

    if (nt_header.OptionalHeader32.Magic == IMAGE_NT_OPTIONAL_HDR64_MAGIC) {
        if (nt_header.OptionalHeader64.NumberOfRvaAndSizes <= IMAGE_DIRECTORY_ENTRY_TLS)
            return nt_status::SUCCESS;

        dir_addr = dos_header.e_lfanew + offsetof(IMAGE_NT_HEADERS, OptionalHeader64.DataDirectory);
    } else {
        if (nt_header.OptionalHeader32.NumberOfRvaAndSizes <= IMAGE_DIRECTORY_ENTRY_TLS)
            return nt_status::SUCCESS;

        dir_addr = dos_header.e_lfanew + offsetof(IMAGE_NT_HEADERS, OptionalHeader32.DataDirectory);
    }

    dir_addr += IMAGE_DIRECTORY_ENTRY_TLS * sizeof(IMAGE_DATA_DIRECTORY);

    auto& data_dir = *(IMAGE_DATA_DIRECTORY*)((uint8_t*)ent.DllBase + dir_addr);

    if (data_dir.VirtualAddress == 0 || data_dir.Size == 0)
        return nt_status::SUCCESS;

    // FIXME - IMAGE_TLS_DIRECTORY32 if 32-bit

    if (data_dir.Size < sizeof(IMAGE_TLS_DIRECTORY64)) {
        print_stderr("alloc_thread_tls_dynamic_mod: TLS directory was {} bytes, expected {}\n",
                     data_dir.Size, sizeof(IMAGE_TLS_DIRECTORY64));
        return nt_status::INVALID_IMAGE_FORMAT;
    }

    auto& dir = *(IMAGE_TLS_DIRECTORY64*)((uint8_t*)ent.DllBase + data_dir.VirtualAddress);

    size_t size = dir.EndAddressOfRawData - dir.StartAddressOfRawData + dir.SizeOfZeroFill;

    if (size == 0 && dir.AddressOfCallBacks == 0)
        return nt_status::SUCCESS;

    auto new_tls_dirs = (IMAGE_TLS_DIRECTORY64**)RtlAllocateHeap(GetProcessHeap(), 0,
                                                                 sizeof(IMAGE_TLS_DIRECTORY64*) * (num_tls_dirs + 1));
    if (!new_tls_dirs) {
        print_stderr("RtlAllocateHeap failed\n");
        return nt_status::INSUFFICIENT_RESOURCES;
    }

    memcpy(new_tls_dirs, tls_dirs, num_tls_dirs * sizeof(IMAGE_TLS_DIRECTORY64*));

    new_tls_dirs[num_tls_dirs] = &dir;
    ent.TlsIndex = num_tls_dirs;

    for (auto le = tls_list.Flink; le != &tls_list; le = le->Flink) {
        auto& teb = *CONTAINING_RECORD(le, TEB, TlsLinks);

        auto ptr = (void**)malloc(sizeof(void*) * (num_tls_dirs + 1));
        if (!ptr)
            return nt_status::INSUFFICIENT_RESOURCES;

        memcpy(ptr, teb.ThreadLocalStoragePointer, sizeof(void*) * num_tls_dirs);

        auto& dir = *new_tls_dirs[ent.TlsIndex];
        auto& p = ptr[ent.TlsIndex];

        p = malloc(size);
        if (!p)
            return nt_status::INSUFFICIENT_RESOURCES;

        memcpy(p, (void*)dir.StartAddressOfRawData,
                dir.EndAddressOfRawData - dir.StartAddressOfRawData);
        memset((uint8_t*)p + size - dir.SizeOfZeroFill, 0, dir.SizeOfZeroFill);

        teb.ThreadLocalStoragePointer = ptr; // let it leak
    }

    *(uint16_t*)dir.AddressOfIndex = ent.TlsIndex;

    tls_dirs = new_tls_dirs; // let it leak
    num_tls_dirs++;

    return nt_status::SUCCESS;
}

extern "C"
nt_status LdrLoadDll(const char16_t* /*dll_path*/, uint32_t* /*dll_characteristics*/,
                     const UNICODE_STRING* dll_name, void** dll_handle) {
    nt_status status;
    auto name = u16string_view(dll_name->Buffer, dll_name->Length / sizeof(char16_t));

    list_entry dlls = { &dlls, &dlls };

    // FIXME - dll_path
    // FIXME - dll_characteristics(?)

    {
        mn_peb_lock lock1;
        mn_loader_lock lock2;
        lock_guard lg1(lock1);
        lock_guard lg2(lock2);

        auto last_init_order_entry = ldr_data.InInitializationOrderModuleList.Blink;

        auto ent = load_dll_u16(name);

        if (!ent)
            return nt_status::DLL_NOT_FOUND;

        if (ent->ldr.LoadCount > 1) { // already loaded
            *dll_handle = ent->ldr.DllBase;
            return nt_status::SUCCESS;
        }

        auto de = new dll_entry;
        if (!de)
            return nt_status::INSUFFICIENT_RESOURCES;

        de->ent = ent;
        de->refs = 1;
        de->new_entry = true;

        insert_tail_list(dlls, de->le);

        auto failure = [&]() {
            dll_entry_detach(last_init_order_entry);

            while (!is_list_empty(dlls)) {
                auto& de2 = containing_record<dll_entry>(dlls.next);

                remove_entry_list(de2.le);

                if (de2.new_entry) {
                    NtUnmapViewOfSection(NtCurrentProcess(), de2.ent->ldr.DllBase);
                    RemoveEntryList(&de2.ent->ldr.InLoadOrderLinks);
                    RemoveEntryList(&de2.ent->ldr.InMemoryOrderLinks);

                    free(de2.ent);
                } else
                    de2.ent->ldr.LoadCount -= (uint16_t)de2.refs;

                delete &de2;
            }
        };

        for (auto le = dlls.next; le != &dlls; le = le->next) {
            auto& de2 = containing_record<dll_entry>(le);

            if (!de2.new_entry)
                continue;

            status = toggle_protections_img(de2.ent->ldr.DllBase, false);
            if (!success(status)) {
                failure();
                return status;
            }

            status = resolve_relocations_img(de2.ent->ldr.DllBase);
            if (!success(status)) {
                failure();
                return status;
            }

            status = add_imports_to_list(de2, dlls);
            if (!success(status)) {
                failure();
                return status;
            }
        }

        for (auto le = dlls.next; le != &dlls; le = le->next) {
            auto& de2 = containing_record<dll_entry>(le);

            if (!de2.new_entry)
                continue;

            status = resolve_imports_img(de2.ent->ldr.DllBase);
            if (!success(status)) {
                failure();
                return status;
            }

            status = toggle_protections_img(de2.ent->ldr.DllBase, true);
            if (!success(status)) {
                failure();
                return status;
            }
        }

        status = alloc_thread_tls_dynamic_mod(ent->ldr);
        if (!success(status)) {
            failure();
            return status;
        }

        status = dll_entry_attach(*de);
        if (!success(status)) {
            failure();
            return status;
        }

        while (!is_list_empty(dlls)) {
            auto& de2 = containing_record<dll_entry>(dlls.next);

            remove_entry_list(de2.le);

            delete &de2;
        }

        *dll_handle = ent->ldr.DllBase;
    }

    return nt_status::SUCCESS;
}
