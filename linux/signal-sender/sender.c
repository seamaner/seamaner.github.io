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
