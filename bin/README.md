# how do you get these .lib and .a files?

### windows:

vcpkg.exe install curl[core,http2,ssh,sspi,brotli,idn,winidn,winldap,websockets,non-http]:x86-windows-static

### linux:

sudo ./vcpkg install  curl[core,http2,ngtcp,ssh,brotli,websockets,non-http,ssl]:x86-linux

