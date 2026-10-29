module;

#include <stdlib.h>
#include <string.h>
#include <string_view>
#include <span>
#include <memory>
#include <bit>

export module tz;

import nt;
import ntdll;
import print;
import unique_handle;
import unicode;

using namespace std;

extern "C" {
    int _daylight;
    int _dstbias;
    long _timezone;
    char* _tzname[] = { (char*)"UTC", (char*)"UTC" };
}

export RTL_SRWLOCK tz_lock;
static char tz_string[256];

static constexpr uint32_t TZIF_MAGIC = 0x66695a54; // "TZif"

struct tzif_header {
    uint32_t magic;
    uint8_t ver;
    uint8_t reserved[15];
    uint32_t isutcnt;
    uint32_t isstdcnt;
    uint32_t leapcnt;
    uint32_t timecnt;
    uint32_t typecnt;
    uint32_t charcnt;
} __attribute__((packed));

struct tzif_time_type {
    int32_t utoff;
    uint8_t dst;
    uint8_t idx;
} __attribute__((packed));

struct tz_transition {
    int64_t time;
    uint8_t type;
};

static unique_ptr<tz_transition[]> tz_transitions_buf;
export span<tz_transition> tz_transitions;
static unique_ptr<tzif_time_type[]> tz_types_buf;
export span<tzif_time_type> tz_types;
export unique_ptr<char[]> tz_names;

