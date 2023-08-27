---
layout: post
title: eBPF：hello world
categories: eBPF
description: 用eBPF字节码写的“hello world”
keywords: eBPF
---

有许多用libbpf或者go-bpf写的eBPF例子，python等别的语言的也有很多。这些都不够直观，代码都经过了层层包装，不能很好的明白eBPF是怎么工作的。
本文用最原始最直接的方式展示eBPF是如何工作的---用eBPF字节码写一个“hello world”。

## 什么是eBPF
"register-based virtual machine" 基于寄存器的虚拟机。不同于KVM等system virtual machine技术，和Java 虚拟机、Lua虚拟机类似，是一种process virtual machine。
进程虚拟机最常用的设计模式有两种：
- register-base virtual machine
- stack-based virtual machine
两种设计模式没有本质的区别，只是计算方式不同。
/*补充具体的代码示例*/
eBPF也可以说是跑在内核空间内，只能运行内核允许运行的任务的语言，是一种“特定领域语言”。如果不考虑JIT，只是模拟运行，几乎可以随意设计“特定领域语言”，一个简单的register-based vm：
/*补充vm例子*/

那么eBPF是如何设计的？看一下eBPF字节码。

## eBPF字节码

## hello world
C hello world
就是把“hello world”输出出来。
BPF系统调用
/* 补充 man bpf内容*/
打印函数
如何把字符串传给给内核呢？
字符串共13个字节。可以分拆成几段，这样可以存在寄存器上，接着可以保存到stack上。
最后，调用print函数完成输出。

代码最终的结果：

如何运行？
编译 -》 加载到内核
一般需要关联到内核事件，内核整个是由事件驱动的。查看man 发现run test CMD

运行结果查看



