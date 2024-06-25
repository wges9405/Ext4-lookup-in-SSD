#include <fcntl.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <linux/nvme_ioctl.h>

void prepare_search_data(char *buf) {
    // prepare data for search
    memset(buf, '\0', sizeof(buf)); // clean buffer
    memcpy(buf, "./linux-5.4/fs/ext4/namei.c", sizeof("dir1/dir2")); // wirte in filepath
    printf("User: prepared buffer:\r\n\tfilepath: %s\r\n", buf);
}

int main (int argc, char** argv) {
    int fd = open("/dev/nvme0n1", O_RDWR);
    if (fd == -1) {
        perror("open");
        exit(1);
    }

    int ret = 0;
    char *buf = malloc(4096 * sizeof(char));
    memset(buf, '\0', sizeof(buf));
    prepare_search_data(buf);

    struct nvme_user_io nui = {0};
    nui.addr = (__u64)(uintptr_t) buf;
    nui.control |= (1 << 9); // cdw12[25] be used as a flag for doing lookup in ssd
    nui.slba = 1;            // be used as a flag for doing lookup in ssd (double check)
    nui.nblocks = 1;
    nui.opcode = 0x03;       // new nvme normal io-cmd if set, if not set, use 0x01 (write)
    ret = ioctl(fd, NVME_IOCTL_SUBMIT_IO, &nui);    // send ioctl cmd

    uint32_t ino = 0;
    ino = atoi(buf);
    if (ino == 0) {
        printf("User: search failed\r\n");
    } else {
        printf("User: received destination inode #: %d\r\n", ino);
    }
    return 0;
}
