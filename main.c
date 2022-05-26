#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include "types.h"
#include "premissas.h"
#define __QUEUE_IMPL__
#include "queue.h"

#include "log.c"

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

/* Verifica se há processos novos a serem tratados
 * e os coloca na fila, caso existam.
 * Recebe:
 *  * curr_time: tempo atual;
 *  * qs: um vetor de filas do cpu;
 *  * numq: quantidade de filas do cpu;
 *  * pcbs: vetor de contextos de software;
 *  * numpcb: quantidade de contextos de software.
 */
static inline
void handle_new_processes(const time curr_time,
        Queue **qs, const u32 numq,
        PCB *pcbs, const u32 numpcb,
        const Log_Ctx log) {

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
        log_new_pid(log, curr_time, new_pid);
    }
}

/* Verifica se há processos bloqueados que acabaram de fazer I/O,
 * e os coloca na fila, caso existam.
 * Recebe:
 *  * curr_time: tempo atual;
 *  * qs: um vetor de filas do cpu;
 *  * numq: quantidade de filas do cpu;
 *  * qios: vetor de filas de i/o (tamanho IO_count);
 *  * iodevs: vetor de dispositivos (tamanho IO_count);
 *  * pcbs: vetor de contextos de software;
 *  * numpcb: quantidade de contextos de software.
TIRANDO DA "FILA DA IMPRESSORA QUE EXECUTA" E COLOCANDO NA FILA DA CPU
 */
static inline
void handle_blocked_processes(const time curr_time,
        Queue **qs, const u32 numq,
        Queue **qios, IODev *iodevs,
        PCB *pcbs, const u32 numpcb,
        const Log_Ctx log) {

    // loop para cada tipo de i/o
    for ( u32 i = 0; i < IO_count; i++ ) {
        while ( 1 ) {

            // Coletando informações sobre o dispositivo de I/O
            // que está executando a mais tempo
            const Queue_Ret qret =
                Queue_peek(iodevs[i].q);

            // Se existe algum dispositivo I/O ativo
            if( qret.err == Queue_Ok ) {
                const u32 idx = qret.data;
                assert( idx < io_dev_count(i) );
                // Coletando o contexto do dispotitivo selecionado
                // que está executando a mais tempo
                const IO_Ctx ctx = iodevs[i].ctx[idx];
                const PID pid = ctx.pid;
                assert( pid < numpcb );
                assert( curr_time <= ctx.finish_IO );

                // Se o dispositivo que está executando a mais tempo
                // já parou de executar
                if ( ctx.finish_IO == curr_time ) {
                    // Tirando o dispositivo
                    // que está rodando a mais tempo
                    const Queue_Ret qret_clone =
                        Queue_dequeue(iodevs[i].q);
                    assert( qret.err == qret_clone.err );
                    assert( qret.data == qret_clone.data );
                    pcbs[pid].status = p_ready;
                    pcbs[pid].io_time +=
                        curr_time - pcbs[pid].io_start;
                    pcbs[pid].io_start = 0;
                    const u32 queue_idx = priority_from_io(i);
                    assert( queue_idx <= numq );
                    const Queue_Err qerr =
                        Queue_enqueue(qs[queue_idx], pid );
                    assert( qerr == Queue_Ok );
                    log_from_IO(log, curr_time, pid, i);

                } else {
                    break;
                }
            } else {
                assert( qret.err == Queue_Empty );
                break;
            }
        }
    }

    // Tirando da fila de espera do I/O e mandando executar
    // ("COLOCANDO NA FILA DO DISPOSITIVO EXECUTANDO")
    for ( u32 i = 0; i < IO_count; i++ ) {
        const u32 dev_cnt = io_dev_count(i);
        // Tempo de duração do I/O
        const time finish_time = curr_time + io_time(i);
        // Verificando se existe dispositivos disponiveis
        while ( Queue_size(iodevs[i].q) < dev_cnt ) {
            // Fila de I/O
            const Queue_Ret qret =
                Queue_dequeue(qios[i]);
            // Tirando algum processo da fila de I/O
            if( qret.err == Queue_Ok ) {
                assert( qret.data < numpcb );
                const IO_Ctx dev_ctx = {
                    .pid = qret.data,
                    .finish_IO = finish_time,
                };
                // Selecionando um dispositivo e
                // alterando seu contexto
                iodevs[i].ctx[iodevs[i].next_idx] = dev_ctx;
                // Colocando este dispositivo na fila de impressoras
                // ("AQUI ESTAMOS SOMENTE REGISTRANDO
                // QUE ELA ESTÁ ATIVA MAS ELA FICOU ATIVA
                // QUANDO MUDAMOS SEU CTX")
                const Queue_Err qerr =
                    Queue_enqueue(iodevs[i].q, iodevs[i].next_idx);
                assert( qerr == Queue_Ok );
                log_start_IO(log, curr_time, qret.data, i);
                // Selecionando a próxima impressora livre
                iodevs[i].next_idx = iodevs[i].next_idx + 1 % dev_cnt;
            } else {
                assert( qret.err == Queue_Empty );
                break;
            }
        }
    }
}

