#include <stdlib.h>

#define NEW(...) ({\
    typeof(__VA_ARGS__) * _pt = malloc(sizeof(__VA_ARGS__));\
    *_pt = __VA_ARGS__;\
    _pt;\
})
