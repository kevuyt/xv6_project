#include "types.h"
#include "user.h"
#include "fcntl.h"
#include "file.h"

#define BUF_SIZE 128

int main(int argc, char *argv[]) {
    int fd;
    char buffer[BUF_SIZE];
    int bytes_read;

    if (argc != 2) {
        printf(2, "Usage: %s <filename>\n", argv[0]);
        exit();
    }

    if ((fd = open(argv[1], O_RDWR)) < 0) {
        printf(2, "Error: cannot open file\n");
        exit();
    }

    write(fd, "Hello, world!\n", 14);

    // Adjust lseek calls
    lseek(fd, 0);  // Assuming lseek sets to the beginning by default

    bytes_read = read(fd, buffer, BUF_SIZE);
    if (bytes_read > 0) {
        printf(1, "Data read from file: %.*s\n", bytes_read, buffer);
    } else {
        printf(2, "Error reading file\n");
    }

    lseek(fd, 7);  // Assuming lseek moves to 7 bytes from the beginning

    bytes_read = read(fd, buffer, BUF_SIZE);
    if (bytes_read > 0) {
        printf(1, "Data read from current position: %.*s\n", bytes_read, buffer);
    } else {
        printf(2, "Error reading file\n");
    }

    close(fd);

    exit();
}