/* Recebe:
 *  * qs: um vetor de filas do cpu;
 *  * numq: quantidade de filas do cpu;
 *  * qios: vetor de filas de i/o (tamanho IO_count);
 *  * iodevs: vetor de dispositivos (tamanho IO_count);
 *  * pcbs: vetor de contextos de software;
 *  * numpcb: quantidade de contextos de software;
 *  * log: contexto para fazer log.
 */
void robinfeedback(Queue **qs, const u32 numq,
        Queue **qios, IODev *iodevs,
        PCB *pcbs, const u32 numpcb,
        const Log_Ctx log) {
    // fazendo verificação quant de filas de cpu e
    // contextos de software
    assert( numq == 2 );
    assert( numpcb <= MAX_PID );

    time curr_time = 0;
    b32 is_not_done = 1;

    // loop até finalizar todos os processos.
    while ( is_not_done ) {

        // Verificando se há processos novos
        handle_new_processes(curr_time,
                qs, numq, pcbs, numpcb, log);

        handle_blocked_processes(curr_time,
                qs, numq, qios, iodevs, pcbs, numpcb, log);

        u32 first_not_empty = 0;

        // Se a fila da CPU não estiver vazia,
        // vamos selecionar um processo para executar
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
            log_enter_cpu(log, curr_time, pid);
            // Pegando o tempo que ele já rodou
            time running = pcbs[pid].running_time;
            time slice;
            // O processo vai rodar até o seu time slice acabar
            for ( slice = 0; slice < TIME_SLICE; slice++ ) {
                if ( pcbs[pid].status != p_running )
                    break;
                // Verificando se o processo vai realizar
                // algum I/O no tempo atual
                const Roda_Ret roda_ret = roda(pid, running + slice);
               // Analisando o que aconteceu com o preocesso
               // após ter executado uma unidade de tempo
                switch ( roda_ret.status ) {
                    case Roda_Done: {
                        pcbs[pid].status = p_done;
                        log_leave_cpu(log, curr_time + slice, pid);
                        log_done(log, curr_time + slice, pid);
                    } break;
                    case Roda_NotDone: {
                        // Nothing to do
                    } break;
                    case Roda_IO: {
                        // Ficou com estado bloqueado
                        assert( pcbs[pid].io_start == 0 );
                        pcbs[pid].status = p_blocked;
                        pcbs[pid].io_start = curr_time + slice;
                        log_leave_cpu(log, curr_time + slice, pid);
                        const IO_t io = roda_ret.io_type;
                        assert( io < IO_count );
                        // Prioridade do retorno do I/O
                        pcbs[pid].priority = priority_from_io(io);
                        // Colocando o processo na
                        // fila de espera do I/O
                        const Queue_Err qerr =
                            Queue_enqueue(qios[io], pid);
                        assert( qerr == Queue_Ok );
                        log_ask_IO(log, curr_time + slice, pid, io);
                    } break;
                    default:
                        assert( 0 && "unreachable" );
                        break;
                }

                // Analisando se algum processo foi criado
                handle_new_processes(curr_time + slice,
                        qs, numq, pcbs, numpcb, log);

                // Analisando se algum processo
                // que estava fazendo I/O terminou
                handle_blocked_processes(curr_time + slice,
                        qs, numq, qios, iodevs, pcbs, numpcb, log);
            }
            // Somando o tempo "rodado" no
            // tempo total de execução do processo
            pcbs[pid].running_time += slice;

            // Lendo o status do processo
            switch ( pcbs[pid].status ) {
                case p_done:
                    pcbs[pid].running_time -= 1;
                    pcbs[pid].end_time = curr_time + slice - 1;
                    break;
                case p_running:
                    pcbs[pid].status = p_ready;
                    log_leave_cpu(log, curr_time + slice - 1, pid);
                    // Processo que sofreu preempção
                    // retornando na fila de baixa prioridade
                    pcbs[pid].priority = numq - 1;
                    // Colocando o processo não finalizado na fila
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
            // Aumentando o tempo
            curr_time += slice;
        } else {
            // Não temos ninguém para rodar

            is_not_done = espera(curr_time, pcbs, numpcb);

            curr_time += 1;
        }
    }
}

