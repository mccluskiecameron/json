
#include <stdio.h>

#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>

#include "json.h"

#define KEYS(_) _(key1) _(key2) _(key3) _(key4)
JS_ENUM_DECL(keys, KEYS)

char buf[1024];

int main(void){
    int fd = open("long.json", O_RDONLY);
    js_buf * js = JS_FD_BUF(fd, err);
    js_skip(js);
    js_skip(js);
    js_skip(js);
    js_skip(js);
    js_skip(js);
    js_skip(js);
    js_skip(js);
    js_skip(js);
    close(fd);
    return 0;
err:
    printf("ERR\n");
    return 1;
}
