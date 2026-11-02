#include "kernel.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef void (*recomp_func_t)(void);
recomp_func_t recomp_lookup(uint32_t va) { (void)va; return NULL; }
recomp_func_t recomp_lookup_manual(uint32_t va) { (void)va; return NULL; }
/* Unused renderer/guest diagnostics required by the shared runtime library. */
void hitz_nis_read_trace(uint32_t buffer, uint32_t bytes, int64_t offset)
{ (void)buffer; (void)bytes; (void)offset; }
int nv2a_gpuvs_probe(const void *state) { (void)state; return 0; }
void nv2a_gpu_flush_pending(void) {}
extern BOOL hitz_disk_size(HANDLE h, PLARGE_INTEGER size);
#define CHECK(x) do { if (!(x)) { fprintf(stderr,"line %d: %s\n",__LINE__,#x); exit(1); } } while(0)

static HANDLE open_path(const char *path, ULONG disposition, ULONG options)
{
    XBOX_ANSI_STRING name = {(USHORT)strlen(path), (USHORT)(strlen(path)+1), (PCHAR)path};
    XBOX_OBJECT_ATTRIBUTES oa = {0};
    XBOX_IO_STATUS_BLOCK ios;
    HANDLE h = NULL;
    oa.ObjectName = &name;
    CHECK(xbox_NtCreateFile(&h, 0xC0110000u, &oa, &ios, NULL, 0x80,
                           3, disposition, options) == STATUS_SUCCESS);
    return h;
}

