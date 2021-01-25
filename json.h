#ifndef JS_H
#define JS_H
#include <unistd.h>
#include <setjmp.h>
#include <string.h>

#include "new.h"

#define CASE break; case
#define DEFAULT break; default

typedef struct js_trie {
    char * name;
    int val;
    struct js_trie ** tab;
} js_trie;

typedef unsigned char u8;

js_trie * js_trie_init(void);
js_trie * js_trie_ins(js_trie * tr, char * name, int val);
js_trie * js_trie_ins_all(js_trie * tr, int n, char ** names);

#define __ENUM_KEY(name) JS_##name,
#define __ENUM_STRING(name) #name,
#define JS_ENUM_DECL(name, expand)\
    enum _js_enum_##name{\
        expand(__ENUM_KEY) _js_keycount##name\
    };\
    char * _js_keynames##name[_js_keycount##name+1] = {\
        expand(__ENUM_STRING) 0\
    };
#define JS_ENUM_TRIE(name)\
    js_trie_ins_all(js_trie_init(), _js_keycount##name, _js_keynames##name);

typedef struct {
    intptr_t arg;
    ssize_t (*read)(int arg, void * buf, size_t len);
    jmp_buf jmp;
    char * dat;
    int l;
    int n;
    int i;
} js_buf;

#define JS_BUF(desc, reader, lbl) ({\
        js_buf * js = NEW((js_buf){.arg=desc, .read=reader, .dat=malloc(1024), .l=1024});\
        if(setjmp(js->jmp)) goto lbl;\
        js;\
    })

#define JS_NEST_HDL(js, jb, lbl) ({\
        memcpy(jb, js->jmp, sizeof(jb));\
        if(setjmp(js->jmp)) goto lbl;\
    })

#define JS_PROP_HDL(js, jb) do{\
        memcpy(js->jmp, jb, sizeof(jb));\
        longjmp(jb, 1);\
    }while(0)

#define JS_UNNEST_HDL(js, jb) do{\
        memcpy(js->jmp, jb, sizeof(jb));\
    }while(0)

#define JS_FD_BUF(fd, lbl) JS_BUF(fd, read, lbl)

int js_obj_start(js_buf * js);
int js_obj_cont(js_buf * js);
int js_arr_start(js_buf * js);
int js_arr_cont(js_buf * js);

#define JS_OBJ_ITER(js) \
    for(int _js_obj_iter=0; _js_obj_iter==0?js_obj_start(js):js_obj_cont(js); _js_obj_iter|=1)
#define JS_ARR_ITER(js) \
    for(int _js_arr_iter=0; _js_arr_iter==0?js_arr_start(js):js_arr_cont(js); _js_arr_iter|=1)

char js_char(js_buf * js);
int js_enum(js_buf *, js_trie *);
int js_enum_key(js_buf *, js_trie * );
int js_next(js_buf * js);

char * js_str(js_buf * js);
char * js_key(js_buf * js);

int js_int(js_buf * js);
double js_flt(js_buf * js);

int js_cmp(js_buf * js, char * str);
int js_bool(js_buf * js);

void js_skip(js_buf * js);

#define JS_SW(js) ({\
        EAT_SPC(js);\
        js->dat[js->i];\
    })

#define JS_SW_STR '"'
#define JS_SW_NUM '0':case '1':case '2':case '3':case '4':case '5':\
             case '6':case '7':case '8':case '9':case '-'
#define JS_SW_OBJ '{'
#define JS_SW_ARR '['
#define JS_SW_BOOL 't':case 'f'
#define JS_SW_NULL 'n'

#endif // JS_H
