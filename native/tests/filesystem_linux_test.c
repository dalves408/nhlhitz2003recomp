#include <assert.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include "../src/filesystem_linux.h"
int main(void)
{
    char root[]="/tmp/hitz-files-XXXXXX",path[512],wrong[512],raw[512];
    assert(mkdtemp(root));snprintf(path,sizeof(path),"%s/Heads",root);assert(!mkdir(path,0700));
    snprintf(path,sizeof(path),"%s/Heads/Face.XBX",root);int fd=open(path,O_CREAT|O_RDWR,0600);assert(fd>=0);
    assert(write(fd,"test",4)==4);close(fd);
    snprintf(wrong,sizeof(wrong),"%s/heads/FACE.xbx",root);fd=hitz_linux_open(wrong,O_RDONLY);assert(fd>=0);
    char buf[4];assert(read(fd,buf,4)==4&&!memcmp(buf,"test",4));close(fd);
    struct stat st;assert(!hitz_linux_stat(wrong,&st)&&st.st_size==4);
    snprintf(wrong,sizeof(wrong),"%s/heads/Subdir",root);assert(!hitz_linux_mkdir(wrong,0700));
    snprintf(path,sizeof(path),"%s/Heads/Subdir",root);assert(!rmdir(path));
    snprintf(wrong,sizeof(wrong),"%s/heads/FACE.xbx",root);
    hitz_linux_set_save_root(root);snprintf(raw,sizeof(raw),"%s/Partition3.img",root);
    fd=hitz_linux_open(raw,O_CREAT|O_RDWR,0600);assert(fd>=0);
    char zeros[4]={1,1,1,1};assert(pread(fd,zeros,4,100)==4&&!memcmp(zeros,"\0\0\0\0",4));
    assert(pwrite(fd,"data",4,100)==4);assert(lseek(fd,17,SEEK_SET)==17);
    int second=hitz_linux_open(raw,O_RDWR);assert(second>=0&&lseek(second,0,SEEK_CUR)==0);
    assert(pread(second,buf,4,100)==4&&!memcmp(buf,"data",4));close(second);close(fd);
    assert(!hitz_linux_stat(raw,&st)&&st.st_size==0x177000ull*512);
    fd=hitz_linux_open(raw,O_RDWR);assert(fd>=0&&pwrite(fd,"x",1,st.st_size)<0);close(fd);
    assert(stat(raw,&st)<0&&errno==ENOENT); /* no partition image was created */
    assert(!hitz_linux_unlink(wrong));snprintf(path,sizeof(path),"%s/Heads",root);assert(!rmdir(path));assert(!rmdir(root));
    puts("PASS: case-insensitive files, sparse memory-only partitions, independent cursors, no images");return 0;
}
