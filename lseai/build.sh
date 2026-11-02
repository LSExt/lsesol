export prefix=/usr/local/lse
gcc -std=gnu99 -Wall -include /usr/local/lse/include/lsecompat.h -c summarize.c 
gcc -std=gnu99 -Wall -include /usr/local/lse/include/lsecompat.h -c read_prompt.c 
gcc -std=gnu99 -Wall -include /usr/local/lse/include/lsecompat.h -c state_file.c 
gcc -std=gnu99 -Wall -include /usr/local/lse/include/lsecompat.h -c request.c 
gcc -std=gnu99 -Wall -include /usr/local/lse/include/lsecompat.h -c json.c 
gcc -std=gnu99 -Wall -include /usr/local/lse/include/lsecompat.h -c print_response.c 

gcc -std=gnu99 -O2 -o lseai lseai.c -I/usr/local/include -I/usr/local/lse/include \
    -include /usr/local/lse/include/lsecompat.h -L/usr/local/lib -L/usr/local/lse/lib -Wl,-rpath,$prefix/lib \
    summarize.o \
    read_prompt.o \
    state_file.o \
    request.o \
    json.o \
    print_response.o \
    -lreadline -lcurses -lcurl -lssl -lcrypto -llsecompat
