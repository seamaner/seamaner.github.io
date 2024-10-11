遇到tirpc方面的编译报错，解决办法：
安装libntirpc-dev:  
```
sudo apt install libntirpc-dev
ln -s  /usr/include/tirpc/netconfig.h /usr/include/
ln -s /usr/include/tirpc/rpc/types.h /usr/include/rpc/  
```

`Undefined reference to 'xdr_int'`:  
  `LIBS += -ltirpc`
