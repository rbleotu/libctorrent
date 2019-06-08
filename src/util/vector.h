#pragma once

#define Vector(N) Vector##N
#define VECTOR() {.cap = 0, .n = 0, .data= NULL}

#define vector_destroy(v) \
    ((v)->n = 0, (v)->cap = 0, free((v)->data), (v)->data = NULL)

#define VECTOR_FOREACH(v, e, type) \
    for (type *e = (v)->data; e < (v)->data + (v)->n; ++e)

#define vector_pushback(v, x)                                                        \
    ((v)->n < (v)->cap ?                                                             \
        ((v)->data[(v)->n] = (x), ++(v)->n) :                                        \
        (!resize((void **)&(v)->data, next_cap((v)->cap), sizeof((v)->data[0])) ?    \
            ((v)->cap = next_cap((v)->cap), (v)->data[(v)->n] = (x), ++(v)->n) : -1))

static inline size_t
next_cap(size_t cap)
{
    return cap ? (cap * 1.5) : 8;
}

#define vector_resizex(N)   CONCAT(CONCAT(vector_, N), _resize)
#define vector_pushbackx(N) CONCAT(CONCAT(vector_, N), _pushback)

#define VECTOR_DEFINE(T, N)                                           \
    typedef struct CONCAT(vector_,N) Vector##N;                       \
                                                                      \
    struct CONCAT(vector_,N) {                                        \
        size_t cap, n;                                                \
        T *data;                                                      \
    };                                                                \
                                                                      \
                                                                      \
    static int                                                        \
    vector_resizex(N)(Vector(N) *v, size_t newcap)                    \
    {                                                                 \
        void * const tmp = realloc(v->data, sizeof(v->data) * newcap);\
        if (!tmp)                                                     \
            return -1;                                                \
        v->data = tmp, v->cap = newcap;                               \
        return 0;                                                     \
    }                                                                 \
                                                                      \
                                                                      \
    static inline int                                                 \
    vector_pushbackx(N)(Vector##N *v, T x)                            \
    {                                                                 \
        if (v->n >= v->cap) {                                         \
            if (vector_resizex(N)(v, next_cap(v->cap)))               \
                return -1;                                            \
        }                                                             \
        v->data[v->n++] = x;                                          \
        return 0;                                                     \
    }

static inline int
resize(void **buf, size_t n, size_t elemsz)
{
    void * const tmp =  realloc(*buf, n * elemsz);
    if (!tmp)
        return -1;
    *buf = tmp;
    return 0;
}
