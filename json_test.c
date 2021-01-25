#include <stdio.h>

#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>

#include "json.h"

#define KEYS(_) _(key1) _(key2) _(key3) _(key4)
JS_ENUM_DECL(keys, KEYS)

int main(void){
    int fd = open("test.json", O_RDONLY);
    js_buf * js = JS_FD_BUF(fd, err);
    js_trie * keys = JS_ENUM_TRIE(keys);
    JS_OBJ_ITER(js){
        switch(js_enum_key(js, keys)){
            CASE JS_key1:
                printf("'%s'\n", js_str(js));
            CASE JS_key2:
                printf("%d\n", js_int(js));
            CASE JS_key3:
                printf("%g\n", js_flt(js));
            CASE JS_key4:
                ; jmp_buf jb;
                JS_NEST_HDL(js, jb, nest_err);
                JS_ARR_ITER(js){
                    printf("%d\n", js_int(js));
                }
                JS_UNNEST_HDL(js, jb);
                break;
            nest_err:
                printf("oops!\n");
                JS_PROP_HDL(js, jb);
            DEFAULT:
                js_skip(js);
        }
    }
    close(fd);
    return 0;
err:
    printf("ERR\n");
    return 1;
}
