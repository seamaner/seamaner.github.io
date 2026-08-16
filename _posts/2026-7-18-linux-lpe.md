---
title: "Linux 本地提权技术分析"
date: 2026-07-18
categories: security
---

一个普通用户权限的进程要变成 root 权限的进程，从技术路径上大致可归为三类：一，借助内核漏洞直接控制执行流，修改进程的关键权限数据结构（如 `cred`）以完成提权；二，借助带有 SUID 位的程序（如 `passwd`）——例如通过内核内存错误篡改 `passwd` 的 page cache，磁盘文件本身不变，但任意用户运行 `passwd` 时就会执行恶意代码，类似手法还包括利用由内核发起的特权进程，如 `modprobe_path` 劫持、coredump 格式注入、cgroup `release_agent` 劫持；三，篡改 root 的认证过程，例如修改 `/etc/passwd` 增设 root 账户或篡改其密码，再用篡改后的密码通过 ssh 登录即可。这三类路径的共同点在于：要么在内核侧突破权限边界，要么在用户侧利用既有的特权程序或配置缺陷。

## 一、内核漏洞直接提权

通过内核漏洞直接控制执行流，修改进程的关键权限数据结构。

### 原理分析

Linux 内核中，每个进程由 `task_struct` 结构体表示，其中包含权限相关的字段：

```c
// include/linux/sched.h
struct task_struct {
    // ...
    const struct cred __rcu *real_cred;
    const struct cred __rcu *cred;
    // ...
};

// include/linux/cred.h
struct cred {
    kuid_t uid;            /* real UID */
    kgid_t gid;            /* real GID */
    kuid_t suid;           /* saved UID */
    kgid_t sgid;           /* saved GID */
    kuid_t euid;           /* effective UID */
    kgid_t egid;           /* effective GID */
    kuid_t fsuid;          /* filesystem UID */
    kgid_t fsgid;          /* filesystem GID */
    // ...
};
```

当进程需要检查权限时，内核会检查 `cred` 结构体中的 `euid` 等字段。如果能通过漏洞直接修改这些字段为 0（root 的 UID），进程就获得了 root 权限。

### 典型漏洞利用方式

1. **堆喷射 + 控制流劫持**
   - 利用内核堆漏洞（如 kmalloc 的 UAF、OOB）
   - 通过堆喷射布局恶意数据
   - 劫持内核控制流，执行提权代码

2. **直接修改 cred 结构**
   - 某些漏洞允许任意内核地址读写
   - 直接定位并修改当前进程的 `cred` 结构
   - 经典手法：劫持控制流执行 `commit_creds(prepare_kernel_cred(&init_task))`——`prepare_kernel_cred` 以 `init_task`（内核初始进程，root）的 cred 为模板复制一份 root cred，`commit_creds` 将其提交给当前进程；随后通过 `swapgs; iretq`（x86_64）等返回用户态，进程即获得 root 身份。注意：老式 PoC 常写的 `prepare_kernel_cred(NULL)` 自 6.1 起（commit `cred: Do not default to init_cred in prepare_kernel_cred()`）会触发 `WARN_ON_ONCE` 并返回 NULL，已不再可用，现代须显式传入 `&init_task` 或直接 `commit_creds(&init_cred)`（`init_cred` 现为 `init/init_task.c` 中的 static 变量，地址仍可经 `/proc/kallsyms` 解析）。
   - 指针重定向变体：与其逐字段改 `uid/euid/...`，不如直接把 `task_struct` 的 `cred`/`real_cred` 指针重定向到 `init_cred` 地址——等价于让当前进程直接套用 root cred，省去逐字段定位的麻烦，但同样要求先泄露 cred 指针与 `init_cred` 地址。


### 经典案例

- **CVE-2017-5123** (waitid 系统调用) - 缺少 `access_ok` 检查，可向内核地址写入任意数据
- **CVE-2021-22555** (Netfilter) - 堆块释放后类型混淆（UAF），经堆喷射劫持控制流提权
- **CVE-2022-0847** (Dirty Pipe) - 管道缓冲区 `PIPE_BUF_FLAG_CAN_MERGE` 缺陷，可越权覆盖只读文件 page cache
- **CVE-2022-0185** (Filesystem Context) - `legacy_parse_param` 整数溢出致堆溢出，可结合 cgroup 逃逸/提权

## 二、利用 SUID/SGID 程序

利用系统中已有的 SUID/SGID 程序作为提权跳板。

### Page Cache 篡改攻击

#### 原理

