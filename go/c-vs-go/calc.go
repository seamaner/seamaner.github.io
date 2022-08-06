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
