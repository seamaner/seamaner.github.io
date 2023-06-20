# Open Sesame

`Something about forty thieves or something? I don't know, they must have had some secret incantation to get the gold!`
## Writeup
```
00000000000011eb <caveOfGold>:
11eb:       55                      push   %rbp
11ec:       48 89 e5                mov    %rsp,%rbp
11ef:       48 81 ec 10 01 00 00    sub    $0x110,%rsp
11f6:       c7 45 fc 00 00 00 00    movl   $0x0,-0x4(%rbp)
```
0x110 = 272
