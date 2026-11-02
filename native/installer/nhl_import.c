/* nhl_import: copies the game's data files out of the player's own copy of the disc.
 *
 *   nhl_import <source> <dest_dir>
 *   nhl_import --check <source>    (validate without writing)
 *   nhl_import --verify <folder>   (validate an existing extraction)
 *
 * <source> is an Xbox disc image (.iso: a full Redump image or a plain XISO) or a
 * folder holding the game's files (the disc's root, with default.xbe). Only the files
 * listed in manifest.h are written to <dest_dir>, after default.xbe matches the
 * version this build was made from. Progress goes to stdout as "PROGRESS done total".
 *
 * Exit codes: 0 ok, 2 source not found, 3 not an Xbox disc image, 4 wrong game or
 * version (default.xbe differs), 5 files missing or the wrong size, 6 cannot write. */
#include <windows.h>
#include <bcrypt.h>
#include <shlobj.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "manifest.h"

#define SECTOR 2048ull
static const unsigned long long XDVDFS_OFFSETS[] = { 0, 0x18300000ull, 0x0FD90000ull, 0x02080000ull };

static unsigned long long g_done;
static unsigned g_found;
static int g_found_flag[MANIFEST_COUNT];
static WCHAR g_dest[MAX_PATH];
static HANDLE g_img = INVALID_HANDLE_VALUE;
static unsigned long long g_base;
static int g_err;
static int g_check_only, g_xbe_verified;
static unsigned long long g_image_size;
static int g_hash_only;      /* 1 while extracting only default.xbe to hash it */

static int manifest_find(const char *rel)
{
    int i;
    for (i = 0; i < MANIFEST_COUNT; i++)
        if (_stricmp(MANIFEST[i].path, rel) == 0)
            return i;
    return -1;
}

static void progress(void)
{
    printf("PROGRESS %llu %llu\n", g_done, (unsigned long long)MANIFEST_BYTES);
    fflush(stdout);
}

static int make_dirs_for(const WCHAR *file)
{
    WCHAR dir[MAX_PATH], *sl;
    wcscpy_s(dir, MAX_PATH, file);
    sl = wcsrchr(dir, L'\\');
    if (!sl) return 1;
    *sl = 0;
    return SHCreateDirectoryExW(NULL, dir, NULL) == ERROR_SUCCESS ||
           GetFileAttributesW(dir) != INVALID_FILE_ATTRIBUTES;
}

static int dest_path(const char *rel, WCHAR *out)
{
    WCHAR w[MAX_PATH];
    WCHAR *p;
    if(!MultiByteToWideChar(CP_ACP,0,rel,-1,w,MAX_PATH))return 0;
    for (p = w; *p; p++) if (*p == L'/') *p = L'\\';
    if(wcslen(g_dest)+wcslen(w)+2>MAX_PATH)return 0;
    return swprintf_s(out,MAX_PATH,L"%s\\%s",g_dest,w)>0;
}

static int sha256_region(HANDLE h,unsigned long long offset,unsigned long long length,unsigned char out[32])
{
    BCRYPT_ALG_HANDLE alg=NULL;BCRYPT_HASH_HANDLE hh=NULL;
    static unsigned char buf[1<<20];LARGE_INTEGER pos;DWORD n;int ok=0;
    pos.QuadPart=(LONGLONG)offset;
    if(!SetFilePointerEx(h,pos,NULL,FILE_BEGIN) || BCryptOpenAlgorithmProvider(&alg,BCRYPT_SHA256_ALGORITHM,NULL,0))return 0;
    if(!BCryptCreateHash(alg,&hh,NULL,0,NULL,0,0)){
        while(length){DWORD want=length>sizeof buf?sizeof buf:(DWORD)length;
            if(!ReadFile(h,buf,want,&n,NULL)||n!=want||BCryptHashData(hh,buf,n,0))goto done;
            length-=n;
        }
        ok=!BCryptFinishHash(hh,out,32,0);
    }
done:
    if(hh)BCryptDestroyHash(hh);BCryptCloseAlgorithmProvider(alg,0);return ok;
}
static int sha256_file(HANDLE h,unsigned char out[32])
{LARGE_INTEGER size;return GetFileSizeEx(h,&size)&&size.QuadPart>=0&&sha256_region(h,0,(unsigned long long)size.QuadPart,out);}

static int xbe_ok(const WCHAR *path)
{
    unsigned char h[32];
    HANDLE f = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    int ok;
    if (f == INVALID_HANDLE_VALUE) return 0;
    ok = sha256_file(f, h) && memcmp(h, XBE_SHA256, 32) == 0;
    CloseHandle(f);
    return ok;
}

