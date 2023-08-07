---
layout: post
title: eBPF：hello world
categories: eBPF
description: 用eBPF字节码写的“hello world”
keywords: eBPF
---

有许多用libbpf或者go-bpf写的eBPF例子，python等别的语言的也有很多。这些都不够直观，代码都经过了层层包装，不能很好的明白eBPF是怎么工作的。
本文用最原始最直接的方式展示eBPF是如何工作的---用eBPF字节码写一个“hello world”。

