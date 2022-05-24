#ifndef QUEUE_LIB
#define QUEUE_LIB

#include <stdio.h>
#include "types.h"

// Status da fila após uma operação
typedef enum _Queue_Err {
    Queue_Ok = 0, Queue_Empty, Queue_Full
} Queue_Err;

// Retorno quando estamos retirando ou vendo o elemento mais antigo da fila 
typedef struct _Queue_Ret {
    Queue_Err err;
    u32 data;
} Queue_Ret;

// Estrutura da fila
typedef struct _Queue {
    u32 len;
    u32 fst;
    u32 lst;
    u32 data[];
} Queue;

size_t Queue_sizeof(u32);
void Queue_init(Queue *, u32);
b32 Queue_is_empty(const Queue *);
b32 Queue_is_full(const Queue *);
u32 Queue_size(const Queue *);
Queue_Err Queue_enqueue(Queue *, u32);
Queue_Ret Queue_dequeue(Queue *);
Queue_Ret Queue_peek(const Queue *);
void Queue_debug(const Queue *, FILE *);

#endif // QUEUE_LIB

#ifdef __QUEUE_IMPL__
#ifndef QUEUE_IMPLEMENTATION
#define QUEUE_IMPLEMENTATION

// Retorna o tamanho para alocar len elementos
size_t Queue_sizeof(u32 len) {
    return sizeof(Queue) + sizeof(((Queue *)0)->data[0])*(len+1);
}

// Inicializa a fila com len elementos
void Queue_init(Queue *q, u32 len) {
    q->len = len + 1;
    q->fst = 0;
    q->lst = 0;
}

// Analisa se a fila está vazia
inline b32 Queue_is_empty(const Queue *q) {
    return q->fst == q->lst;
}

// Analisa se a fila está cheia
inline b32 Queue_is_full(const Queue *q) {
    return q->fst == (q->lst + 1) % q->len;
}

// Analisando quantos elementos existem na fila
inline u32 Queue_size(const Queue *q) {
    return (q->lst - q->fst + q->len) % q->len;
}

// Colocando um elemento na fila
Queue_Err Queue_enqueue(Queue *q, u32 thing) {
    const u32 len = q->len, lst = q->lst;
    if ( Queue_is_full(q) ) {
        return Queue_Full;
    } else {
        q->data[lst] = thing;
        q->lst = (lst + 1) % len;
        return Queue_Ok;
    }
}

// Retirando um elemento da fila
Queue_Ret Queue_dequeue(Queue *q) {
    Queue_Ret ret = { .err = Queue_Ok, .data = 0 };
    const u32 len = q->len, fst = q->fst;
    if ( Queue_is_empty(q) ) {
        ret.err = Queue_Empty;
        return ret;
    } else {
        ret.data = q->data[fst];
        q->fst = (fst + 1) % len;
        return ret;
    }
}

// Analisa o elemento mais antigo da fila
Queue_Ret Queue_peek(const Queue *q) {
    Queue_Ret ret = { .err = Queue_Ok, .data = 0 };
    const u32 fst = q->fst;
    if ( Queue_is_empty(q) ) {
        ret.err = Queue_Empty;
        return ret;
    } else {
        ret.data = q->data[fst];
        return ret;
    }
}

#ifndef QUEUE_DEBUG_COL
#define QUEUE_DEBUG_COL             15
#endif // QUEUE_DEBUG_COL

#ifndef QUEUE_DEBUG_FST_PREFIX
#define QUEUE_DEBUG_FST_PREFIX      "["
#endif // QUEUE_DEBUG_FST_PREFIX

#ifndef QUEUE_DEBUG_NO_FST_PREFIX
#define QUEUE_DEBUG_NO_FST_PREFIX   ""
#endif // QUEUE_DEBUG_NO_FST_PREFIX

#ifndef QUEUE_DEBUG_LST_PREFIX
#define QUEUE_DEBUG_LST_PREFIX      "]"
#endif // QUEUE_DEBUG_LST_PREFIX

#ifndef QUEUE_DEBUG_NO_LST_PREFIX
#define QUEUE_DEBUG_NO_LST_PREFIX   ""
#endif // QUEUE_DEBUG_NO_LST_PREFIX

// Printa a fila
void Queue_debug(const Queue *q, FILE* f) {
    const u32 len = q->len, fst = q->fst, lst = q->lst;
    fprintf(f, "DEBUG: Queue {\n");
    fprintf(f, "    .len = %u,\n", len);
    fprintf(f, "    .fst = %u,\n", fst);
    fprintf(f, "    .lst = %u,\n", lst);
    fprintf(f, "    (is_empty) = %u,\n", Queue_is_empty(q));
    fprintf(f, "    (is_full)  = %u,\n", Queue_is_full(q));
    fprintf(f, "    (size)     = %u,\n", Queue_size(q));
    fprintf(f, "    .data = {");
    for ( u32 i = 0; i < len; i++ ) {
        u32 thing = q->data[i];
        char *prefix = (i == fst ?
                QUEUE_DEBUG_FST_PREFIX :
                QUEUE_DEBUG_NO_FST_PREFIX);
        char *sufix = (i == lst ?
                QUEUE_DEBUG_LST_PREFIX :
                QUEUE_DEBUG_NO_LST_PREFIX);

        if ( i % QUEUE_DEBUG_COL == 0 ) {
            fprintf(f, "\n        %s%s%u", prefix, sufix, thing);
        } else {
            fprintf(f, ", %s%s%u", prefix, sufix, thing);
        }
    }
    fprintf(f, "\n    }\n");
    fprintf(f, "}\n");
}

#endif // QUEUE_IMPLEMENTATION
#endif // QUEUE_IMPL
