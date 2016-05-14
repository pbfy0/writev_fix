#include <unistd.h>
#include <sys/uio.h>
#include <stdio.h> 

ssize_t writev_shim(int fd, const struct iovec *iov, int iovcnt){
    ssize_t len_written = 0;
    int i;
    for(i = 0; i < iovcnt; i++){
        len_written += write(fd, iov[i].iov_base, iov[i].iov_len);
    }
    return len_written;
}
