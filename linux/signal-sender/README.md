程序经常收到SIGTERM退出，一直没查明白SIGTERM是谁发送的。signal函数注册的处理函数只接收到signum，不知道发送者是谁。一番查找发现sigaction函数注册的sa_sigaction回调函数，有siginfo_t结构，查看man其中的si_pid就是发送者的pid。

## si_pid  

**sigaction**
```
#include <signal.h>
int sigaction(int signum, const struct sigaction *act,
        struct sigaction *oldact);
```
**sigaction struct**:
```C
struct sigaction {
    void     (*sa_handler)(int);
    void     (*sa_sigaction)(int, siginfo_t *, void *);
    sigset_t   sa_mask;
    int        sa_flags;
    void     (*sa_restorer)(void);
};
```
**siginfo_t**
```C
siginfo_t {
    int      si_signo;     /* Signal number */
    int      si_errno;     /* An errno value */
    int      si_code;      /* Signal code */
    int      si_trapno;    /* Trap number that caused
                              hardware-generated signal
                              (unused on most architectures) */
    pid_t    si_pid;       /* Sending process ID */
    uid_t    si_uid;       /* Real user ID of sending process */
    int      si_status;    /* Exit value or signal */
    clock_t  si_utime;     /* User time consumed */
    clock_t  si_stime;     /* System time consumed */
    sigval_t si_value;     /* Signal value */
    int      si_int;       /* POSIX.1b signal */
    void    *si_ptr;       /* POSIX.1b signal */
    int      si_overrun;   /* Timer overrun count;
                              POSIX.1b timers */
    int      si_timerid;   /* Timer ID; POSIX.1b timers */
    void    *si_addr;      /* Memory location which caused fault */
    long     si_band;      /* Band event (was int in
                              glibc 2.3.2 and earlier) */
    int      si_fd;        /* File descriptor */
    short    si_addr_lsb;  /* Least significant bit of address
                              (since Linux 2.6.32) */
    void    *si_lower;     /* Lower bound when address violation
                              occurred (since Linux 3.19) */
    void    *si_upper;     /* Upper bound when address violation
                              occurred (since Linux 3.19) */
    int      si_pkey;      /* Protection key on PTE that caused
                              fault (since Linux 4.6) */
    void    *si_call_addr; /* Address of system call instruction
                              (since Linux 3.5) */
    int      si_syscall;   /* Number of attempted system call
                              (since Linux 3.5) */
    unsigned int si_arch;  /* Architecture of attempted system call
                              (since Linux 3.5) */
}
```

## example  

**sender.c**
```C
#include<stdio.h>
#include<signal.h>
#include<unistd.h>
#include <sys/wait.h>

static void handler(int sig, siginfo_t* s, void* uctx) {
    printf("si_signo=%d, si_status=%d, si_code=%d, si_pid=%ld, si_uid=%ld, si_addr=0x%lx\n",
           s->si_signo, s->si_status, s->si_code, (long)s->si_pid, (long)s->si_uid, (unsigned long)s->si_addr);
}

int main(){
    struct sigaction act = {
        .sa_sigaction = &handler,
        .sa_flags = SA_SIGINFO,
    };

    sigaction(SIGTERM, &act, NULL);

    if (fork()==0) {
        kill(getppid(),SIGTERM);
        printf("myself is %d\n", getpid());
        _exit(0);
    }
    wait(NULL);
    printf("myself is %d\n", getpid());

    return 0;
}
```
**运行结果**：
```
 gcc -o sender sender.c

./sender
myself is 58579
si_signo=15, si_status=0, si_code=0, si_pid=58579, si_uid=1000, si_addr=0x3e80000e4d3
myself is 58578
```

