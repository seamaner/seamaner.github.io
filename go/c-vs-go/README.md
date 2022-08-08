这篇是"理解Golang"系列的第一篇，主要涉及Go在性能方面的一些特点。接下来的几篇涉及scheduler、routine、chan的具体实现。

CPU速度远快于内存，这就像两个转动的齿轮，一个比另一个转动的要快，等待读取内存时，CPU将浪费掉不少指令。CPU的L1/L2/L3 Cache就是通过缓存来缩短平均读取时间。

内核线程切换约消耗可以执行10K条指令的时间，另一面，L1/L2 cache都是每个core独有的。如果一个thread切换到不同的core执行，一方面切换本身有消耗，另一方面再次运行时落在不同的core上，cache都将失效，这对性能影响是非常大的。

线程执行的任务分这么几类：
- CPU-bound：CPU消耗型  
典型的有数学计算，如计算π的前n位
- IO-bound: IO消耗型
- Memory-bound
- Cache-bound

## CPU-bound

ball.c 代码：
```C
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
```
calc.go 代码：
```go
package main

import (

    "fmt"
    "time"
)

func main() {
    x := 0.0
    start := time.Now()
    for i := 1.0; i < 100000000; i++ {
        tmp := 10000+i*0.01
        x +=  tmp*tmp - tmp
    }
    consume := time.Since(start)
    fmt.Printf("calc %e in go, took %s\n", x, consume)
}
```
编译C `gcc -O2  calc.c -lm`,一定要带上-O2性能优化，C在不优化的情况下，运行时间比go大得多；
编译go ` go build calc.go`;
运行：
```
./calc;./a.out
calc 3.434328e+19 in go, took 160.97609ms
calc 3.434328e+19 in c, cost 141 ms

./calc;./a.out
calc 3.434328e+19 in go, took 189.720358ms
calc 3.434328e+19 in c, cost 130 ms
```
从结果上看，同样完成1亿次x*x-x的运算，C和Go都是非常快的，Go和C的差距也不是特别大。和C一样，Go也是直接编译成可执行程序，这种纯计算型的任务（CPU-bound）结果相近也在情理之中。

## IO-bound

锁、同步system call、IO（socket、硬盘读写）等会阻塞，CPU会将程序换出。  
设想这样一个游戏，A将球踢给B，B收到球后再讲球踢给A，游戏的规则是只有一个球，A拿球期间B只能等着，同样B拿到球后A只能等待B将球传给他在进行下一次游戏。用程序来模拟：
Go代码：
```go
package main

import (
    "fmt"
    "time"
)
var myTurn int = 1
var ball int = 1
var total int = 100000000
func main() {
    chan1to2 := make(chan int)
    chan2to1 := make(chan int)
    quit := make(chan int)
    start:=time.Now()

    go func() {
        for {
        select {
            case chan1to2 <- ball:
                //fmt.Printf("goroutine1-1 ball %d\n", ball)
                ball = <-chan2to1
                //fmt.Printf("goroutine1-2 ball %d\n", ball)
                //fmt.Printf("goroutine1 not my turn\n")
                //myTurn++
            }
            if (ball >= total) {
                close(quit)
                return
            }
    }
    }()

    go func() {
        for {
            select {
            case i:= <-chan1to2:
                //fmt.Printf("goroutine2-1 ball %d\n", i)
                i++
                chan2to1 <- i
                //fmt.Printf("goroutine2-2 ball %d\n", i)
                //if (myTurn != 2) {
                //  fmt.Printf("goroutine2 not my turn\n")
                //}
                //myTurn--
            }
            if (ball >= total) {
                return
            }
        }
    }()
    <-quit
    during:=time.Since(start)
    fmt.Printf("kick ball %d times in go, cost %s\n", total, during)
}
```
玩100000000次"击球传花", Go花了56秒：  

```
./ball
kick ball 100000000 times in go, cost 56.704199515s
```

