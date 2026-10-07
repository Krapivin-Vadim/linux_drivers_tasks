#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <errno.h>

#define DEVICE_PATH "/dev/test_device"

/*
 * Для ioctl сейчас любая команда просто очищает буфер в драйвере.
 * Можно определить свой код команды, например:
 *   #define IOCTL_CLEAR_BUFFER _IO('T', 0)
 * и передавать его вместо 0.
 */
int main(void)
{
    int fd;
    const char *msg = "hello";
    char buf[16] = {0};
    ssize_t n;

    /* open */
    fd = open(DEVICE_PATH, O_RDWR);
    if (fd < 0) {
        perror("open");
        return EXIT_FAILURE;
    }
    printf("Device %s opened (fd=%d)\n", DEVICE_PATH, fd);

    /* write */
    n = write(fd, msg, strlen(msg));
    if (n < 0) {
        perror("write");
        close(fd);
        return EXIT_FAILURE;
    }
    printf("Wrote %zd bytes: \"%s\"\n", n, msg);

    /* read */
    memset(buf, 0, sizeof(buf));
    n = read(fd, buf, sizeof(buf) - 1);
    if (n < 0) {
        perror("read");
        close(fd);
        return EXIT_FAILURE;
    }
    printf("Read %zd bytes: \"%s\"\n", n, buf);

    /* ioctl (очистка буфера в драйвере) */
    if (ioctl(fd, 0, 0) < 0) {
        perror("ioctl");
        close(fd);
        return EXIT_FAILURE;
    }
    printf("ioctl() called (buffer cleared in driver)\n");

    /* ещё один read, чтобы увидеть, что буфер очищен */
    memset(buf, 0, sizeof(buf));
    n = read(fd, buf, sizeof(buf) - 1);
    if (n < 0) {
        perror("read after ioctl");
        close(fd);
        return EXIT_FAILURE;
    }
    printf("After ioctl read %zd bytes: \"%s\"\n", n, buf);

    /* close */
    if (close(fd) < 0) {
        perror("close");
        return EXIT_FAILURE;
    }
    printf("Device closed\n");

    return EXIT_SUCCESS;
}