static bool parse_tzif(span<const uint8_t> data, u16string_view fn) {
    static constexpr size_t TIME_SIZE1 = sizeof(int32_t);
    static constexpr size_t TIME_SIZE = sizeof(int64_t);

    size_t ver1_size;

    {
        auto& h1 = *(tzif_header*)data.data();

        if (h1.magic != TZIF_MAGIC) {
            print_stderr("TZ magic for {} was {:08x}, not {:08x} as expected\n",
                        fn, +h1.magic, TZIF_MAGIC);
            return false;
        }

        if (h1.ver != '2' && h1.ver != '3' && h1.ver != '4') {
            print_stderr("{} had unsupported version value {:02x}\n", fn, +h1.ver);
            return false;
        }

        auto isutcnt1 = byteswap(h1.isutcnt);
        auto isstdcnt1 = byteswap(h1.isstdcnt);
        auto leapcnt1 = byteswap(h1.leapcnt);
        auto timecnt1 = byteswap(h1.timecnt);
        auto typecnt1 = byteswap(h1.typecnt);
        auto charcnt1 = byteswap(h1.charcnt);

        ver1_size = sizeof(tzif_header) + (timecnt1 * TIME_SIZE1) + timecnt1 +
                    (typecnt1 * sizeof(tzif_time_type)) + charcnt1 +
                    (leapcnt1 * (TIME_SIZE1 + 4)) + isstdcnt1 + isutcnt1;

        if (data.size() < ver1_size + sizeof(tzif_header)) {
            print_stderr("{} was {} bytes, not at least {} as expected\n",
                        fn, data.size(), ver1_size + sizeof(tzif_header));
            return false;
        }
    }

    auto& h = *(tzif_header*)(data.data() + ver1_size);

    if (h.magic != TZIF_MAGIC) {
        print_stderr("TZ magic for {} was {:08x}, not {:08x} as expected\n",
                    fn, +h.magic, TZIF_MAGIC);
        return false;
    }

    if (h.ver != '2' && h.ver != '3' && h.ver != '4') {
        print_stderr("{} had unsupported version value {:02x}\n", fn, +h.ver);
        return false;
    }

    auto isutcnt = byteswap(h.isutcnt);
    auto isstdcnt = byteswap(h.isstdcnt);
    auto leapcnt = byteswap(h.leapcnt);
    auto timecnt = byteswap(h.timecnt);
    auto typecnt = byteswap(h.typecnt);
    auto charcnt = byteswap(h.charcnt);

    size_t ver2_size = sizeof(tzif_header) + (timecnt * TIME_SIZE) + timecnt +
                       (typecnt * sizeof(tzif_time_type)) + charcnt +
                       (leapcnt * (TIME_SIZE + 4)) + isstdcnt + isutcnt;

    if (data.size() < ver1_size + ver2_size) {
        print_stderr("{} was {} bytes, not at least {} as expected\n",
                    fn, data.size(), ver1_size + ver2_size);
        return false;
    }

    if (typecnt == 0) {
        print_stderr("{}: typecnt was 0\n", fn);
        return false;
    }

    if (charcnt == 0) {
        print_stderr("{}: charcnt was 0\n", fn);
        return false;
    }

    // read transitions times and type indexes

    unique_ptr<tz_transition[]> tztt;

    if (timecnt > 0) {
        tztt.reset(new tz_transition[timecnt]);
        if (!tztt) {
            print_stderr("new failed\n");
            return false;
        }

        auto times = (int64_t*)((uint8_t*)&h + sizeof(tzif_header));
        auto types = (uint8_t*)times + (sizeof(int64_t) * timecnt);

        for (size_t i = 0; i < timecnt; i++) {
            tztt[i].time = byteswap(times[i]);
            tztt[i].type = types[i];

            if (i != 0 && tztt[i].time <= tztt[i - 1].time) {
                print_stderr("{}: transition times are out of order\n", fn);
                return false;
            }

            if (types[i] >= typecnt) {
                print_stderr("{}: invalid transition type {}\n",
                             fn, types[i]);
                return false;
            }
        }
    }

    // read transition type records

    unique_ptr<tzif_time_type[]> tztt2;

    tztt2.reset(new tzif_time_type[typecnt]);
    if (!tztt2) {
        print_stderr("new failed\n");
        return false;
    }

    memcpy(tztt2.get(), (uint8_t*)&h + sizeof(tzif_header) + ((sizeof(int64_t) + 1) * timecnt),
           sizeof(tzif_time_type) * typecnt);

    for (size_t i = 0; i < typecnt; i++) {
        tztt2[i].utoff = byteswap(tztt2[i].utoff);

        if (tztt2[i].idx >= charcnt) {
            print_stderr("{}: type {} had invalid idx {}\n", fn, i, tztt2[i].idx);
            return false;
        }
    }

    // read time zone designations

    unique_ptr<char[]> names;

    names.reset(new char[charcnt]);
    if (!names) {
        print_stderr("new failed\n");
        return false;
    }

    memcpy(names.get(),
           (uint8_t*)&h + sizeof(tzif_header) + ((sizeof(int64_t) + 1) * timecnt) + (sizeof(tzif_time_type) * typecnt),
           charcnt);

    if (names[charcnt - 1] != 0) {
        print_stderr("{}: designation strings did not end with 0\n", fn);
        return false;
    }

    // replace existing values

    swap(tz_transitions_buf, tztt);
    tz_transitions = span(tz_transitions_buf.get(), timecnt);

    swap(tz_types_buf, tztt2);
    tz_types = span(tz_types_buf.get(), typecnt);

    swap(tz_names, names);

    // FIXME - derive _timezone and _tzname from footer
    // FIXME - set _daylight
    // FIXME - copy over _daylight, _timezone, and _tzname to mnposix counterparts

    return true;
}