int main(int argc, char **argv)
{
    static const uint64_t sectors[] = {0x9C4800, 0x465400, 0xFA000, 0x177000, 0x177000, 0x177000};
    unsigned char buffer[1024], written[1024];
    XBOX_IO_STATUS_BLOCK ios;
    LARGE_INTEGER offset, size;
    XBOX_FILE_STANDARD_INFORMATION standard;
    HANDLE h, dup, other;
    WCHAR glob[MAX_PATH];
    WIN32_FIND_DATAW found;
    char device[80];
    CHECK(argc == 2);
    SetEnvironmentVariableA("RECOMP_SAVE_DIR", NULL);
    _putenv_s("RECOMP_SAVE_DIR", "");
    xbox_path_init(argv[1], argv[1]);
    for (int p = 0; p <= 5; ++p) {
        snprintf(device, sizeof device, "\\Device\\Harddisk0\\Partition%d", p);
        h = open_path(device, XBOX_FILE_OPEN, 0);
        CHECK(hitz_disk_size(h, &size) && size.QuadPart == sectors[p] * 512);
        CHECK(xbox_NtQueryInformationFile(h, &ios, &standard, sizeof standard,
                                          XboxFileStandardInformation) == 0);
        CHECK(standard.EndOfFile.QuadPart == size.QuadPart);
        offset.QuadPart = p == 0 ? 0x800 : 0;
        CHECK(xbox_NtReadFile(h, NULL, NULL, NULL, &ios, buffer, 512, &offset) == 0);
        if (p == 0) CHECK(!memcmp(buffer, "****PARTINFO****", 16));
        else { for (int i=0;i<512;i++) CHECK(buffer[i] == 0); }
        offset.QuadPart = 4090;
        CHECK(xbox_NtReadFile(h, NULL, NULL, NULL, &ios, buffer, sizeof buffer, &offset) == 0);
        for (int i=0;i<sizeof buffer;i++) CHECK(buffer[i] == 0);
        /* Cross a RAM block boundary and check independent opens share data. */
        memset(written, p+1, sizeof written); offset.QuadPart = 4090;
        CHECK(xbox_NtWriteFile(h, NULL, NULL, NULL, &ios, written, sizeof written, &offset) == 0);
        CHECK(DuplicateHandle(GetCurrentProcess(), h, GetCurrentProcess(), &dup,
                               0, FALSE, DUPLICATE_SAME_ACCESS));
        CHECK(xbox_NtClose(h) == 0);
        CHECK(xbox_NtReadFile(dup, NULL, NULL, NULL, &ios, buffer, sizeof buffer, &offset) == 0);
        CHECK(!memcmp(buffer, written, sizeof buffer));
        other = open_path(device, XBOX_FILE_OPEN, 0);
        CHECK(xbox_NtReadFile(other, NULL, NULL, NULL, &ios, buffer, sizeof buffer, &offset) == 0);
        CHECK(!memcmp(buffer, written, sizeof buffer));
        offset.QuadPart = size.QuadPart - 16;
        CHECK(xbox_NtReadFile(other, NULL, NULL, NULL, &ios, buffer, 512, &offset) == 0 && ios.Information == 16);
        offset.QuadPart = size.QuadPart;
        CHECK(xbox_NtReadFile(other, NULL, NULL, NULL, &ios, buffer, 512, &offset) == STATUS_END_OF_FILE);
        CHECK(xbox_NtWriteFile(other, NULL, NULL, NULL, &ios, written, 512, &offset) != 0);
        CHECK(xbox_NtFlushBuffersFile(dup, &ios) == 0);
        CHECK(xbox_NtClose(dup) == 0 && xbox_NtClose(other) == 0);
        h = open_path(device, XBOX_FILE_OPEN, XBOX_FILE_DIRECTORY_FILE);
        XBOX_FILE_FS_SIZE_INFORMATION volume;
        CHECK(xbox_NtQueryVolumeInformationFile(h, &ios, &volume, sizeof volume,
                                                XboxFileFsSizeInformation) == 0);
        CHECK(volume.BytesPerSector == 512 && volume.SectorsPerAllocationUnit == 32);
        if (p == 5) {
            XBOX_ANSI_STRING name = {12, 13, "relative.sav"};
            XBOX_OBJECT_ATTRIBUTES oa = {0};
            oa.RootDirectory = h; oa.ObjectName = &name;
            CHECK(xbox_NtCreateFile(&other, 0xC0110000u, &oa, &ios, NULL,
                0x80, 3, XBOX_FILE_OVERWRITE_IF, 0) == 0);
            offset.QuadPart = 0;
            CHECK(xbox_NtWriteFile(other, NULL, NULL, NULL, &ios, written,
                                   sizeof written, &offset) == 0);
            CHECK(xbox_NtClose(other) == 0);
            other = open_path("\\Device\\Harddisk0\\Partition5\\relative.sav", XBOX_FILE_OPEN, 0);
            CHECK(xbox_NtReadFile(other, NULL, NULL, NULL, &ios, buffer,
                                  sizeof buffer, &offset) == 0);
            CHECK(!memcmp(buffer, written, sizeof buffer));
            CHECK(xbox_NtClose(other) == 0);
        }
        CHECK(xbox_NtClose(h) == 0);
    }
    /* Real save files continue to use the host filesystem. */
    h = open_path("T:\\virtual_disk_test.sav", XBOX_FILE_OVERWRITE_IF, 0);
    offset.QuadPart = 0;
    CHECK(xbox_NtWriteFile(h, NULL, NULL, NULL, &ios, written, sizeof written, &offset) == 0);
    CHECK(xbox_NtClose(h) == 0);
    h = open_path("T:\\virtual_disk_test.sav", XBOX_FILE_OPEN, 0);
    CHECK(xbox_NtReadFile(h, NULL, NULL, NULL, &ios, buffer, sizeof buffer, &offset) == 0);
    CHECK(!memcmp(buffer, written, sizeof buffer));
    CHECK(xbox_NtClose(h) == 0);
    CHECK(MultiByteToWideChar(CP_UTF8, 0, argv[1], -1, glob, MAX_PATH));
    wcscat_s(glob, MAX_PATH, L"\\Partition*.img");
    h = FindFirstFileW(glob, &found);
    CHECK(h == INVALID_HANDLE_VALUE);
    puts("PASS: RAM partitions, geometry, duplicated handles, bounds, real saves, no images");
    return 0;
}
