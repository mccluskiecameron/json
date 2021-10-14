#include "json.h"

#define JS_READ(js, buf, len) js->read(js->arg, buf, len)

#define JS_ERR(js) do{\
        __builtin_trap();\
        longjmp(js->jmp, 1);\
    }while(0)

#define JS_RD(js) do{\
        if(js->i>=js->n)\
            js->n=JS_READ(js, js->dat,js->l), js->i=0;\
        if(js->n<=0)\
            JS_ERR(js);\
    }while(0)

#define JS_ADV(js) do{\
        js->i++;\
        JS_RD(js);\
    }while(0)

#define IS_DIG(c) ((c)>='0'&&(c)<='9')

#define IS_SPC(c) ((c)==' '||(c)=='\t'||(c)=='\n'||(c)=='\r')
#define EAT_SPC(js) do{\
        JS_RD(js);\
        while(IS_SPC(js->dat[js->i]))\
            JS_ADV(js);\
    }while(0)

js_trie * js_trie_init(void){
    return NULL;
}

typedef unsigned char u8;

struct js_trie {
    char * name;
    int val;
    struct js_trie ** tab;
}; 

js_trie * js_trie_ins(js_trie * tr, char * name, int val){
    if(!tr) return NEW((js_trie){.name=name, .val=val});
    if(tr->name){
        tr->tab = calloc(sizeof(js_trie), 256);
        tr->tab[(u8)*tr->name]=js_trie_ins(tr->tab[(u8)*tr->name], tr->name+1, tr->val);
        tr->name = NULL;
    }
    tr->tab[(u8)*name]=js_trie_ins(tr->tab[(u8)*name], name+1, val);
    return tr;
}

js_trie * js_trie_ins_all(js_trie * tr, int n, char ** names){
    for(int i=0; i<n; i++)
        tr = js_trie_ins(tr, names[i], i);
    return tr;
}

static int js_cmp(js_buf * js, char * str){
    while(*str){
        if(*str == js->dat[js->i]){
            str++;
            JS_ADV(js);
        }else{
            return 0;
        }
    }
    return 1;
}

int js_obj_start(js_buf * js){
    EAT_SPC(js);
    if(js->dat[js->i++] != '{')
        JS_ERR(js);
    EAT_SPC(js);
    if(js->dat[js->i] != '}')
        return 1;
    js->i++;
    return 0;
}

int js_obj_cont(js_buf * js){
    EAT_SPC(js);
    switch(js->dat[js->i++]){
        CASE '}': return 0;
        CASE ',': return 1;
        DEFAULT: JS_ERR(js);
    }
}

int js_arr_start(js_buf * js){
    EAT_SPC(js);
    if(js->dat[js->i++] != '[')
        JS_ERR(js);
    EAT_SPC(js);
    if(js->dat[js->i] != ']')
        return 1;
    js->i++;
    return 0;
}

int js_arr_cont(js_buf * js){
    EAT_SPC(js);
    switch(js->dat[js->i++]){
        CASE ']': return 0;
        CASE ',': return 1;
        DEFAULT: JS_ERR(js);
    }
}

#define JS_HEX(js, ch) ({\
        int val;\
        int c = ch;\
        if(IS_DIG(c)) val = c-'0';\
        else if('a'<=c&&'f'>=c) val = c-'a'+10;\
        else if('A'<=c&&'F'>=c) val = c-'A'+10;\
        else JS_ERR(js);\
        val;\
    })

static uint16_t js_short(js_buf * js){
    uint16_t res = JS_HEX(js, js->dat[js->i++]);
    JS_RD(js);
    res = res << 4 | JS_HEX(js, js->dat[js->i++]);
    JS_RD(js);
    res = res << 4 | JS_HEX(js, js->dat[js->i++]);
    JS_RD(js);
    res = res << 4 | JS_HEX(js, js->dat[js->i++]);
    return res;
}

static int js_u(js_buf * js){
    uint16_t s = js_short(js);
    if(s < 0xd800 || s >= 0xe000)
        return s;
    if(!js_cmp(js, "\\u"))
        JS_ERR(js);
    uint16_t e = js_short(js);
    if(s >> 10 != 0x36 || e >> 10 != 0x37)
        JS_ERR(js);
    return (((int)s & 0x3ff)<<10 | ((int)e & 0x3ff)) + 0x10000;
}

static u8 * js_to_utf8(int chr, u8 s[static 4]){
    u8 * t = s+4;
    if(chr <= 127){
        s[3] = chr;                                                     return s + 3;
    }
    for(int i=1; i<=4; i++){
        s[4-i] = 0x80 | ((chr>>(6*i-6)) & 0x3f);
        if(!((chr>>(6*i)) & ((-64)>>i))){
            s[3-i] = ((-128)>>i) | (chr>>(6*i));
            return s + 3 - i;
        }
    }
    //shouldn't happen
    abort();
}

int js_prepare(js_buf * js, int n){
    if(js->n-js->i >= n)
        return 1;
    memmove(js->dat, js->dat+js->i, js->n);
    js->i = 0;
    while(js->n < n){
        int k = js->read(js->arg, js->dat+js->n, js->l-js->n);
        if(k == 0) return 0;
        if(k < 0) JS_ERR(js);
        js->n += k;
    }
    return 1;
}

static char js_char(js_buf * js){
    char c = js->dat[js->i++];
    JS_RD(js);
    if(c == '\\'){
        char s = js->dat[js->i++];
        JS_RD(js);
        if(s == '"') return '"';
        if(s == '\\') return '\\';
        if(s == '/') return '/';
        if(s == 'b') return '\b';
        if(s == 'f') return '\f';
        if(s == 'n') return '\n';
        if(s == 'r') return '\r';
        if(s == 't') return '\t';
        if(s == 'u'){
            js_prepare(js, 10);
            int p = js_u(js);
            js->i = (char*)js_to_utf8(p, (u8*)(js->dat+js->i-4)) - js->dat;
            return js->dat[js->i++];
        }
        JS_ERR(js);
    }
    return c;
}