// Realocando memoria
void* realloc_or_exit(const char *prefix, void *ptr,
        const size_t size) {
    void *new_ptr = realloc(ptr, size);
    if ( !new_ptr ) {
        fprintf(stderr, "%s: Sem memória para realocar (%lu bytes)",
                prefix, size);
        exit(1);
    }
    return new_ptr;
}

// Alocando memoria
void* alloc_or_exit(const char *prefix, const size_t size) {
    void *ptr = malloc(size);
    if ( !ptr ) {
        fprintf(stderr, "%s: Sem memória para alocar (%lu bytes)",
                prefix, size);
        exit(1);
    }
    return ptr;
}

void print_input(FILE *f, const PID pid, const u32 cap,
        const u32 nios, const u32 io_cap) {
    assert( pid <= cap );
    assert( nios <= io_cap );
    const CTable *ct = cheat_table;
    const CIO *cio = cheat_io_table;
    fprintf(f, "pid-cap = %hhu-%u, nios-io_cap = %u-%u\n",
            pid, cap, nios, io_cap);
    for ( u32 i = 0; i < pid; i++ ) {
        CLine line = ct->lines[i];
        fprintf(f, "%hhu: %hu %hu [%hu %hu]",
                i, line.start, line.service,
                line.io_start, line.io_count);
        for ( u32 j = 0; j < line.io_count; j++ ) {
            const u32 idx = line.io_start + j;
            fprintf(f, " (%u)%hu-%c",
                    idx,
                    cio[idx].begin, "DTP"[cio[idx].io_type]);
        }
        fprintf(f, "\n");
    }
    fprintf(f, "\n");
}

