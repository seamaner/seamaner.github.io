#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>

void child() {
    if (fork() == 0) {
        printf("child func: my pid is %d\n", getpid());
        //exit(0);
    } else {
        printf("child func: my pid is %d\n", getpid());
    }
    
    printf("child func 2: my pid is %d\n", getpid());
}

int main(void) {
    child();
    printf("main func: my pid is %d\n", getpid());
    sleep(1);
    return 0;
}