static bool load_tz_file(string_view fn) {
    OBJECT_ATTRIBUTES oa;
    UNICODE_STRING us;
    nt_status status;
    HANDLE h;
    unique_handle file, sect;
    IO_STATUS_BLOCK iosb;
    FILE_STANDARD_INFORMATION fsi;
    unique_ptr<char16_t[]> buf;
    void* addr;
    size_t size;

    static const u16string_view path = u"/share/zoneinfo/";

    if (fn[0] == ':') {
        fn = string_view(fn.data() + 1, fn.size() - 1);
        if (fn.empty())
            return false;
    }

    size_t prefix_size = fn[0] == '/' ? 0 : path.size();
    auto u16len = utf8_to_utf16_len(fn);

    buf.reset(new char16_t[prefix_size + u16len]);
    if (!buf) {
        print_stderr("malloc failed\n");
        return false;
    }

    if (fn[0] != '/')
        memcpy(buf.get(), path.data(), path.size() * sizeof(char16_t));

    utf8_to_utf16(span(buf.get() + prefix_size, u16len), fn);

    auto tzfn = u16string_view(buf.get(), prefix_size + u16len);

    us.Length = us.MaximumLength = (uint16_t)(tzfn.size() * sizeof(char16_t));
    us.Buffer = (char16_t*)tzfn.data();

    memset(&oa, 0, sizeof(oa));
    oa.Length = sizeof(OBJECT_ATTRIBUTES);
    oa.ObjectName = &us;

    status = NtCreateFile(&h, FILE_READ_DATA, &oa, &iosb, nullptr, 0, 0,
                          FILE_OPEN, FILE_NON_DIRECTORY_FILE, nullptr, 0);
    if (!success(status)) {
        print_stderr("NtCreateFile returned {} for {}\n", status, tzfn);
        return false;
    }

    file.reset(h);

    status = NtQueryInformationFile(file.get(), &iosb, &fsi, sizeof(fsi),
                                    FILE_INFORMATION_CLASS::StandardInformation);
    if (!success(status)) {
        print_stderr("NtQueryInformationFile returned {} for {}\n", status, tzfn);
        return false;
    }

    if (fsi.EndOfFile.QuadPart < (int64_t)sizeof(tzif_header)) {
        print_stderr("size of {} was {}, expected at least {}\n",
                     tzfn, fsi.EndOfFile.QuadPart, sizeof(tzif_header));
        return false;
    }

    status = NtCreateSection(&h, SECTION_MAP_READ, nullptr, nullptr,
                             PAGE_READONLY, SEC_COMMIT, file.get());
    if (!success(status)) {
        print_stderr("NtCreateSection returned {} for {}\n", status, tzfn);
        return false;
    }

    sect.reset(h);

    addr = nullptr;
    size = 0;

    status = NtMapViewOfSection(sect.get(), NtCurrentProcess(), &addr, 0, 0,
                                nullptr, &size, SECTION_INHERIT::ViewUnmap, 0,
                                PAGE_READONLY);
    if (!success(status)) {
        print_stderr("NtMapViewOfSection returned {} for {}\n", status, tzfn);
        return false;
    }

    auto ret = parse_tzif(span((uint8_t*)addr, (size_t)fsi.EndOfFile.QuadPart),
                          tzfn);

    status = NtUnmapViewOfSection(NtCurrentProcess(), addr);
    if (!success(status))
        print_stderr("NtUnmapViewOfSection returned {} for {}\n", status, tzfn);

    return ret;
}

export extern "C"
void _tzset() {
    RtlAcquireSRWLockExclusive(&tz_lock);

    auto tz = getenv("TZ");
    auto len = tz ? strlen(tz) : 0;

    if (!tz || !strcmp(tz, "") || len >= sizeof(tz_string)) {
        if (strcmp(tz_string, "")) {
            tz_string[0] = 0;

            _daylight = 0;
            _dstbias = 0;
            _timezone = 0;
            _tzname[0] = (char*)"UTC";
            _tzname[1] = (char*)"UTC";

            tz_transitions_buf.reset();
            tz_types_buf.reset();
            tz_names.reset();
            tz_transitions = span<tz_transition>();
            tz_types = span<tzif_time_type>();
        }

        RtlReleaseSRWLockExclusive(&tz_lock);

        return;
    }

    if (!strcmp(tz_string, tz)) {
        RtlReleaseSRWLockExclusive(&tz_lock);
        return;
    }

    // FIXME - TZ might be rules string rather than filename

    if (!load_tz_file(tz)) {
        tz_string[0] = 0;

        _daylight = 0;
        _dstbias = 0;
        _timezone = 0;
        _tzname[0] = (char*)"UTC";
        _tzname[1] = (char*)"UTC";

        tz_transitions_buf.reset();
        tz_types_buf.reset();
        tz_names.reset();
        tz_transitions = span<tz_transition>();
        tz_types = span<tzif_time_type>();

        RtlReleaseSRWLockExclusive(&tz_lock);
        return;
    }

    strcpy(tz_string, tz);

    RtlReleaseSRWLockExclusive(&tz_lock);
}

extern "C"
long* ____timezone() {
    return &_timezone;
}