当程序执行时，内核会将其代码从磁盘加载到 page cache。如果能篡改 page cache 中的内容而不修改磁盘文件，当其他用户执行该程序时就会执行恶意代码。

#### 攻击条件

1. 有内核内存写入漏洞
2. 能定位目标文件的 page cache 页
3. 目标文件具有 SUID 位，**或**是会被 root 执行/读取（source）的文件（见下）

#### 典型场景

```
正常流程：
磁盘文件 → page cache → 进程内存执行

攻击流程：
磁盘文件(不变) → page cache(被篡改) → 进程执行恶意代码
```

#### 目标选择：从 SUID 到「root 会执行的任意文件」

SUID 文件只是最早被注意到的目标——任意用户主动运行它即可触发。但 page-cache 篡改的真正威力在于**不碰磁盘、不改属主、越过只读挂载与 `chattr +i`**，因此任何「root 会在将来某一刻执行或 source 的文件」都同等可用：

- SUID 二进制（`/usr/bin/passwd`、`/usr/bin/sudo` …）：任意用户主动触发，落地最快；
- `/root/.ssh/authorized_keys`：root 下次 ssh 登录即用攻击者公钥；
- `/etc/cron.*/`、systemd unit 文件：下一个 cron 周期 / `daemon-reload` 时由 root 执行；
- `/etc/profile.d/*`、`/root/.bashrc`：root 登录 shell source 时执行；
- `/etc/passwd` 本身：Dirty Pipe（CVE-2022-0847）的招牌 demo 正是只改 page cache、磁盘不动，借此增设无密码 root 账户。

落地收益取决于「root 何时会执行/读取该文件」——SUID 即时触发，cron/登录类需要等待窗口，但一旦触发即为 root。

### 特殊内核发起进程

#### modprobe_path 劫持

内核在处理未知模块时会调用 `modprobe`：

```c
// kernel/module/kmod.c
static int call_modprobe(char *module_name, int wait)
{
    // ...
    char *argv[] = { modprobe_path, "-q", "--", module_name, NULL };
    // ...
}
```

如果能修改 `modprobe_path` 内核变量，就可以让内核执行任意程序。`call_modprobe` 以 `init_cred`（即 root）身份通过 `call_usermodehelper_exec` 启动子进程，因此被调用的程序以 root 权限运行：

```
# 原始值
modprobe_path = "/sbin/modprobe"

# 篡改后
modprobe_path = "/tmp/malicious_binary"
```

触发条件：让内核尝试加载一个不存在的模块。最典型的做法是用未知协议族创建 socket，例如 `socket(AF_INET, SOCK_STREAM, 132)`（IPPROTO 值未在内核注册），内核在处理协议时调用 `request_module`，进而触发 `modprobe`。触发后 root 权限的 `/tmp/malicious_binary` 便被执行。

#### Coredump 格式注入

当进程崩溃时，内核会根据 `/proc/sys/kernel/core_pattern` 生成 coredump。当配置以 `|` 开头时，内核会通过 usermode helper 以 root 权限执行管道程序并把崩溃信息通过标准输入传入：

```
# 正常配置
|/usr/lib/systemd/systemd-coredump

# 恶意配置（需要 root 权限设置，但可利用内核漏洞写入）
|/tmp/malicious_script
```

如果内核漏洞允许写入此文件（非 `|` 前缀时是普通文件，可被任意内核写覆盖），可以注入恶意程序。由于 coredump 处理流程由内核发起，执行时具有特权，因此是常见的 LPE 落脚点。

#### cgroup release_agent 劫持

与 `modprobe_path` 形态最像的一条：一个可写的路径字符串变量 + 一个可主动触发的事件 → root 代码执行。

```c
// kernel/cgroup/cgroup-v1.c
// 路径变量：cgroup 根的 release_agent_path（经 release_agent cgroupfs 文件写入）
argv[0] = agentbuf;            // strscpy 自 release_agent_path
argv[1] = pathbuf;             // 被释放 cgroup 的路径
call_usermodehelper(argv[0], argv, envp, UMH_WAIT_EXEC);  // :838
```

触发条件：cgroup 设了 `notify_on_release`，当最后一个进程离开该 cgroup 时，内核排 `cgroup1_release_agent` work，以 root 身份执行 `release_agent_path` 指向的程序。任意内核写把该变量覆盖为 `/tmp/x`，再搬空某 cgroup 即可触发。

