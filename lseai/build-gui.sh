export prefix=/usr/local/lse
gcc -g -std=gnu99 -Wall -include /usr/local/lse/include/lsecompat.h -c summarize.c 
gcc -g -std=gnu99 -Wall -include /usr/local/lse/include/lsecompat.h -c read_prompt.c 
gcc -g -std=gnu99 -Wall -include /usr/local/lse/include/lsecompat.h -c state_file.c 
gcc -g -std=gnu99 -Wall -include /usr/local/lse/include/lsecompat.h -c request.c 
gcc -g -std=gnu99 -Wall -include /usr/local/lse/include/lsecompat.h -c json.c 
gcc -g -std=gnu99 -Wall -include /usr/local/lse/include/lsecompat.h -c print_response.c 

gcc -g -O2 lseai-motif.c -o lseai-motif  -I/usr/dt/include -I/usr/openwin/include -L/usr/dt/lib -R/usr/dt/lib -L/usr/openwin/lib -R/usr/openwin/lib -lXm -lXt -lX11 -lsocket -lnsl \
    summarize.o \
    read_prompt.o \
    state_file.o \
    request.o \
    json.o \
    print_response.o \
    -lreadline -lcurses -lcurl -lssl -lcrypto -llsecompat