void read_input(u32 *numpcb, u32 *numios) {
    u32 cap = 4;
    u32 io_cap = 4;
    PID pid = 0;
    u32 nios = 0;

    cheat_table = (CTable *) alloc_or_exit("cheat_table",
            sizeof(CTable) + cap*sizeof(CLine));
    cheat_io_table = (CIO *) alloc_or_exit("cheat_io)table",
            io_cap*sizeof(CIO));

    enum State {
        begin_line = 0,
        reading_start, start_read,
        reading_service, service_read,
        reading_io_begin, io_begin_read,
        io_type_read,
    } state = begin_line;
    i32 c;
    u32 acc = 0;
    while ( (c = getchar()) != EOF ) {
        // ignoring '\r'
        if ( c == '\r' ) continue;

        switch (state) {
            case begin_line:
                assert( acc == 0 );
                if ( '0' <= c && c <= '9' ) {
                    acc = c - '0';
                    state = reading_start;
                    if ( pid >= cap ) {
                        cap *= 2;
                        cheat_table = (CTable *) realloc_or_exit(
                                "cheat_table",
                                cheat_table,
                                sizeof(CTable) + cap*sizeof(CLine));
                    }
                } else if ( c == ' ' || c == '\t'
                        || c == '\n' ) {
                    // Nothing to do
                } else {
                    fprintf(stderr, "[warn] read_input (%d): "
                            "unhandled char: '%c' (%d)\n",
                            state, c, c);
                }
                break;
            case reading_start:
            case reading_service:
                if ( '0' <= c && c <= '9' ) {
                    acc = 10*acc + c - '0';
                } else if ( c == ' ' || c == '\t' ) {
                    if ( state == reading_start ) {
                        state = start_read;
                        cheat_table->lines[pid].start = acc;
                        if ( pid > 0 && cheat_table->lines[pid-1]
                                .start > acc ) {
                            fprintf(stderr, "[warn] read_input (%d): "
                                    "linha não ordenada por chegada: "
                                    "[%hhu] %hu; [%hhu] %hu\n",
                                    reading_start, pid-1,
                                    cheat_table->lines[pid-1].start,
                                    pid, acc);
                        }
                    } else if ( state == reading_service ) {
                        state = service_read;
                        cheat_table->lines[pid].service = acc;
                        cheat_table->lines[pid].io_start = nios;
                    } else {
                        assert( 0 && "unreachable" );
                    }
                    acc = 0;
                } else if ( state == reading_service && c == '\n' ) {
                    state = begin_line;
                    cheat_table->lines[pid].service = acc;
                    cheat_table->lines[pid].io_start = nios;
                    cheat_table->lines[pid].io_count = 0;
                    acc = 0;
                    pid += 1;
                } else {
                    fprintf(stderr, "[warn] read_input (%d): "
                            "unhandled char: '%c' (%d)\n",
                            state, c, c);
                }
                break;
            case start_read:
                if ( '0' <= c && c <= '9' ) {
                    assert( acc == 0 );
                    acc = c - '0';
                    state = reading_service;
                } else if ( c == ' ' || c == '\t' ) {
                    // Nothing to do
                } else {
                    fprintf(stderr, "[warn] read_input (%d): "
                            "unhandled char: '%c' (%d)\n",
                            state, c, c);
                }
                break;
            case service_read:
                if ( '0' <= c && c <= '9' ) {
                    assert( acc == 0 );
                    acc = c - '0';
                    state = reading_io_begin;
                    if ( nios >= io_cap ) {
                        io_cap *= 2;
                        cheat_io_table = (CIO *) realloc_or_exit(
                                "cheat_io_table",
                                cheat_io_table,
                                io_cap*sizeof(CIO));
                    }
                } else if ( c == ' ' || c == '\t' ) {
                    // Nothing to do
                } else if ( c == '\n' ) {
                    state = begin_line;
                    cheat_table->lines[pid].io_count =
                        nios - cheat_table->lines[pid].io_start;
                    assert( acc == 0 );
                    pid += 1;
                } else {
                    fprintf(stderr, "[warn] read_input (%d): "
                            "unhandled char: '%c' (%d)\n",
                            state, c, c);
                }
                break;
            case reading_io_begin:
                if ( '0' <= c && c <= '9' ) {
                    acc = 10*acc + c - '0';
                } else if ( c == '-' ) {
                    state = io_begin_read;
                    cheat_io_table[nios].begin = acc;
                    acc = 0;
                } else {
                    fprintf(stderr, "[warn] read_input (%d): "
                            "unhandled char: '%c' (%d)\n",
                            state, c, c);
                }
                break;
            case io_begin_read:
                if ( c == 'D' ) {
                    state = io_type_read;
                    cheat_io_table[nios].io_type = IO_Disk;
                    nios += 1;
                } else if ( c == 'T' ) {
                    state = io_type_read;
                    cheat_io_table[nios].io_type = IO_Tape;
                    nios += 1;
                } else if ( c == 'P' ) {
                    state = io_type_read;
                    cheat_io_table[nios].io_type = IO_Printer;
                    nios += 1;
                } else {
                    fprintf(stderr, "[warn] read_input (%d): "
                            "unhandled char: '%c' (%d)\n",
                            state, c, c);
                }
                const b32 is_not_first_io =
                    (pid == 0 && nios-1 > 0)
                    || (
                        cheat_table->lines[pid-1].io_start +
                        cheat_table->lines[pid-1].io_count < nios-1
                    );
                if ( state == io_type_read
                        && is_not_first_io
                        && cheat_io_table[nios-2].begin
                            >= cheat_io_table[nios-1].begin ) {
                    fprintf(stderr, "[warn] read_input (%d): "
                            "IO não ordenado por início: "
                            "[%hhu] (%hu) %hu-%s "
                            "(%hu) %hu-%s\n",
                            io_begin_read, pid,
                            nios-2,
                            cheat_io_table[nios-2].begin,
                            io_name(cheat_io_table[nios-2].io_type),
                            nios-1,
                            cheat_io_table[nios-1].begin,
                            io_name(cheat_io_table[nios-1].io_type));
                }
                break;
            case io_type_read:
                if ( c == '\n' ) {
                    state = begin_line;
                    cheat_table->lines[pid].io_count =
                        nios - cheat_table->lines[pid].io_start;
                    pid += 1;
                } else if ( c == ' ' || c == '\t' ) {
                    state = service_read;
                } else {
                    fprintf(stderr, "[warn] read_input (%d): "
                            "unhandled char: '%c' (%d)\n",
                            state, c, c);
                }
                break;
            default:
                assert( 0 && "unreachable" );
                break;
        }
    }
    cheat_table->len = pid;

    print_input(stdout, pid, cap, nios, io_cap);

    *numpcb = pid;
    *numios = nios;
}

