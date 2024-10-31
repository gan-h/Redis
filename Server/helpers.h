#include <unistd.h>
#include <assert.h>

int read_full(int fd, char *buf, size_t n);
int write_all(int fd, const char *buf, size_t n);
