#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include "types.h"
#include "premissas.h"
#define __QUEUE_IMPL__
#include "queue.h"

/* espera algum processo chegar (CPU está ociosa):
 * retorna true (!= 0) se a simulação vai continuar,
 * retorna false (0) se a simulação terminou */
b32 espera(Queue *qs, const u32 numq,
        Queue *qios, const u32 numio,
        PCB *pcbs, const u32 numpcb) {
    (void) qs;   (void) numq;
    (void) qios; (void) numio;
    (void) pcbs; (void) numpcb;
    // TODO: esperar o tempo passar,
    // ou seja, até chegar alguém (novo ou bloqueado)
    return 0;
}

typedef enum _Roda_Status {
    Roda_Done = 0, Roda_Timeout, Roda_IO
} Roda_Status;

typedef struct _Roda_Ret {
    Roda_Status status;
    IO_t io_type;
} Roda_Ret;

/* roda o processo:
 * retorna o que aconteceu com o processo:
 *  * Terminou;
 *  * Recebeu timeout; ou
 *  * Vai fazer IO.
 */
Roda_Ret roda(PID pid, PCB *pcb) {
    (void) pid; (void) pcb;
    Roda_Ret ret = {
        .status = Roda_Done
    };
    return ret;
}

void robinfeedback(Queue *qs, const u32 numq,
        Queue *qios, const u32 numio,
        PCB *pcbs, const u32 numpcb) {
    assert( numq > 2 );
    assert( numio > 2 );
    assert( numpcb <= MAX_PID );
    b32 is_done = 1;
    while ( is_done ) {
        u32 first_not_empty = 0;
        for ( ; first_not_empty < numq; first_not_empty++ ) {
            if ( !Queue_is_empty(qs + first_not_empty) )
                break;
        }
        if ( first_not_empty < numq ) {
            // Temos alguém para rodar
            const Queue_Ret qret =
                Queue_dequeue(qs + first_not_empty);
            assert( qret.err == Queue_Ok );
            PID pid = qret.data;
            assert( pid < numpcb );
            assert( pcbs[pid].priority == first_not_empty );
            pcbs[pid].status = p_running;

            const Roda_Ret roda_ret = roda(pid, pcbs + pid);

            // TODO: dar enqueue nos processos que chegaram (novos)
            // TODO: dar enqueue nos processos que chegaram (IO)

            switch ( roda_ret.status ) {
                case Roda_Done: {
                    pcbs[pid].status = p_done;
                } break;
                case Roda_Timeout: {
                    pcbs[pid].status = p_ready;
                    pcbs[pid].priority = numq - 1;
                    const Queue_Err qerr =
                        Queue_enqueue(qs + numq - 1, pid);
                    assert( qerr == Queue_Ok );
                } break;
                case Roda_IO: {
                    pcbs[pid].status = p_blocked;
                    const IO_t io = roda_ret.io_type;
                    assert( io < numio );
                    pcbs[pid].priority = priority_from_io(io, numio);
                    const Queue_Err qerr =
                        Queue_enqueue(qios + io, pid);
                    assert( qerr == Queue_Ok );
                } break;

            }
        } else {
            // Não temos ninguém para rodar
            is_done = espera(qs, numq, qios, numio, pcbs, numpcb);
        }
    }
}

int main() {
    printf("main!\n");
}
