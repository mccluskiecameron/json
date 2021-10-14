
#include <stdio.h>

#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>

#include "json.h"

int main(void){
    int fd = open("unicode.json", O_RDONLY);
    js_buf * js = JS_FD_BUF(fd, err);
    JS_ARR_ITER(js){
        char * s = js_str(js);
        puts(s);
        free(s);
    }
    close(fd);
    return 0;
err:
    printf("ERR\n");
    return 1;
}