/* copy len bytes from `in` (already positioned) to a new file at dest */
static int write_out(const char *rel, HANDLE in, unsigned long long len, int count_progress)
{
    static unsigned char buf[1 << 20];
    WCHAR dst[MAX_PATH];
    HANDLE out;
    unsigned long long left = len;
    DWORD n, w;
    if(!dest_path(rel,dst))return 0;
    if (!make_dirs_for(dst)) return 0;
    out = CreateFileW(dst, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (out == INVALID_HANDLE_VALUE) return 0;
    while (left) {
        DWORD want = left > sizeof buf ? (DWORD)sizeof buf : (DWORD)left;
        if (!ReadFile(in, buf, want, &n, NULL) || n == 0) break;
        if (!WriteFile(out, buf, n, &w, NULL) || w != n) { CloseHandle(out); return 0; }
        left -= n;
        if (count_progress) { g_done += n; progress(); }
    }
    CloseHandle(out);
    return left == 0;
}

/* ---- disc image (XDVDFS) -------------------------------------------------- */
static int img_read(unsigned long long off, void *buf, DWORD n)
{
    LARGE_INTEGER p;
    DWORD got;
    if(off>g_image_size || n>g_image_size-off)return 0;
    p.QuadPart = (LONGLONG)off;
    return SetFilePointerEx(g_img, p, NULL, FILE_BEGIN) && ReadFile(g_img, buf, n, &got, NULL) && got == n;
}

static void walk(unsigned long long dir_sector, unsigned dir_size, unsigned off, const char *prefix,unsigned depth)
{
    unsigned char *d;
    unsigned char *e;
    unsigned left, right, sector, size, attr, nl;
    char name[256], rel[600];
    unsigned visits=0;
    if(g_err || dir_size==0)return;
    if(depth>32 || dir_size>(16u<<20)){g_err=3;return;}
    d = (unsigned char *)malloc(dir_size);
    if (!d || !img_read(g_base + dir_sector * SECTOR, d, dir_size)) { free(d); g_err = 3; return; }
    {
        /* iterative-free tree walk: the directory is a binary tree of entries */
        struct { unsigned pos; int state; } st[512];
        int sp = 0;
        st[0].pos = off; st[0].state = 0; sp = 1;
        while (sp && !g_err) {
            if(++visits>dir_size+1){g_err=3;break;}
            unsigned pos = st[sp - 1].pos;
            int s = st[sp - 1].state;
            if(pos>dir_size || dir_size-pos<14){g_err=3;break;}
            e = d + pos;
            left = e[0] | (e[1] << 8);
            right = e[2] | (e[3] << 8);
            if (left == 0xFFFF) { sp--; continue; }
            if (s == 0) {
                st[sp - 1].state = 1;
                if(left && sp>=512){g_err=3;break;}
                if (left && sp < 512) { st[sp].pos = left * 4; st[sp].state = 0; sp++; }
                continue;
            }
            if (s == 1) {
                st[sp - 1].state = 2;
                sector = e[4] | (e[5] << 8) | (e[6] << 16) | ((unsigned)e[7] << 24);
                size = e[8] | (e[9] << 8) | (e[10] << 16) | ((unsigned)e[11] << 24);
                attr = e[12];
                nl = e[13];
                if(nl==0 || nl>dir_size-pos-14){g_err=3;break;}
                memcpy(name, e + 14, nl);
                name[nl] = 0;
                if(strchr(name,'/')||strchr(name,'\\')||strchr(name,':')||!strcmp(name,"..")||!strcmp(name,".")){g_err=3;break;}
                if(snprintf(rel,sizeof rel,"%s%s",prefix,name)>=(int)sizeof rel){g_err=3;break;}
                if (attr & 0x10) {
                    char sub[600];
                    if(snprintf(sub,sizeof sub,"%s/",rel)>=(int)sizeof sub){g_err=3;break;}
                    walk(sector,size,0,sub,depth+1);
                } else {
                    int mi = manifest_find(rel);
                    if (mi >= 0 && (!g_hash_only || _stricmp(rel, "default.xbe") == 0)) {
                        unsigned long long offset=g_base+(unsigned long long)sector*SECTOR;
                        LARGE_INTEGER pos;unsigned char hash[32];pos.QuadPart=(LONGLONG)offset;
                        if(size!=MANIFEST[mi].size){g_err=5;}
                        else if(offset>g_image_size || size>g_image_size-offset){g_err=3;}
                        else if(g_hash_only){
                            g_xbe_verified=sha256_region(g_img,offset,size,hash)&&!memcmp(hash,XBE_SHA256,32);
                            g_found_flag[mi]=1;
                        }else if(g_found_flag[mi]){g_err=3;}
                        else if(!g_check_only && (!SetFilePointerEx(g_img,pos,NULL,FILE_BEGIN)||!write_out(rel,g_img,size,1))){g_err=6;}
                        else{g_found_flag[mi]=1;g_found++;}
                    }
                }
                continue;
            }
            /* s == 2: right subtree */
            sp--;
            if(right && sp>=512){g_err=3;break;}
            if (right && sp < 512) { st[sp].pos = right * 4; st[sp].state = 0; sp++; }
        }
    }
    free(d);
}

static int find_xdvdfs(unsigned long long *root_sector, unsigned *root_size)
{
    unsigned char v[24];
    size_t i;
    for (i = 0; i < sizeof XDVDFS_OFFSETS / sizeof *XDVDFS_OFFSETS; i++) {
        if (img_read(XDVDFS_OFFSETS[i] + 32 * SECTOR, v, 24) && memcmp(v, "MICROSOFT*XBOX*MEDIA", 20) == 0) {
            g_base = XDVDFS_OFFSETS[i];
            *root_sector = v[20] | (v[21] << 8) | (v[22] << 16) | ((unsigned)v[23] << 24);
            if (!img_read(g_base + 32 * SECTOR + 24, root_size, 4)) return 0;
            return 1;
        }
    }
    return 0;
}

/* ---- folder --------------------------------------------------------------- */
static int copy_folder(const WCHAR *root)
{
    int i;
    for (i = 0; i < MANIFEST_COUNT; i++) {
        WCHAR src[MAX_PATH], rel[MAX_PATH], *p;
        HANDLE in;
        LARGE_INTEGER sz;
        MultiByteToWideChar(CP_ACP, 0, MANIFEST[i].path, -1, rel, MAX_PATH);
        for (p = rel; *p; p++) if (*p == L'/') *p = L'\\';
        swprintf_s(src, MAX_PATH, L"%s\\%s", root, rel);
        in = CreateFileW(src, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
        if (in == INVALID_HANDLE_VALUE) continue;
        if (GetFileSizeEx(in, &sz) && (unsigned long long)sz.QuadPart == MANIFEST[i].size) {
            if (!g_check_only && !write_out(MANIFEST[i].path, in, MANIFEST[i].size, 1)) { CloseHandle(in); return 6; }
            g_found_flag[i] = 1; g_found++;
        }
        CloseHandle(in);
    }
    return 0;
}

/* the folder that holds default.xbe: the folder itself or one of its children */
static int find_root(const WCHAR *dir, WCHAR *root)
{
    WCHAR pat[MAX_PATH], t[MAX_PATH];
    WIN32_FIND_DATAW fd;
    HANDLE h;
    swprintf_s(t, MAX_PATH, L"%s\\default.xbe", dir);
    if (GetFileAttributesW(t) != INVALID_FILE_ATTRIBUTES) { wcscpy_s(root, MAX_PATH, dir); return 1; }
    swprintf_s(pat, MAX_PATH, L"%s\\*", dir);
    h = FindFirstFileW(pat, &fd);
    if (h == INVALID_HANDLE_VALUE) return 0;
    do {
        if ((fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) && fd.cFileName[0] != L'.') {
            swprintf_s(t, MAX_PATH, L"%s\\%s\\default.xbe", dir, fd.cFileName);
            if (GetFileAttributesW(t) != INVALID_FILE_ATTRIBUTES) {
                swprintf_s(root, MAX_PATH, L"%s\\%s", dir, fd.cFileName);
                FindClose(h);
                return 1;
            }
        }
    } while (FindNextFileW(h, &fd));
    FindClose(h);
    return 0;
}

static int files_complete(void)
{
    unsigned i,missing=0;
    for(i=0;i<MANIFEST_COUNT;i++)if(!g_found_flag[i]){if(missing++<5)printf("MISSING %s\n",MANIFEST[i].path);}
    return missing==0;
}
static int overlaps(const WCHAR *a,const WCHAR *b)
{
    size_t n=wcslen(a),m=wcslen(b);
    return (!_wcsnicmp(a,b,n)&&(b[n]==0||b[n]==L'\\')) || (!_wcsnicmp(a,b,m)&&(a[m]==0||a[m]==L'\\'));
}
/* Resolve existing parents as well as junctions, so an alias of the source
 * cannot accidentally be selected as its own output directory. */
static int canonical_directory(const WCHAR *input,WCHAR out[MAX_PATH])
{
    WCHAR full[MAX_PATH],parent[MAX_PATH],*slash;DWORD count;size_t cut;HANDLE h;
    count=GetFullPathNameW(input,MAX_PATH,full,NULL);if(!count||count>=MAX_PATH)return 0;
    wcscpy_s(parent,MAX_PATH,full);
    while(GetFileAttributesW(parent)==INVALID_FILE_ATTRIBUTES){
        slash=wcsrchr(parent,L'\\');if(!slash||slash<=parent+2)return 0;*slash=0;
    }
    cut=wcslen(parent);
    h=CreateFileW(parent,0,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,NULL,OPEN_EXISTING,FILE_FLAG_BACKUP_SEMANTICS,NULL);
    if(h==INVALID_HANDLE_VALUE)return 0;
    count=GetFinalPathNameByHandleW(h,out,MAX_PATH,FILE_NAME_NORMALIZED|VOLUME_NAME_DOS);CloseHandle(h);
    if(!count||count>=MAX_PATH||count+wcslen(full+cut)>=MAX_PATH)return 0;
    wcscat_s(out,MAX_PATH,full+cut);
    while(wcslen(out)>7&&out[wcslen(out)-1]==L'\\')out[wcslen(out)-1]=0;
    return 1;
}
int wmain(int argc,wchar_t **argv)
{
    int check=argc==3&&(!wcscmp(argv[1],L"--check")||!wcscmp(argv[1],L"--verify"));
    const WCHAR *source;DWORD attr;WCHAR root[MAX_PATH],xbe[MAX_PATH],full_source[MAX_PATH];
    unsigned long long rs=0;unsigned rsz=0;int folder,r;FILE *marker;
    if(argc!=3){puts("usage: nhl_import <iso or folder> <destination> | --check <source> | --verify <folder>");return 2;}
    source=argv[check?2:1];attr=GetFileAttributesW(source);
    if(attr==INVALID_FILE_ATTRIBUTES){puts("ERROR The source was not found.");return 2;}
    folder=(attr&FILE_ATTRIBUTE_DIRECTORY)!=0;
    if(!check){
        DWORD n=GetFullPathNameW(argv[2],MAX_PATH,g_dest,NULL);if(!n||n>=MAX_PATH)return 6;
        while(wcslen(g_dest)>3 && (g_dest[wcslen(g_dest)-1]==L'\\'||g_dest[wcslen(g_dest)-1]==L'/'))g_dest[wcslen(g_dest)-1]=0;
    }
    if(folder){
        if(!find_root(source,root)){puts("ERROR default.xbe was not found.");return 4;}
        swprintf_s(xbe,MAX_PATH,L"%s\\default.xbe",root);
        if(!xbe_ok(xbe)){puts("ERROR This is not the supported NHL Hitz 2003 Xbox USA executable.");return 4;}
        if(!check){WCHAR target[MAX_PATH];
            if(!canonical_directory(root,full_source)||!canonical_directory(g_dest,target)||overlaps(full_source,target)){
                puts("ERROR Source and destination folders must be separate.");return 6;}}
        g_check_only=1;r=copy_folder(root);if(r||!files_complete()){puts("ERROR Source files are incomplete or have incorrect sizes.");return 5;}
    }else{
        LARGE_INTEGER size;
        g_img=CreateFileW(source,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,FILE_FLAG_RANDOM_ACCESS,NULL);
        if(g_img==INVALID_HANDLE_VALUE || !GetFileSizeEx(g_img,&size))return 2;
        g_image_size=(unsigned long long)size.QuadPart;
        if(!find_xdvdfs(&rs,&rsz)){puts("ERROR This is not an Xbox disc image.");return 3;}
        g_hash_only=1;walk(rs,rsz,0,"",0);
        if(g_err || !g_xbe_verified){puts("ERROR This is not the supported NHL Hitz 2003 Xbox USA disc image.");return 4;}
        g_hash_only=0;g_check_only=1;g_found=0;memset(g_found_flag,0,sizeof g_found_flag);walk(rs,rsz,0,"",0);
        if(g_err||!files_complete()){puts("ERROR The source image is damaged or incomplete.");return 5;}
    }
    if(check){if(g_img!=INVALID_HANDLE_VALUE)CloseHandle(g_img);printf("OK verified %u files\n",g_found);return 0;}
    {WCHAR path[MAX_PATH];swprintf_s(path,MAX_PATH,L"%s\\.hitz-import-complete",g_dest);DeleteFileW(path);}
    g_check_only=0;g_found=0;g_done=0;memset(g_found_flag,0,sizeof g_found_flag);
    if(folder)r=copy_folder(root);else{walk(rs,rsz,0,"",0);r=g_err;CloseHandle(g_img);}
    if(r||!files_complete()){puts("ERROR Game files could not be copied completely.");return 6;}
    swprintf_s(xbe,MAX_PATH,L"%s\\.hitz-import-complete",g_dest);
    marker=_wfopen(xbe,L"wb");if(!marker)return 6;
    r=fprintf(marker,"%s\n",IMPORT_SIGNATURE);if(fclose(marker)||r<0)return 6;
    printf("OK %u files\n",g_found);return 0;
}
