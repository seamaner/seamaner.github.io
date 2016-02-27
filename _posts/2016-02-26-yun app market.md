---
layout: blog
title: 云市场 - Google/Aliyun/AWS market
---
[AWS marketplace](https://aws.amazon.com/marketplace/)
[Google b2b app marketplace](https://apps.google.com/marketplace/u/0/?pann=gam)




##IAAS or SAAS
Google: App for work，效率高的多。Google Apps 是一款专为您企业设计的基于云的增效套件，可帮助您随时随地在任何设备上完成工作。
Aliyun: 主机租赁业务，只不过是云主机。不需要运维管理方便一些。

面对不同需求，云主机租赁还是会继续发展，更高效率的SAAS相信Aliyun也会很快完善起来。

```
.section .data
    msg:
       .ascii  "hello, world!"
 
.section .text
    .global _start
_start:
    mov     $1, %rax
    mov     $1, %rdi
    mov     $msg, %rsi
    mov     $13, %rdx
    syscall
    mov    $60, %rax
    mov    $0, %rdi
    syscall

```

##编译成可执行文件

```
as -o print.o hello.S
ld -o print print.o
```
