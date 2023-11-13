---
layout: post
title: eBPF：hello world
categories: eBPF
description: 用eBPF字节码写的“hello world”
keywords: eBPF
---

有许多用libbpf或者go-bpf写的eBPF例子，python等别的语言的也有很多。这些都不够直观，代码都经过了层层包装，不能很好的明白eBPF是怎么工作的。
本文用最原始最直接的方式展示eBPF是如何工作的 --- 用eBPF字节码写一个“hello world”。

## 什么是eBPF

"register-based virtual machine" 基于寄存器的虚拟机。不同于KVM等system virtual machine技术，和Java 虚拟机、Lua虚拟机类似，是一种process virtual machine。
进程虚拟机最常用的设计模式有两种：
- register-base virtual machine 
- stack-based virtual machine
  
两种设计模式没有本质的区别，只是计算方式不同。 
计算两个整数相加，用register-based virtual machine，字节码是： 
```
 1 LOAD reg0 #100
 2 LOAD reg1 #200
 3 ADD reg2 reg1 reg0
```
第1、2行把100、200分别存入寄存器reg0和reg1；  
第3行把两个数相加，把结果存到寄存器reg2上。  
如果用基于stack的方式计算，数据先放在stack上的，计算时先执行两次POP，计算结果再存回到栈上。  
```
r = STACK[sp--];
l = STACK[sp--];
STACK[++sp] = (r + l);
``` 
eBPF也可以说是跑在内核空间内，只能运行内核允许运行的任务的语言，是一种“特定领域语言”。如果不考虑JIT，只是模拟运行，几乎可以随意设计“特定领域语言”。  
“寄存器”并不是CPU的寄存器，确实只是模拟的寄存器，在eBPF里（eBPF的解释器，也就是内核代码里的C函数）“寄存器”就是uint64的整数数组或全局变量。  
一个简单的[register-based VM](https://github.com/seamaner/eBPF-tutorial/blob/main/hello-world/vm.c)     
在这个VM里“寄存器”就是`int`数组`int regs[NUM_REGS];`。    
    
那么eBPF是如何设计的？看一下eBPF字节码。   

## eBPF字节码

每个字节码实际上是8个字节，操作码占8位，源目的寄存器各占4位，offset 16位，立即数32位：      
`opcode:8 src_reg:4 dst_reg:4 offset:16 imm:32 // In little-endian BPF.`     

## hello world

### C hello world 

就是把“hello world”输出出来:    
```
printf("%s\n", "Hello world!\n");
```

### eBPF Hello world

eBPF是在内核内运行的，为了输出“Hello world”，需要3步：  
- 准备eBPF字节码
- 使用BPF系统调用加载eBPF字节码到内核
- 在内核的运行

BPF系统调用  
``` 
int bpf(int cmd, union bpf_attr *attr, unsigned int size);

struct {    /* Used by BPF_PROG_LOAD */
                   __u32         prog_type;
                   __u32         insn_cnt;
                   __aligned_u64 insns;      /* 'const struct bpf_insn *' */
                   __aligned_u64 license;    /* 'const char *' */
                   __u32         log_level;  /* verbosity level of verifier */
                   __u32         log_size;   /* size of user buffer */
                   __aligned_u64 log_buf;    /* user supplied 'char *'
                                                buffer */
                   __u32         kern_version;
                                             /* checked when prog_type=kprobe
                                                (since Linux 4.1) */
};
```

#### 如何打印

打印函数  
可以用helper函数`bpf_trace_printk`输出。    

#### 如何把字符串传给给内核呢？

动手写才发现，通过系统调用bpf传递给内核的是字节码序列，这不像C可以直接输出字符串，“hello world\n”共13个字节。可以分拆成几段，每4字节段（imm是32bit），这样可以存在寄存器上，接着可以保存到stack上。  
最后，调用print函数完成输出。  

字节码代码最终的结果：  

```
struct bpf_insn bpf_prog[] = {
    { 0xb7, 1, 0, 0, 0 }, //r1 = 0
    { 0x73, 0xa, 0x1, 0xfffc, 0}, //*(u8*)(r10 - 4 ) = r1
    { 0xb7, 1, 0, 0, 0x0a646c72}, //r1 = b'\ndlr'
    { 0x63, 0xa, 0x1, 0xfff8, 0}, //*(u32*)(r10 - 8 ) = r1
    { 0x18, 1, 0, 0, 0x6c6c6568}, //r1 = b'lleh'
    { 0x00, 0, 0, 0, 0x6f77206f}, //b'ow o'
    //7b 1a e0 ff 00 00 00 00 *(u64 *)(r10 - 16) = r1
    { 0x7b, 0xa, 0x1, 0xfff0, 0}, 
    {0xbf, 0x1, 0xa, 0, 0}, //r1 = r10
    {0x07, 0x1, 0, 0, 0xfffffff0}, //r1 += -16
    //b7 02 00 00 08 00 00 00 r2 = 13
    {0xb7, 2, 0, 0, 0x0d},
    //85 00 00 00 06 00 00 00 call 6
    {0x85, 0, 0, 0, 0x06},
    {0xb7, 0, 0, 0, 0x1},//r0 = 1
    {0x95, 0, 0, 0, 0x0 }, //exit;
};
```

#### 如何运行

查看kernel doc发现可以用BPF_PROG_RUN  

```
The BPF_PROG_RUN command can be used through the bpf() syscall to execute a BPF program in the kernel and return the results to userspace. This can be used to unit test BPF programs against user-supplied context objects, and as way to explicitly execute programs in the kernel for their side effects. The command was previously named BPF_PROG_TEST_RUN, and both constants continue to be defined in the UAPI header, aliased to the same value.
```
得到代码：  
```
int bpf_prog_test_run(int prog_fd)
{
    struct ipv4_packet pkt_v4;//must have one to pass TEST_RUN check
    union bpf_attr attr = {
        .test.prog_fd = prog_fd,        
        .test.repeat = 1,
        .test.data_in = (unsigned long)&pkt_v4,
        .test.data_size_in = sizeof(pkt_v4),
    };

    return bpf(BPF_PROG_RUN, &attr, sizeof(attr));
}
```

#### 编译 -》 加载到内核 

```
int main(void){
    int prog_fd = bpf_prog_load(BPF_PROG_TYPE_SOCKET_FILTER, bpf_prog, sizeof(bpf_prog)/sizeof(bpf_prog[0]), "GPL");
    //int prog_fd = bpf_prog_load(BPF_PROG_TYPE_CGROUP_SKB, bpf_prog, sizeof(bpf_prog)/sizeof(bpf_prog[0]), "GPL");
    if (prog_fd < 0) {
        perror("BPF load prog");
        exit(-1);
    }
    printf("%s", bpf_log_buf);
    printf("prog_fd: %d\n", prog_fd);
    int ret = bpf_prog_test_run(prog_fd);
    printf("%s(pid %d): ret %d\n", "run this hello world", getpid(), ret);
    return 0;
}
```  
完整代码见这里：https://github.com/seamaner/eBPF-tutorial/blob/main/hello-world/hello.c

#### 运行结果查看  

bpftool prog tracelog    
![tracelog](/images/ebpf/eBPF-hello-world-tracelog.png)
