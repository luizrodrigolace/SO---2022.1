#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include "types.h"
#include "premissas.h"
#define __QUEUE_IMPL__
#include "queue.h"

typedef enum _Roda_Status {
    Roda_Done = 0, Roda_NotDone, Roda_IO
} Roda_Status;

typedef struct _Roda_Ret {
    Roda_Status status;
    IO_t io_type;
} Roda_Ret;

/* retorna o que aconteceu com o processo
 * no tempo servido:
 *  * Terminou;
 *  * Recebeu timeout; ou
 *  * Vai fazer IO.
 */
Roda_Ret roda(PID pid, time served) {
    (void) pid; (void) served;
    Roda_Ret ret = {
        .status = Roda_Done,
        .io_type = IO_count,
    };
    return ret;
}

/* espera algum processo chegar (CPU está ociosa):
 * retorna true (!= 0) se a simulação vai continuar,
 * retorna false (0) se a simulação terminou */
b32 espera(Queue *qs, const u32 numq,
        Queue *qios,
        PCB *pcbs, const u32 numpcb) {
    (void) qs;   (void) numq;
    (void) qios;
    (void) pcbs; (void) numpcb;
    // TODO: esperar o tempo passar,
    // ou seja, até chegar alguém (novo ou bloqueado)
    return 0;
}

/* verifica se algum processo chegou no tempo t.
 * (similar a espera)
 * retorna true (!= 0) se a simulação vai continuar,
 * retorna false (0) se a simulação terminou
 * retorna no ppid o novo pid */
b32 new_process(time t, PID *pid) {
    (void) t; (void) pid;
    return 0;
}

void robinfeedback(Queue *qs, const u32 numq,
        Queue *qios, IODev *iodevs,
        PCB *pcbs, const u32 numpcb) {
    assert( numq == 2 );
    assert( numpcb <= MAX_PID );

    time curr_time = 0;
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

            time running = pcbs[pid].running_time;
            time slice;
            for ( slice = 0; slice < TIME_SLICE; slice++ ) {
                if ( pcbs[pid].status != p_running )
                    break;

                const Roda_Ret roda_ret = roda(pid, running + slice);

                switch ( roda_ret.status ) {
                    case Roda_Done: {
                        pcbs[pid].status = p_done;
                    } break;
                    case Roda_NotDone: {
                        // Nothing to do
                    } break;
                    case Roda_IO: {
                        pcbs[pid].status = p_blocked;
                        const IO_t io = roda_ret.io_type;
                        assert( io < IO_count );
                        pcbs[pid].priority = priority_from_io(io);
                        const Queue_Err qerr =
                            Queue_enqueue(qios + io, pid);
                        assert( qerr == Queue_Ok );
                    } break;
                }

                PID new_pid;
                while ( new_process(curr_time, &new_pid) ) {
                    assert( new_pid < numpcb );
                    PCB new_pcb = {
                        .status = p_ready,
                        .pid = new_pid,
                        .start_time = curr_time + slice,
                        .end_time = 0,
                        .running_time = 0,
                        .priority = 0,
                    };
                    pcbs[new_pid] = new_pcb;
                    const Queue_Err qerr =
                        Queue_enqueue(qs + new_pid, pid);
                    assert( qerr == Queue_Ok );
                }
                // TODO: dar enqueue nos processos que chegaram (IO)
            }

            pcbs[pid].running_time += slice;

            switch ( pcbs[pid].status ) {
                case p_done:
                    pcbs[pid].end_time = curr_time + slice;
                    break;
                case p_running:
                    pcbs[pid].status = p_ready;
                    pcbs[pid].priority = numq - 1;
                    const Queue_Err qerr =
                        Queue_enqueue(qs + numq - 1, pid);
                    assert( qerr == Queue_Ok );
                    break;
                case p_ready:
                    assert( 0 && "unreachable" );
                    break;
                case p_blocked:
                    // Nothing to do
                    break;
            }

            curr_time += slice;
        } else {
            // Não temos ninguém para rodar

            // TODO: dar enqueue nos processos que chegaram (novos)
            // TODO: dar enqueue nos processos que chegaram (IO)

            is_done = espera(qs, numq, qios, pcbs, numpcb);

            curr_time += 1;
        }
    }
}

int main() {
    printf("main!\n");
}
