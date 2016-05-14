#include <sys/ptrace.h>
#include <sys/reg.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/syscall.h>
#include <sys/uio.h>
#include <sys/user.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <dlfcn.h>
#include <stdint.h>

int do_child(int argc, char **argv);
int do_trace(pid_t child, void *writev_ptr);


int main(int argc, char **argv, char **environ) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s prog args\n", argv[0]);
        exit(1);
    }

    pid_t child = fork();
    if (child == 0) {
        return do_child(argc-1, argv+1);
    } else {
        void *writev_ptr = dlsym(0, "writev_shim");
        return do_trace(child, writev_ptr);
    }
}

int do_child(int argc, char **argv) {
    char *args [argc+1];
    memcpy(args, argv, argc * sizeof(char*));
    args[argc] = NULL;

    ptrace(PTRACE_TRACEME);
    kill(getpid(), SIGSTOP);
    return execvp(args[0], args);
}

int wait_for_syscall(pid_t child);
ssize_t fake_writev(pid_t pid, int fd, const struct iovec *iov, int iovcnt);

void copy_to_child(pid_t pid, void *v_src, unsigned long dst, size_t len){
    unsigned long *src = (unsigned long *)v_src;
    int i, j;
    for(i = 0, j = 0; i < len; i += sizeof(unsigned long), j++){
        ptrace(PTRACE_POKEDATA, pid, dst + i, src[j]);
    }
}

void copy_from_child(pid_t pid, unsigned long src, void *v_dst, size_t len){
    unsigned long *dst = (unsigned long *)v_dst;
    int i, j;
    for(i = 0, j = 0; i < len; i += sizeof(unsigned long), j++){
        dst[j] = ptrace(PTRACE_PEEKDATA, pid, src + i);
    }
}

int do_trace(pid_t child, void *writev_ptr) {
    int status, syscall;
    ssize_t retval;
    waitpid(child, &status, 0);
    ptrace(PTRACE_SETOPTIONS, child, 0, PTRACE_O_TRACESYSGOOD);
    struct user_regs_struct regs;
    while(1) {
        if (wait_for_syscall(child) != 0) break;

        ptrace(PTRACE_GETREGS, child, NULL, &regs);
        syscall = regs.orig_rax;
        if(syscall == SYS_writev){
            int fd = regs.rdi;
            unsigned long iov_orig = regs.rsi;
            int iovcnt = regs.rdx;
            struct iovec iov[iovcnt];
            copy_from_child(child, iov_orig, iov, sizeof(iov));
            int i;
            struct iovec iov_zero[iovcnt];
            for(i = 0; i < iovcnt; i++){
                iov_zero[i].iov_base = iov[i].iov_base;
                iov_zero[i].iov_len = 0;
            }
            copy_to_child(child, iov_zero, iov_orig, sizeof(iov));

            if (wait_for_syscall(child) != 0) break;

            copy_to_child(child, iov, iov_orig, sizeof(iov));
            ptrace(PTRACE_GETREGS, child, NULL, &regs);
            regs.rsp -= sizeof(unsigned long);
            copy_to_child(child, &(regs.rip), regs.rsp, sizeof(unsigned long));
            regs.rip = (unsigned long)writev_ptr;
            ptrace(PTRACE_SETREGS, child, NULL, &regs);
            continue;
        }
        if(wait_for_syscall(child) != 0) break;
    }
    return 0;
}

int wait_for_syscall(pid_t child) {
    int status;
    while (1) {
        ptrace(PTRACE_SYSCALL, child, 0, 0);
        waitpid(child, &status, 0);
        if (WIFSTOPPED(status) && WSTOPSIG(status) & 0x80)
            return 0;
        if (WIFEXITED(status))
            return 1;
    }
}
