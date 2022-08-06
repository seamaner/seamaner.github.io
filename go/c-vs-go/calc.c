#include<stdio.h>
#include<sys/time.h>
long long  get_current_ms() {
    struct timeval t;
    gettimeofday(&t,NULL);
    return t.tv_sec * 1000 + t.tv_usec / 1000;
}

int main() {
    long long start_ms = get_current_ms(); 
    double sum = 0.0;
    for (int i = 0; i < 100000000; i ++) {
        double tmp = 10000 + 0.01 * i;
        sum += tmp * tmp - tmp;
    }
    long long end_ms = get_current_ms();
    printf("calc %e in c, cost %lld ms\n", sum, end_ms - start_ms);
}
