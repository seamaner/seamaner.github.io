*recon*
- /robots.tx
- /.well-known/security.txt
- 404 page
- directory list
    /admin/, /images/, /css/, ..., try to fuzz
- TLS named alternative names
    change "Host" to  'SAN'
    openssl s_client -connect hackycorp.com:443 </dev/null 2>/dev/null | openssl x509 -noout -text | grep DNS:
    openssl x509 -noout -text -in hackycorp.com | grep DNS;
- Virtual host
    change header "Host" & try http/https
    Host headere fuzz
    ffuf -H "Host: FUZZ.hackycorp.com"  -c -w "/usr/share/dirb/wordlists/small.txt" -u https://hackycorp.com -fs 107
    btw: ffuf is very quick.
- TXT record
    search from mxtoolbox.com
- DNS zone transfer 
    dig axfr z.hackycorp.com @ns1.hackycorp.com
- DNS version
    dig chaos txt version.bind @z.hackycorp.com