int js_enum(js_buf * js, js_trie * trie){
    EAT_SPC(js);
    if(js->dat[js->i++] != '"') JS_ERR(js);
    JS_RD(js);
    int val = -1;
    js_trie * tr = trie;
    int i=0;
    while(1){
        if(js->dat[js->i] == '"'){
            js->i++;
            if(tr && tr->name && tr->name[i]=='\0')
                val = tr->val;
            break;
        }
        char c = js_char(js);
        if(!c) JS_ERR(js);
        if(tr){
            if(tr->name){
                if(tr->name[i++] != c) tr=NULL;
            }else{
                tr=tr->tab[(u8)c];
            }
        }
    }
    return val;
}

int js_enum_key(js_buf * js, js_trie * trie){
    int val = js_enum(js, trie);
    EAT_SPC(js);
    if(js->dat[js->i++] != ':') JS_ERR(js);
    return val;
}

char * js_str(js_buf * js){
    EAT_SPC(js);
    if(js->dat[js->i++] != '"') 
        JS_ERR(js);
    JS_RD(js);
    int n = 1024;
    char * buf = malloc(n);
    int i=0;
    while(js->dat[js->i] != '"'){
        char c = js_char(js);
        buf[i++] = c;
        if(i==n-1){
            buf = realloc(buf, n*=2);
        }
    }
    js->i++;
    buf[i] = '\0';
    return buf;
}

int js_str_arr(js_buf * js, int n, char arr[static n]){
    EAT_SPC(js);
    if(js->dat[js->i] != '"') 
        JS_ERR(js);
    int i = 0;
    while(i < n){
        js->i++;
        JS_RD(js);
        if(js->dat[js->i] == '"'){
            js->i++;
            return i;
        }
        arr[i++] = js_char(js);
        js->i--; //we want to be able to overwrite this char
    }
    js->dat[js->i] = '"';
    return i;
}

char * js_key(js_buf * js){
    char * str = js_str(js);
    EAT_SPC(js);
    if(js->dat[js->i++] != ':') JS_ERR(js);
    return str;
}

int js_key_arr(js_buf * js, int n, char arr[static n]){
    int l = js_str_arr(js, n, arr);
    if(l < n){
        EAT_SPC(js);
        if(js->dat[js->i++] != ':') JS_ERR(js);
    }
    return l;
}

int js_int(js_buf * js){
    EAT_SPC(js);
    int neg=1, val=0;
    if(js->dat[js->i] == '-'){
        neg = -1;
        JS_ADV(js);
    }
    if(js->dat[js->i] == '0')
        return 0;
    if(!IS_DIG(js->dat[js->i]))
        JS_ERR(js);
    do{
        int dig = js->dat[js->i] - '0';
        if(__builtin_mul_overflow(val, 10, &val)
        || __builtin_add_overflow(val, dig, &val))
            JS_ERR(js);
        JS_ADV(js);
    }while(IS_DIG(js->dat[js->i]));
    return neg*val;
}

static double fastpow(double b, int exp){
    if(exp<0) return 1/fastpow(b, -exp);
    if(!exp) return 1;
    double sq = fastpow(b, exp/2);
    if(exp%2)
        return sq*sq*b;
    return sq*sq;
}

double js_flt(js_buf * js){
    EAT_SPC(js);
    int neg = 0;
    if(js->dat[js->i] == '-'){
        neg = 1;
        JS_ADV(js);
    }
    double a = js_int(js);
    if(neg) a *= -1;
    if(js->dat[js->i] == '.'){
        JS_ADV(js);
        double b = .1;
        if(neg) b *= -1;
        while(IS_DIG(js->dat[js->i])){
            a += b*(js->dat[js->i]-'0');
            b /= 10;
            JS_ADV(js);
        }
    }
    if((js->dat[js->i]&32) == 'e'){
        JS_ADV(js);
        int p = js_int(js);
        a *= fastpow(10, p);
    }
    return a;
}

int js_bool(js_buf * js){
    EAT_SPC(js);
    if(js_cmp(js, "true")) return 1;
    else if(js_cmp(js, "false")) return 0;
    JS_ERR(js);
}

void js_null(js_buf * js){
    EAT_SPC(js);
    if(js_cmp(js, "null")) return;
    JS_ERR(js);
}

static void js_str_skip(js_buf * js){
    EAT_SPC(js);
    if(js->dat[js->i] != '"') JS_ERR(js);
    JS_ADV(js);
    while(1){
        if(js_cmp(js, "\"")) return;
        if(js_cmp(js, "\\\"")) continue;
        JS_ADV(js);
    }
}

#define MIN(a, b) ((a)<(b)?(a):(b))

void js_skip(js_buf * js){
    size_t depth = 0;
    do{
        EAT_SPC(js);
        //printf(" *** %.*s\n", MIN(js->n-js->i, 10), js->dat+js->i);
        switch(js->dat[js->i]){
            CASE '"': js_str_skip(js);
            CASE JS_SW_NUM: js_flt(js);
            CASE '{':case '[':depth ++; js->i++;
            CASE ':':case ',':js->i++;
            CASE '}':case ']':depth --; js->i++;
            CASE 'f':case 't':js_bool(js);
            CASE 'n': js_null(js);
            DEFAULT: JS_ERR(js);
        }
    }while(depth);
}
