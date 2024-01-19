gc 会回收文件吗（os.File）？
会的，可以写一个函数始终
```
package main

import (
    "fmt"
    "os"
    "runtime"
)

func allocate() int {
    //use this file as target file
    _, fileName, _, ok := runtime.Caller(1)
    if !ok {
        panic("source file not found")
    }
    fmt.Printf("filename: %s\n", fileName)

    count := 0
    var files []*os.File
    for {
        f, err := os.Open(fileName)
        if err != nil {
            fmt.Printf("successful opens= %d\n", count)
            return count
        }
        count++
        files = append(files, f)
    }
}

func main() {
    allocate()
   // runtime.GC()
    allocate()
}
```
