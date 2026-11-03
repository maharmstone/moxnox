// SPDX-License-Identifier: MIT
module;

#include <stdint.h>

export module pe;

export {

constexpr inline uint16_t IMAGE_DOS_SIGNATURE = 0x5a4d; // "MZ"
constexpr inline uint32_t IMAGE_NT_SIGNATURE = 0x00004550; // "PE\0\0"

constexpr inline uint16_t IMAGE_FILE_MACHINE_AMD64 = 0x8664;
constexpr inline uint16_t IMAGE_FILE_MACHINE_ARM64 = 0xaa64;

constexpr inline uint16_t IMAGE_NT_OPTIONAL_HDR32_MAGIC = 0x10b;
constexpr inline uint16_t IMAGE_NT_OPTIONAL_HDR64_MAGIC = 0x20b;

constexpr inline uint32_t IMAGE_DIRECTORY_ENTRY_EXPORT = 0;
constexpr inline uint32_t IMAGE_DIRECTORY_ENTRY_IMPORT = 1;
constexpr inline uint32_t IMAGE_DIRECTORY_ENTRY_BASERELOC = 5;
constexpr inline uint32_t IMAGE_DIRECTORY_ENTRY_TLS = 9;

constexpr inline uint16_t IMAGE_REL_BASED_ABSOLUTE = 0;
constexpr inline uint16_t IMAGE_REL_BASED_HIGHLOW = 3;
constexpr inline uint16_t IMAGE_REL_BASED_DIR64 = 10;

constexpr inline uint32_t IMAGE_SCN_TYPE_NO_PAD = 0x00000008;
constexpr inline uint32_t IMAGE_SCN_CNT_CODE = 0x00000020;
constexpr inline uint32_t IMAGE_SCN_CNT_INITIALIZED_DATA = 0x00000040;
constexpr inline uint32_t IMAGE_SCN_CNT_UNINITIALIZED_DATA = 0x00000080;
constexpr inline uint32_t IMAGE_SCN_LNK_OTHER = 0x00000100;
constexpr inline uint32_t IMAGE_SCN_LNK_INFO = 0x00000200;
constexpr inline uint32_t IMAGE_SCN_LNK_REMOVE = 0x00000800;
constexpr inline uint32_t IMAGE_SCN_LNK_COMDAT = 0x00001000;
constexpr inline uint32_t IMAGE_SCN_GPREL = 0x00008000;
constexpr inline uint32_t IMAGE_SCN_MEM_PURGEABLE = 0x00020000;
constexpr inline uint32_t IMAGE_SCN_MEM_16BIT = 0x00020000;
constexpr inline uint32_t IMAGE_SCN_MEM_LOCKED = 0x00040000;
constexpr inline uint32_t IMAGE_SCN_MEM_PRELOAD = 0x00080000;
constexpr inline uint32_t IMAGE_SCN_ALIGN_1BYTES = 0x00100000;
constexpr inline uint32_t IMAGE_SCN_ALIGN_2BYTES = 0x00200000;
constexpr inline uint32_t IMAGE_SCN_ALIGN_4BYTES = 0x00300000;
constexpr inline uint32_t IMAGE_SCN_ALIGN_8BYTES = 0x00400000;
constexpr inline uint32_t IMAGE_SCN_ALIGN_16BYTES = 0x00500000;
constexpr inline uint32_t IMAGE_SCN_ALIGN_32BYTES = 0x00600000;
constexpr inline uint32_t IMAGE_SCN_ALIGN_64BYTES = 0x00700000;
constexpr inline uint32_t IMAGE_SCN_ALIGN_128BYTES = 0x00800000;
constexpr inline uint32_t IMAGE_SCN_ALIGN_256BYTES = 0x00900000;
constexpr inline uint32_t IMAGE_SCN_ALIGN_512BYTES = 0x00a00000;
constexpr inline uint32_t IMAGE_SCN_ALIGN_1024BYTES = 0x00b00000;
constexpr inline uint32_t IMAGE_SCN_ALIGN_2048BYTES = 0x00c00000;
constexpr inline uint32_t IMAGE_SCN_ALIGN_4096BYTES = 0x00d00000;
constexpr inline uint32_t IMAGE_SCN_ALIGN_8192BYTES = 0x00e00000;
constexpr inline uint32_t IMAGE_SCN_LNK_NRELOC_OVFL = 0x01000000;
constexpr inline uint32_t IMAGE_SCN_MEM_DISCARDABLE = 0x02000000;
constexpr inline uint32_t IMAGE_SCN_MEM_NOT_CACHED = 0x04000000;
constexpr inline uint32_t IMAGE_SCN_MEM_NOT_PAGED = 0x08000000;
constexpr inline uint32_t IMAGE_SCN_MEM_SHARED = 0x10000000;
constexpr inline uint32_t IMAGE_SCN_MEM_EXECUTE = 0x20000000;
constexpr inline uint32_t IMAGE_SCN_MEM_READ = 0x40000000;
constexpr inline uint32_t IMAGE_SCN_MEM_WRITE = 0x80000000;

constexpr inline uint16_t IMAGE_FILE_DLL = 0x2000;

struct IMAGE_DOS_HEADER {
    uint16_t e_magic;
    uint16_t e_cblp;
    uint16_t e_cp;
    uint16_t e_crlc;
    uint16_t e_cparhdr;
    uint16_t e_minalloc;
    uint16_t e_maxalloc;
    uint16_t e_ss;
    uint16_t e_sp;
    uint16_t e_csum;
    uint16_t e_ip;
    uint16_t e_cs;
    uint16_t e_lfarlc;
    uint16_t e_ovno;
    uint16_t e_res[4];
    uint16_t e_oemid;
    uint16_t e_oeminfo;
    uint16_t e_res2[10];
    uint32_t e_lfanew;
};

struct IMAGE_FILE_HEADER {
    uint16_t Machine;
    uint16_t NumberOfSections;
    uint32_t TimeDateStamp;
    uint32_t PointerToSymbolTable;
    uint32_t NumberOfSymbols;
    uint16_t SizeOfOptionalHeader;
    uint16_t Characteristics;
};

struct IMAGE_DATA_DIRECTORY {
    uint32_t VirtualAddress;
    uint32_t Size;
};

struct IMAGE_OPTIONAL_HEADER32 {
    uint16_t Magic;
    uint8_t MajorLinkerVersion;
    uint8_t MinorLinkerVersion;
    uint32_t SizeOfCode;
    uint32_t SizeOfInitializedData;
    uint32_t SizeOfUninitializedData;
    uint32_t AddressOfEntryPoint;
    uint32_t BaseOfCode;
    uint32_t BaseOfData;
    uint32_t ImageBase;
    uint32_t SectionAlignment;
    uint32_t FileAlignment;
    uint16_t MajorOperatingSystemVersion;
    uint16_t MinorOperatingSystemVersion;
    uint16_t MajorImageVersion;
    uint16_t MinorImageVersion;
    uint16_t MajorSubsystemVersion;
    uint16_t MinorSubsystemVersion;
    uint32_t Win32VersionValue;
    uint32_t SizeOfImage;
    uint32_t SizeOfHeaders;
    uint32_t CheckSum;
    uint16_t Subsystem;
    uint16_t DllCharacteristics;
    uint32_t SizeOfStackReserve;
    uint32_t SizeOfStackCommit;
    uint32_t SizeOfHeapReserve;
    uint32_t SizeOfHeapCommit;
    uint32_t LoaderFlags;
    uint32_t NumberOfRvaAndSizes;
    IMAGE_DATA_DIRECTORY DataDirectory[0];
};

struct IMAGE_OPTIONAL_HEADER64 {
    uint16_t Magic;
    uint8_t MajorLinkerVersion;
    uint8_t MinorLinkerVersion;
    uint32_t SizeOfCode;
    uint32_t SizeOfInitializedData;
    uint32_t SizeOfUninitializedData;
    uint32_t AddressOfEntryPoint;
    uint32_t BaseOfCode;
    uint64_t ImageBase;
    uint32_t SectionAlignment;
    uint32_t FileAlignment;
    uint16_t MajorOperatingSystemVersion;
    uint16_t MinorOperatingSystemVersion;
    uint16_t MajorImageVersion;
    uint16_t MinorImageVersion;
    uint16_t MajorSubsystemVersion;
    uint16_t MinorSubsystemVersion;
    uint32_t Win32VersionValue;
    uint32_t SizeOfImage;
    uint32_t SizeOfHeaders;
    uint32_t CheckSum;
    uint16_t Subsystem;
    uint16_t DllCharacteristics;
    uint64_t SizeOfStackReserve;
    uint64_t SizeOfStackCommit;
    uint64_t SizeOfHeapReserve;
    uint64_t SizeOfHeapCommit;
    uint32_t LoaderFlags;
    uint32_t NumberOfRvaAndSizes;
    IMAGE_DATA_DIRECTORY DataDirectory[0];
};

struct IMAGE_NT_HEADERS {
    uint32_t Signature;
    IMAGE_FILE_HEADER FileHeader;
    union {
        IMAGE_OPTIONAL_HEADER32 OptionalHeader32;
        IMAGE_OPTIONAL_HEADER64 OptionalHeader64;
    };
};

struct IMAGE_SECTION_HEADER {
    char Name[8];
    uint32_t VirtualSize;
    uint32_t VirtualAddress;
    uint32_t SizeOfRawData;
    uint32_t PointerToRawData;
    uint32_t PointerToRelocations;
    uint32_t PointerToLinenumbers;
    uint16_t NumberOfRelocations;
    uint16_t NumberOfLinenumbers;
    uint32_t Characteristics;
} __attribute__((packed));

struct IMAGE_BASE_RELOCATION {
    uint32_t VirtualAddress;
    uint32_t SizeOfBlock;
};

struct IMAGE_EXPORT_DIRECTORY {
    uint32_t Characteristics;
    uint32_t TimeDateStamp;
    uint16_t MajorVersion;
    uint16_t MinorVersion;
    uint32_t Name;
    uint32_t Base;
    uint32_t NumberOfFunctions;
    uint32_t NumberOfNames;
    uint32_t AddressOfFunctions;
    uint32_t AddressOfNames;
    uint32_t AddressOfNameOrdinals;
} __attribute__((packed));

struct IMAGE_IMPORT_DESCRIPTOR {
    uint32_t Characteristics;
    uint32_t TimeDateStamp;
    uint32_t ForwarderChain;
    uint32_t Name;
    uint32_t FirstThunk;
} __attribute__((packed));

struct IMAGE_TLS_DIRECTORY64 {
    uint64_t StartAddressOfRawData;
    uint64_t EndAddressOfRawData;
    uint64_t AddressOfIndex;
    uint64_t AddressOfCallBacks;
    uint32_t SizeOfZeroFill;
    uint32_t Characteristics;
};

}
