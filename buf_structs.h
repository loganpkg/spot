#ifndef BUF_STRUCTS_H
#define BUF_STRUCTS_H

#include <stddef.h>

struct ibuf {
    char *a;                    /* Memory */
    size_t i;                   /* Write index */
    size_t n;                   /* Allocated number of elements */
};

struct obuf {
    char *a;                    /* Memory */
    size_t i;                   /* Write index */
    size_t n;                   /* Allocated number of elements */
};

struct lbuf {
    long *a;                    /* Memory */
    size_t i;                   /* Write index */
    size_t n;                   /* Allocated number of elements */
};

struct sbuf {
    size_t *a;                  /* Memory */
    size_t i;                   /* Write index */
    size_t n;                   /* Allocated number of elements */
};

struct pbuf {
    void **a;                   /* Memory */
    size_t i;                   /* Write index */
    size_t n;                   /* Allocated number of elements */
};

#endif