用C来实现最先想到的是用条件变量，C代码：  
ball.c  
```
#include <pthread.h>
#include <stdio.h>
#include <unistd.h>
#include<sys/time.h>
 
long long  get_current_ms() {
    struct timeval t;
    gettimeofday(&t,NULL);
    return t.tv_sec * 1000 + t.tv_usec / 1000;
}

pthread_cond_t cond1 = PTHREAD_COND_INITIALIZER;
pthread_cond_t cond2 = PTHREAD_COND_INITIALIZER;
 
pthread_mutex_t lock1 = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t lock2 = PTHREAD_MUTEX_INITIALIZER;
int ball = 1;
//int total = 100000000;
int total = 1000000;
int msg1to2 = 0; 
int msg2to1 = 0; 
int myTurn =1;
void* func1() {
    int tmp = 0; 
    long long start_ms = get_current_ms(); 
    while (1) {
        pthread_mutex_lock(&lock1);
        if (myTurn == 1) {
            //printf("thread1 msg1to2 cond1\n");
            msg1to2 = tmp;
            myTurn = 2;
            pthread_cond_signal(&cond1);
            //printf("thread1: Signaling cond2, %d\n", tmp);
        } else {
            // let's wait on condition variable cond1
            //printf("thread1: Waiting on cond1\n");
            pthread_cond_wait(&cond1, &lock1);
            tmp = msg2to1;
            if (tmp >= total) {
                pthread_mutex_unlock(&lock1);
                break;
            }
        }
        pthread_mutex_unlock(&lock1);
    }
    long long end_ms = get_current_ms();
    printf("kick ball %d times in c, cost %lld ms\n", tmp, end_ms - start_ms);
    printf("Returning thread1\n");
    return NULL;
}

void* func2() {
    int tmp = 0; 
    while (1) {
        pthread_mutex_lock(&lock1);
        if (myTurn == 2) {
            //printf("thread2 msg1to2 cond1\n");
            msg2to1 = tmp;
            myTurn =1;
            pthread_cond_signal(&cond1);
            //printf("thread2: Signaling cond2, %d\n", tmp);
            if (tmp >= total) {
                pthread_mutex_unlock(&lock1);
                break;
            }
        } else {
            // let's wait on condition variable cond1
            //printf("thread2: Waiting on cond1\n");
            pthread_cond_wait(&cond1, &lock1);
            tmp = msg1to2;
            tmp++;
        }
        pthread_mutex_unlock(&lock1);
    }
    printf("Returning thread2\n");
    return NULL;
}

int main()
{
    pthread_t tid1, tid2;

    // thread 1
    pthread_create(&tid1, NULL, func2, NULL);

    sleep(1);

    // thread 2
    pthread_create(&tid2, NULL, func1, NULL);

    // wait for the completion of thread 2
    pthread_join(tid2, NULL);

    return 0;
}
```
编译，`gcc -O3 ball.c -lm -lpthread -o ball-c`
**结果**，相对Go的实现，运行时间非常长，运行1000000次也需要20+s。尝试改成用`pipe`代替条件变量。  
`ball-2.c`，代码：  
```
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include<sys/time.h>
#include<sys/types.h>
 
long long  get_current_ms() {
    struct timeval t;
    gettimeofday(&t,NULL);
    return t.tv_sec * 1000 + t.tv_usec / 1000;
}

int pipe1to2[2];
int pipe2to1[2];
//int total = 100000000;
int total = 1000000;
void* func1() {
    long long start_ms = get_current_ms(); 
    int tmp = 0;
    while (1) {
        write(pipe1to2[1], &tmp, sizeof(tmp));
        //printf("thread1 write msg1to2 %d\n", tmp);
        read(pipe2to1[0], &tmp, sizeof(tmp));
        //printf("thread1 read msg2to1 %d\n", tmp);
        if (tmp >= total) {
            break;
        }
    }
    long long end_ms = get_current_ms();
    printf("kick ball %d times in c, cost %lld ms\n", tmp, end_ms - start_ms);
    printf("Returning thread1\n");
    return NULL;
}

void* func2() {
    long long start_ms = get_current_ms(); 
    int tmp = 0;
    while (1) {
        read(pipe1to2[0], &tmp, sizeof(tmp));
        //printf("thread2 read pipe1to2 %d\n", tmp);
        tmp++;
        write(pipe2to1[1], &tmp, sizeof(tmp));
        //printf("thread2 write pipe2to1 %d\n", tmp);
        if (tmp >= total) {
            break;
        }
    }
    long long end_ms = get_current_ms();
    printf("kick ball %d times in c, cost %lld ms\n", tmp, end_ms - start_ms);
    printf("Returning thread2\n");
    return NULL;
}

int main() {
    pthread_t tid1, tid2;

    if (pipe(pipe1to2) == -1) {
        perror("pipe");
        exit(1);
    }
    if (pipe(pipe2to1) == -1) {
        perror("pipe");
        exit(1);
    }

    // thread 1
    pthread_create(&tid1, NULL, func2, NULL);


    // thread 2
    pthread_create(&tid2, NULL, func1, NULL);

    // wait for the completion of thread 2
    pthread_join(tid2, NULL);

    return 0;
}
```
编译：`gcc -O3 ball-2.c -lm -lpthread -o ball-c2`  
**运行结果**  
```
./ball-c;./ball-c2;
Returning thread2
kick ball 1000000 times in c, cost 29098 ms
Returning thread1
kick ball 1000000 times in c, cost 20495 ms
Returning thread2
kick ball 1000000 times in c, cost 20495 ms
Returning thread1
```
换成`pipe`的代码好了许多，但执行1M次也运行了20s，这远慢于使用goroutine的Go版本。  

执行CPU bound任务，Go比C要慢一些，但差别并不是特别明显。在可以使用goroutine切换替代频繁thread切换的情形，Golang展示出非常好的性能。相比于kernel thread context switch 12K条指令左右的时间开销，goroutine开销在2~3K左右，再考虑到cache miss的开销，实际项目中值得留意schedule的开销。  

- 同步调用可能会将执行routine的线程（M）置为Waiting状态

参考资料：  
- https://stackoverflow.com/questions/868568/what-do-the-terms-cpu-bound-and-i-o-bound-mean
- https://www.ardanlabs.com/blog/2018/08/scheduling-in-go-part1.html
- https://www.atatus.com/ask/cpu-io-memory-bound

