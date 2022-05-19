#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include "types.h"
#include "premissas.h"
#define __QUEUE_IMPL__
#include "queue.h"

// Tabelas de entrada
static CTable *cheat_table;
static CIO *cheat_io_table;

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
Roda_Ret roda(const PID pid, const time served) {
    Roda_Ret ret = {
        .status = Roda_NotDone,
        .io_type = IO_count,
    };
    const CLine cheat_line = cheat_table->lines[pid];
    assert( cheat_line.service > 0 );
    assert( served <= cheat_line.service );
    if ( served == cheat_line.service ) {
        ret.status = Roda_Done;
        return ret;
    } else {
        u16 io_i = cheat_line.io_start;
        for ( u16 i = 0; i < cheat_line.io_count; i++ ) {
            const CIO cheat_io = cheat_io_table[io_i + i];
            if ( served > cheat_io.begin ) {
                break;
            } else if ( served == cheat_io.begin ) {
                assert( cheat_io.io_type < IO_count );
                ret.status = Roda_IO;
                ret.io_type = cheat_io.io_type;
                break;
            }
        }
        return ret;
    }
}

/* espera algum processo chegar (CPU está ociosa):
 * retorna true (!= 0) se a simulação vai continuar,
 * retorna false (0) se a simulação terminou */
b32 espera(const time t, const PCB *pcbs, const u32 numpcb) {
    assert( cheat_table->len == numpcb );
    const PID last_pid = cheat_table->len - 1;
    const CLine last_pid_line = cheat_table->lines[last_pid];
    const time last_start = last_pid_line.start;
    const time last_service = last_pid_line.service;
    if ( t < last_start + last_service ) {
        return 1;
    } else {
        static PID last_pid_not_done = 0;
        for ( ; last_pid_not_done < numpcb; last_pid_not_done++ ) {
            if ( pcbs[last_pid_not_done].status != p_done )
                break;
        }
        return pcbs[last_pid_not_done].status != p_done;
    }
}

/* verifica se algum processo chegou no tempo t.
 * retorna true (!= 0) se chegou um novo processo,
 * retorna false (0) se não chegou um novo processo.
 * retorna no pid o pid do processo que acabou de chegar */
b32 new_process(const time t, PID *pid) {
    static PID last_pid_not_started = 0;
    if ( last_pid_not_started < cheat_table->len ) {
        const time next_start =
            cheat_table->lines[last_pid_not_started].start;
        if ( t == next_start ) {
            *pid = last_pid_not_started;
            last_pid_not_started += 1;
            return 1;
        } else {
            assert( t < next_start );
            return 0;
        }
    } else {
        return 0;
    }
}

static inline
void handle_new_processes(const time curr_time,
        Queue **qs, const u32 numq,
        PCB *pcbs, const u32 numpcb) {
    assert( numq > 0 );
    PID new_pid;
    while ( new_process(curr_time, &new_pid) ) {
        assert( new_pid < numpcb );
        PCB new_pcb = {
            .status = p_ready,
            .pid = new_pid,
            .start_time = curr_time,
            .end_time = 0,
            .running_time = 0,
            .priority = 0,
        };
        pcbs[new_pid] = new_pcb;
        const Queue_Err qerr =
            Queue_enqueue(qs[0], new_pid);
        assert( qerr == Queue_Ok );
    }
}

static inline
void handle_blocked_processes(const time curr_time,
        Queue **qs, const u32 numq,
        Queue **qios, IODev *iodevs,
        PCB *pcbs, const u32 numpcb) {
    for ( u32 i = 0; i < IO_count; i++ ) {
        while ( 1 ) {
            const Queue_Ret qret =
                Queue_peek(iodevs[i].q);
            if( qret.err == Queue_Ok ) {
                const u32 idx = qret.data;
                assert( idx < io_dev_count(i) );
                const IO_Ctx ctx = iodevs[i].ctx[idx];
                const PID pid = ctx.pid;
                assert( pid < numpcb );
                assert( curr_time <= ctx.finish_IO );
                if ( ctx.finish_IO == curr_time ) {
                    const Queue_Ret qret_clone =
                        Queue_dequeue(iodevs[i].q);
                    assert( qret.err == qret_clone.err );
                    assert( qret.data == qret_clone.data );
                    pcbs[pid].status = p_ready;
                    const u32 queue_idx = priority_from_io(i);
                    assert( queue_idx <= numq );
                    const Queue_Err qerr =
                        Queue_enqueue(qs[queue_idx], pid );
                    assert( qerr == Queue_Ok );
                } else {
                    break;
                }
            } else {
                assert( qret.err == Queue_Empty );
                break;
            }
        }
    }

    for ( u32 i = 0; i < IO_count; i++ ) {
        const u32 dev_cnt = io_dev_count(i);
        const u32 time_to_finish = io_time(i);
        while ( Queue_size(iodevs[i].q) < dev_cnt ) {
            const Queue_Ret qret =
                Queue_dequeue(qios[i]);
            if( qret.err == Queue_Ok ) {
                assert( qret.data < numpcb );
                const IO_Ctx dev_ctx = {
                    .pid = qret.data,
                    .finish_IO = time_to_finish,
                };
                iodevs[i].ctx[iodevs[i].next_idx] = dev_ctx;
                const Queue_Err qerr =
                    Queue_enqueue(iodevs[i].q, iodevs[i].next_idx);
                assert( qerr == Queue_Ok );
                iodevs[i].next_idx = iodevs[i].next_idx + 1 % dev_cnt;
            } else {
                assert( qret.err == Queue_Empty );
                break;
            }
        }
    }
}

