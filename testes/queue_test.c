#include <stdio.h>
#include <stdlib.h>

#include "../types.h"

#define __QUEUE_IMPL__
#include "../queue.h"

int main() {
    const u32 len = 20;
    Queue *q = (Queue *) malloc(Queue_sizeof(len));
    Queue_init(q, len);

    for ( u32 i = 0; i < len; i++ ) {
        u32 thing = 2 * ((u32) i);
        Queue_Err err = Queue_enqueue(q, thing);
        if ( err == Queue_Ok ) {
            printf("[%hhd] %u: Queue enqueue\n", i, thing);
        } else {
            printf("[%hhd] %u: Queue overflow\n", i, thing);
        }
    }
    Queue_debug(q, stdout);

    for ( u32 i = 0; i < len; i++ ) {
        Queue_Ret ret = Queue_dequeue(q);
        if ( ret.err ) {
            printf("[%hhd]: Queue underflow\n", i);
        } else {
            printf("[%hhd] %u: Queue dequeue\n", i, ret.data);
        }
    }
    Queue_debug(q, stdout);

    for ( u32 i = len; i < 3* (len/2); i++ ) {
        u32 thing = 2 * ((u32) i);
        if ( Queue_enqueue(q, thing) ) {
            printf("[%hhd] %u: Queue overflow\n", i, thing);
        } else {
            printf("[%hhd] %u: Queue enqueue\n", i, thing);
        }
    }
    Queue_debug(q, stdout);

    free(q);
    return 0;
}
