---
layout: post
title: Golang GC 文件回收
categories: eBPF
description: Golang GC 文件回收
keywords: Golang, GC
---

gc 会回收文件吗（os.File）？   
会的，可以写一个函数循环`open`文件，可以计算出到资源限制`open`失败前总共可以打开多少文件。紧接着调用`GC`，并再次循环`open`文件，如果`GC`回收文件，则第二次循环应该和第一次循环打开同样数量的文件。  

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
无`GC`可以看到第二次循环打开的文件数为0：  
```
filename: /home/ubuntu/test/gogc.go
successful opens= 1048569
filename: /home/ubuntu/test/gogc.go
successful opens= 0
```
添加上`GC`后，可以看到第二次和第一次`open`的文件数一样:  
```
filename: /home/ubuntu/test/gogc.go
successful opens= 1048569
filename: /home/ubuntu/test/gogc.go
successful opens= 1048569
```