void unread_input() {
    free(cheat_table);
    free(cheat_io_table);
}

void print_stats(FILE *f, const PCB *pcbs, const u32 numpcbs) {
    fprintf(f, "=== Statisticas ===\n\n");
    u32 total_turn_around = 0;
    f64 total_ratio = 0;
    for ( u32 i = 0; i < numpcbs; i++ ) {
        PCB info = pcbs[i];
        assert( info.status == p_done );
        assert( info.pid == i );
        const u32 turn_around = info.end_time - info.start_time;
        const f64 ratio =
            ((f64) turn_around) / ((f64) info.running_time);
        total_turn_around += turn_around;
        total_ratio += ratio;

        fprintf(f, "Processo %hhu:\n", info.pid);
        fprintf(f, "    start_time   : %hu\n", info.start_time);
        fprintf(f, "    end_time     : %hu\n", info.end_time);
        fprintf(f, "    running_time : %hu\n", info.running_time);
        fprintf(f, "    io_time      : %hu\n", info.io_time);
        fprintf(f, "    turn_around  : %hu\n", turn_around);
        fprintf(f, "    ratio(tq/ts) : %lf\n", ratio);
    }
    fprintf(f, "\nMédia:\n");
    fprintf(f, "    turn_around  : %hu\n",
            total_turn_around / numpcbs);
    fprintf(f, "    ratio(tq/ts) : %lf\n",
            total_ratio / ((f64) numpcbs));
    fprintf(f, "\n");
}

int main() {

    Log_Ctx logctx = {
        .type = log_sout,
    };

    u32 numpcb, numios;
    read_input(&numpcb, &numios);

    // Alocando memoria
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

    //Chamando a função que vai realizar o Robin com Feedback
    robinfeedback(qs, numq, qios, iodevs, pcbs, numpcb, logctx);

    print_log(logctx);

    print_stats(stdout, pcbs, numpcb);

    //Liberando memoria
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
