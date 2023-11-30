---
layout: post
title: eBPF介绍
categories: eBPF
description: 1分钟简单介绍
keywords: eBPF
---

## 什么是eBPF？

- eBPF is a programming language and runtime to extend operating systems.
- eBPF is like JavaScript or Lua but for kernel Developers.  

- eBPF是一种编程语言和运行时，用于扩展操作系统。eBPF就像JavaScript或Lua一样，但面向内核开发人员。  
JavaScript代码如何运行？当某个事件发生时，例如当网站用户点击按钮并提交表单时。类似地，当内核中某个事件发生时，运行eBPF程序 -- 当系统调用被调用、网络数据包被处理、文件被访问等等。所以在概念上它们非常相似，它们使系统可编程。  
- 为什么选择eBPF？可编程性使内核可以快速适应不断变化的需求。  
操作系统，特别是Linux，变得非常难以改变。花费数周甚至数月的时间才能将更改提交到上游，然后需要数年时间才能让用户使用新版本。这样就不能一直适应不断变化的需求。eBPF使操作系统具有可编程性，从而可以不断适应不断变化的需求，从而快速创新。这是为什么eBPF存在的最重要的根本原因。
- eBPF的可编程性，与Lua或WebAssembly有什么不同呢？
eBPF专门设计用于嵌入到操作系统Linux和Windows中，它可以与Linux内核进行接口交互，因此可以调用内核API，并且它限制在安全地在内核上下文中运行。但它面向的是内核开发人员，因此传统上很难学习和使用。另一方面，Lua和WebAssembly被设计为嵌入到任意应用程序或系统中，它们是通用的，并且面向应用程序开发人员，更容易学习。但它们不能在操作系统的上下文中运行。因此，eBPF的一个独特特点是它嵌入到操作系统中，这是使eBPF独特的地方。例如，我们本可以选择Lua作为运行时，嵌入到内核中，并赋予它一个类似于eBPF今天的名称。

eBPF是如何工作的？它有一种语言和一种运行时。这种语言可以用多种不同的语言来表达，比如使用类似于C的代码，我们使用像LLVM或Clang这样的编译器将其编译成字节码。但也有几种不同的语言框架，比如IR libbpf BCC，可以使用不同的高级语言来表达eBPF程序。所有以上都是关于eBPF的一般介绍。如果你有任何具体的问题或需要更详细的信息，请随时提问。

## 发展历史

eBPF-timeline

- 2011 network virtualization and software-defined networking
- PLUMgrid公司的Alexei Starovoitov发明了eBPF的前身，一种基于x86汇编的指令集，在内核内校验后再运行，用于定位网络问题
- Alexei为了把他的发明合入Linux kernel，找到了Chris Wright(现在时Red Hat 的CTO，当时是SDN的首席架构师），Chris建议Alexei看一下cBPF，
  整合进已经在内核中存在的cBPF，这样便于内核社区理解和接收。
- 最初的patch没有引起内核维护者的兴趣，但引起了Daniel Borkmann and Thomas Graf的兴趣（此时两人工作与Red Hat，Daniel当时从事cBPF的工作），Daniel Borkmann and Thomas Graf去PLUMgrid的办公司拜访了Alexei，3人讨论怎样让内核社区接收。
- Daniel负责向Linux kernel网络团队评估eBPF的价值。Alexei也扩充了eBPF的应用场景，用于kernel tracing。
- 2014 Brendan Gregg(Netflix) 和Alexei讨论，增加kprobe支持，B-rendan增加周边支持，发展成现在的bcc和bpftrace

- 2014 合入linux kernel: David S. Miller 批准了v10版patch，视为对BPF的增强“if this enhances something we already have, why not?”

- 2017 关于eBPF的应用被在各大会议上被展示：
- 2017 NetDev展示了基于eBPF的L4 负载均衡Katran，获得了相对于IPVS 10倍的性能提升
- DockerCon2017 展示了Cilium - eBPF在网络和安全方面的应用，围绕cilium，创立了公司Isovalent

- Google宣布把Cilium 作为自家K8s(GKE)的网络层。随后AWS和微软也都跟进采用Cilium和eBPF作为他们k8s平台的网络。

- eBPF 移植到windows，IETF标准化
  