void robinfeedback(Queue **qs, const u32 numq,
        Queue **qios, IODev *iodevs,
        PCB *pcbs, const u32 numpcb) {
    assert( numq == 2 );
    assert( numpcb <= MAX_PID );

    time curr_time = 0;
    b32 is_done = 1;
    while ( is_done ) {

        handle_new_processes(curr_time,
                qs, numq, pcbs, numpcb);

        handle_blocked_processes(curr_time,
                qs, numq, qios, iodevs, pcbs, numpcb);

        u32 first_not_empty = 0;
        for ( ; first_not_empty < numq; first_not_empty++ ) {
            if ( !Queue_is_empty(qs[first_not_empty]) )
                break;
        }
        if ( first_not_empty < numq ) {
            // Temos alguém para rodar
            const Queue_Ret qret =
                Queue_dequeue(qs[first_not_empty]);
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
                            Queue_enqueue(qios[io], pid);
                        assert( qerr == Queue_Ok );
                    } break;
                    default:
                        assert( 0 && "unreachable" );
                        break;
                }

                handle_new_processes(curr_time + slice,
                        qs, numq, pcbs, numpcb);

                handle_blocked_processes(curr_time + slice,
                        qs, numq, qios, iodevs, pcbs, numpcb);
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
                        Queue_enqueue(qs[numq - 1], pid);
                    assert( qerr == Queue_Ok );
                    break;
                case p_ready:
                    assert( 0 && "unreachable" );
                    break;
                case p_blocked:
                    // Nothing to do
                    break;
                default:
                    assert( 0 && "unreachable" );
                    break;
            }

            curr_time += slice;
        } else {
            // Não temos ninguém para rodar

            is_done = espera(curr_time, pcbs, numpcb);

            curr_time += 1;
        }
    }
}

void* alloc_or_exit(const char *prefix, const size_t size) {
    void *ptr = malloc(size);
    if ( !ptr ) {
        fprintf(stderr, "%s: Sem memória para alocar (%lu bytes)",
                prefix, size);
        exit(1);
    }
    return ptr;
}

void read_input(u32 *numpcb, u32 *numios) {
    (void) numpcb;
    (void) numios;
    printf("reading input... (but not!)\n");

    // Tabela hard coded
    const PID len = 2;
    const u32 nios = 3;
    cheat_table = (CTable *) alloc_or_exit("cheat_table",
            sizeof(CTable) + len*sizeof(CLine));
    cheat_table->len = len;
    CLine p0 = {
        .start = 0,
        .service = 2*TIME_SLICE + TIME_SLICE/2,
        .io_start = 0,
        .io_count = 2,
    },
    p1 = {
        .start = 0,
        .service = 2*TIME_SLICE - TIME_SLICE/2,
        .io_start = 2,
        .io_count = 1,
    };
    cheat_table->lines[0] = p0;
    cheat_table->lines[1] = p1;

    cheat_io_table = (CIO *) alloc_or_exit("cheat_io)table",
            nios*sizeof(CIO));
    CIO p0_io0 = {
        .io_type = IO_Tape,
        .begin = 3,
    },
    p0_io1 = {
        .io_type = IO_Disk,
        .begin = 7,
    },
    p1_io0 = {
        .io_type = IO_Printer,
        .begin = 5,
    };
    cheat_io_table[0] = p0_io0;
    cheat_io_table[1] = p0_io1;
    cheat_io_table[2] = p1_io0;

    *numpcb = len;
    *numios = nios;
}

void unread_input() {
    free(cheat_table);
    free(cheat_io_table);
}

int main() {

    u32 numpcb, numios;
    read_input(&numpcb, &numios);

    const u32 numq = 2;
    const u32 queue_size = Queue_sizeof(numpcb);

    Queue *qs[2];
    for ( u32 i = 0; i < numq; i++ ) {
        qs[i] = (Queue *) alloc_or_exit("qs_i", queue_size);
        Queue_init(qs[i], numpcb);
    }

    Queue *qios[IO_count];
    IODev iodevs[IO_count] = { 0 };
    for ( u32 i = 0; i < IO_count; i++ ) {
        qios[i] = (Queue *) alloc_or_exit("qios_i", queue_size);
        Queue_init(qios[i], queue_size);
        const u32 dev_cnt = io_dev_count(i);
        iodevs[i].q = (Queue *) alloc_or_exit("idev.q_i", dev_cnt);
        Queue_init(iodevs[i].q, numpcb);
        iodevs[i].ctx = (IO_Ctx *) alloc_or_exit("idev.ctx_i", dev_cnt);
    }
    PCB *pcbs = (PCB *) alloc_or_exit("pcbs", sizeof(*pcbs)*numpcb);

    robinfeedback(qs, numq, qios, iodevs, pcbs, numpcb);

    printf("main!\n");

    free(pcbs);
    for ( u32 i = 0; i < IO_count; i++ ) {
        assert( Queue_is_empty(iodevs[i].q) );
        free(iodevs[i].q);
        free(iodevs[i].ctx);
        assert( Queue_is_empty(qios[i]) );
        free(qios[i]);
    }

    for ( u32 i = 0; i < numq; i++ ) {
        assert( Queue_is_empty(qs[i]) );
        free(qs[i]);
    }

    unread_input();
}