与 `modprobe_path` 的关键异同：
- 同：都是「可写路径变量 + 可触发事件 → root 执行」，落地范式一致。
- 同：同样经 `call_usermodehelper_setup`，因此 **也被 `CONFIG_STATIC_USERMODEHELPER` 收敛**（`call_usermodehelper` 便捷封装内部即调 setup，覆盖发生在 setup 阶段）。
- 异：触发动作是「搬空 cgroup」而非「未注册协议族 socket」；路径变量在 cgroup 根、而非 sysctl；仅 cgroup v1 提供，v2 无 `release_agent`。

> 截至 7.1/7.2-rc（核对自主线源码），这类「内核发起的特权 usermode helper」——`modprobe_path`、`core_pattern`、`poweroff_cmd`、`uevent_helper`、cgroup `release_agent`——**无一被移除**，因为它们承载着模块加载、崩溃转储、关机、热插拔、cgroup 释放等内核必需功能。唯一的变化是 `kmod.c` 从 `kernel/kmod.c` 迁入了 `kernel/module/kmod.c`（`modprobe_path` 仍定义于其第 64 行）。`core_pattern` 侧新增了 `name_contains_dotdot()` 与 `fs.suid_dumpable=2` 下的路径白名单校验（`fs/coredump.c`），加固的是「越权用户态写入」入口，并不影响「任意内核写直接覆盖变量」这条路径；`uevent_helper` 整段仍被 `CONFIG_UEVENT_HELPER` 包裹，现代发行版默认编译关闭。统一缓解仍是 `CONFIG_STATIC_USERMODEHELPER`（`kernel/umh.c:368`），在 `call_usermodehelper_setup` 阶段把所有 helper 路径收敛成编译期只读的 `CONFIG_STATIC_USERMODEHELPER_PATH`——`release_agent` 经 `call_usermodehelper` 便捷封装内部同样走 setup，故一并被覆盖。
>
> 此外还有几处更小众的「内核发起执行路径」，本质同族，仅在对应子系统加载且配置可达时才有意义：`binfmt_misc` 注册解释器（`fs/binfmt_misc.c`，execve 命中 magic/扩展名时由内核以注册的 interpreter 解释执行，注意默认以执行者身份运行，creds 语义不如 modprobe 干净）、`request_key` 的 key 实例化 helper（`security/keys/request_key.c`）、TOMOYO 策略加载器（`security/tomoyo/load_policy.c`）、NFSd/ocfs2 的回收与栈 helper（`fs/nfsd/*`、`fs/ocfs2/stackglue.c`）。

### 常见 SUID 程序利用点

| 程序 | 潜在风险 |
|------|---------|
| /usr/bin/passwd | 修改密码文件 |
| /usr/bin/sudo | 配置错误、漏洞 |
| /usr/bin/chsh | 修改 shell |
| /usr/bin/newgrp | 组切换 |
| /usr/bin/at | 定时任务执行 |

> 典型案例如 **CVE-2019-18634**：sudo 在开启 `pwfeedback`（输入密码回显星号）时，`getln()` 向栈上定长缓冲区逐字符写入而未检查长度，普通用户构造超长密码即可触发栈溢出并以 root 身份执行 `sudo`，完成提权。

## 三、篡改认证过程

通过修改认证相关文件实现提权。

### /etc/passwd 文件篡改

#### 文件结构

```
root:x:0:0:root:/root:/bin/bash
daemon:x:1:1:daemon:/usr/sbin:/usr/sbin/nologin
```

格式：`用户名:密码占位:UID:GID:描述:家目录:Shell`

#### 攻击方式

1. **添加 root 账户**
   ```
   toor:$6$salt$hash:0:0:root:/root:/bin/bash
   ```

2. **修改 root 密码**
   ```
   root:$6$newhash$:0:0:root:/root:/bin/bash
   ```

3. **无密码 root**
   ```
   root::0:0:root:/root:/bin/bash
   ```

### /etc/shadow 文件篡改

Shadow 文件存储实际的密码哈希：

```
root:$6$rounds=5000$salt$hash:18000:0:99999:7:::
```

如果能写入此文件，可以直接设置已知密码的哈希。

## 参考

- Linux Kernel Source Code (`kernel/cred.c`, `kernel/module/kmod.c`, `kernel/umh.c`, `fs/coredump.c`)
- Linux Kernel Exploitation Techniques
- MITRE ATT&CK: Privilege Escalation (T1068 Exploitation for Privilege Escalation)
- CVE-2021-22555 writeup, CVE-2022-0847 (Dirty Pipe) writeup, CVE-2022-0185 writeup
- phrack, libprivilege / kernel-exploit 相关公开资料
