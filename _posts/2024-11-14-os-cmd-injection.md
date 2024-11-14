---
layout: post
title: OS command injection
categories: security
description: command injection 
keywords: security command indection
---

有些应用是通过后台执行OS 命令实现的（如shell）， 像导出tech-support文件. 如果又使用了用户输入作为命令的一部分就很容易出现命令注入(command injection)漏洞。  

## Ways of injecting OS commands

**分割字符**
```
- &
- &&
- | 
- ||
```
**unix独有分割字符**
```
- ;
- 0x0a or \n
```
**unix inline execution**
```
- `
- $(
```

## how to prevent

The most effective way to prevent OS command injection vulnerabilities is to never call out to OS commands from application-layer code. In almost all cases, there are different ways to implement the required functionality using safer platform APIs.

If you have to call out to OS commands with user-supplied input, then you must perform strong input validation. Some examples of effective validation include:

- Validating against a whitelist of permitted values.
- Validating that the input is a number.
- Validating that the input contains only alphanumeric characters, no other syntax or whitespace.
Never attempt to sanitize input by escaping shell metacharacters. In practice, this is just too error-prone and vulnerable to being bypassed by a skilled attacker.


## 参考资料  
[portswigger/web-security/os-command-injection](https://portswigger.net/web-security/os-command-injection)  
[pwn.college/inter/web-security/cmdi](https://pwn.college/intro-to-cybersecurity/web-security/)  

