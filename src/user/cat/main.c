#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int main(int argc, char *argv[])
{
    /* (Final) TODO BEGIN */
    int fd, n;
    char buf[512];
    for(int i = 1; i < argc; i++){
        if((fd = open(argv[i], O_RDONLY)) < 0){
            continue;
        }
        while((n = read(fd, buf, sizeof(buf))) > 0){
            for(int j = 0; j < n; j++){
                fprintf(stdout, "%c", buf[j]);
            }
        }
        close(fd);
    }
    if(argc == 1){
        while((n = read(0, buf, sizeof(buf))) > 0){
            for(int j = 0; j < n; j++){
                fprintf(stdout, "%c", buf[j]);
            }
        }
    }
    /* (Final) TODO END */
    return 0;
}