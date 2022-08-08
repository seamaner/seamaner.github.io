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
				//	fmt.Printf("goroutine2 not my turn\n")
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
C代码：
```
```

