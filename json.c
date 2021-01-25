#include "json.h"

#define JS_READ(js, buf, len) js->read(js->arg, buf, len)

#define JS_ERR(js) longjmp(js->jmp, 1)

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

#define IS_SPC(c) ((c)==' '||(c)=='\n'||(c)=='\r')
#define EAT_SPC(js) do{\
        JS_RD(js);\
        while(IS_SPC(js->dat[js->i]))\
            JS_ADV(js);\
    }while(0)

js_trie * js_trie_init(void){
    return NULL;
}

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

int js_next(js_buf * js){
    EAT_SPC(js);
    char c = js->dat[js->i++];
    if(c!=','&&c!='}'&&c!=']') JS_ERR(js);
    return c==',';
}

#define JS_HEX(js, c) ({\
        int val;\
        if(IS_DIG(c)) val = c-'0';\
        else if('a'<=c&&'f'>=c) val = c-'a'+10;\
        else if('A'<=c&&'F'>=c) val = c-'A'+10;\
        else JS_ERR(js);\
        val;\
    })

char js_char(js_buf * js){
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
        JS_ERR(js); // no \u support
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
    while(1){
        if(js->dat[js->i] == '"'){
            buf[i] = '\0';
            js->i++;
            break;
        }
        char c = js_char(js);
        buf[i++] = c;
        if(i==n-1){
            buf = realloc(buf, n*=2);
        }
    }
    return buf;
}

char * js_key(js_buf * js){
    char * str = js_str(js);
    EAT_SPC(js);
    if(js->dat[js->i++] != ':') JS_ERR(js);
    return str;
}

int js_int(js_buf * js){
    EAT_SPC(js);
    int neg=0, val=0;
    if(js->dat[js->i] == '-'){
        neg = 1;
        JS_ADV(js);
    }
    if(!IS_DIG(js->dat[js->i]))
        JS_ERR(js);
    do{
        val *= 10;
        val += js->dat[js->i] - '0';
        JS_ADV(js);
    }while(IS_DIG(js->dat[js->i]));
    return neg?-val:val;
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
    if(js->dat[js->i] == 'e' || js->dat[js->i] == 'E'){
        JS_ADV(js);
        int p = js_int(js);
        a *= fastpow(10, p);
    }
    return a;
}

int js_cmp(js_buf * js, char * str){
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

int js_bool(js_buf * js){
    EAT_SPC(js);
    if(js_cmp(js, "true")) return 1;
    else if(js_cmp(js, "false")) return 0;
    JS_ERR(js);
}

#define JS_NUM(js) do{\
        EAT_SPC(js);\
        while(strchr("-0123456789.e+",js->dat[js->i])){\
            JS_ADV(js);\
        }\
    }while(0)

void js_skip(js_buf * js){
    int depth = 0;
    do{
        EAT_SPC(js);
        switch(js->dat[js->i]){
            CASE '"': js_enum(js, NULL);
            CASE JS_SW_NUM: js_flt(js);
            CASE '{':case '[':depth ++; js->i++;
            CASE ':':case ',':js->i++;
            CASE '}':case ']':depth --; js->i++;
            CASE 'f':case 't':js_bool(js);
            CASE 'n':if(!js_cmp(js, "null")) JS_ERR(js);
            DEFAULT: JS_ERR(js);
        }
    }while(depth);
}
