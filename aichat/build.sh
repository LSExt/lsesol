gcc -std=gnu99 -Wall -include /usr/local/lse/include/lsecompat.h -c summarize.o summarize.c 

gcc -std=gnu99 -O2 -o aichat aichat.c -I/usr/local/include -I/usr/local/lse/include -include /usr/local/lse/include/lsecompat.h -L/usr/local/lib -L/usr/local/lse/lib -Wl,-rpath,$prefix/lib summarize.o -lreadline -lcurses -lcurl -lssl -lcrypto -llsecompat
