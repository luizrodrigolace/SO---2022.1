#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include "types.h"
#include "premissas.h"
#define __QUEUE_IMPL__
#include "queue.h"

// Tabelas de entrada
static CTable cheat_table;
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
    const CLine cheat_line = cheat_table.lines[pid];
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
    assert( cheat_table.len == numpcb );
    const PID last_pid = cheat_table.len - 1;
    const CLine last_pid_line = cheat_table.lines[last_pid];
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
        return pcbs[last_pid_not_done].status == p_done;
    }
}

/* verifica se algum processo chegou no tempo t.
 * retorna true (!= 0) se chegou um novo processo,
 * retorna false (0) se não chegou um novo processo.
 * retorna no pid o pid do processo que acabou de chegar */
b32 new_process(const time t, PID *pid) {

    static PID last_pid_not_started = 0;

    if ( last_pid_not_started < cheat_table.len ) {
        const time next_start =
            cheat_table.lines[last_pid_not_started].start;
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

/* Verifica se há processos novos a serem tratados e os coloca na fila, caso existam.
   Recebe: curr_time: tempo atual;
    * qs: um vetor de filas do cpu;
    * numq: quantidade de filas do cpu;
    * pcbs: vetor de contextos de software;
    * numpcb: quantidade de contextos de software.
    */               
*/
static inline
void handle_new_processes(const time curr_time,
        Queue **qs, const u32 numq,
        PCB *pcbs, const u32 numpcb) {

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

/* Verifica se há processos bloqueados, que acabaram de fazer I/O, e os coloca na fila, caso existam.
   Recebe: curr_time: tempo atual;
    * qs: um vetor de filas do cpu;
    * numq: quantidade de filas do cpu;
    * qios: vetor de filas de i/o (tamanho IO_count);
    * iodevs: vetor de dispositivos (tamanho IO_count);
    * pcbs: vetor de contextos de software;
    * numpcb: quantidade de contextos de software.  
TIRANDO DA "FILA DA IMPRESSORA QUE EXECUTA" E COLOCANDO NA FILA DA CPU             
*/
static inline
void handle_blocked_processes(const time curr_time,
        Queue **qs, const u32 numq,
        Queue **qios, IODev *iodevs,
        PCB *pcbs, const u32 numpcb) {

    // loop para cada tipo de i/o
    for ( u32 i = 0; i < IO_count; i++ ) {
        while ( 1 ) {
            
            // Coletando informações sobre o primeiro dispositivo de I/O que está executando a mais tempo
            const Queue_Ret qret =
                Queue_peek(iodevs[i].q);

            // Se existe algum dispositivo I/O ativo
            if( qret.err == Queue_Ok ) {
                const u32 idx = qret.data;
                assert( idx < io_dev_count(i) );
                // Coletando o contexto do dispotitivo selecionado que está executando a mais tempo
                const IO_Ctx ctx = iodevs[i].ctx[idx];
                const PID pid = ctx.pid;
                assert( pid < numpcb );
                assert( ctx.finish_IO >= curr_time );
              
                // Se o dispositivo que está executando a mais tempo já parou de executar
                if ( ctx.finish_IO == curr_time ) {
                    // Tirando o dispositivo que está rodando a mais tempo
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

    // Tirando da fila de espera do I/O e mandando executar ("COLOCANDO NA FILA DO DISPOSITIVO EXECUTANDO")
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
                // Selecionando um dispositivo e alterando seu contexto
                iodevs[i].ctx[iodevs[i].next_idx] = dev_ctx;
                // Colocando este dispositivo na fila de impressoras ("AQUI ESTAMOS SOMENTE REGISTRANDO QUE ELA ESTÁ ATIVA MAS ELA FICOU ATIVA QUANDO MUDAMOS SEU CTX")
                const Queue_Err qerr =
                    Queue_enqueue(iodevs[i].q, iodevs[i].next_idx);
                assert( qerr == Queue_Ok );
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
 * qs: um vetor de filas do cpu;
 * numq: quantidade de filas do cpu;
 * qios: vetor de filas de i/o (tamanho IO_count);
 * iodevs: vetor de dispositivos (tamanho IO_count);
 * pcbs: vetor de contextos de software;
 * numpcb: quantidade de contextos de software.
 */  
void robinfeedback(Queue **qs, const u32 numq,
        Queue **qios, IODev *iodevs,
        PCB *pcbs, const u32 numpcb) {
    return;
    // fazendo verificação quant de filas de cpu e contextos de software
    assert( numq == 2 );
    assert( numpcb <= MAX_PID );

    time curr_time = 0;
    b32 is_not_done = 1;
    
    // loop até finalizar todos os processos.
    while ( is_not_done ) {
    
        // Verificando se há processos novos 
        handle_new_processes(curr_time,
                qs, numq, pcbs, numpcb);

        handle_blocked_processes(curr_time,
                qs, numq, qios, iodevs, pcbs, numpcb);

        u32 first_not_empty = 0;

        // Se a fila da CPU não estiver vazia, vamos selecionar um processo para executar
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
            // Pegando o tempo que ele já rodou
            time running = pcbs[pid].running_time;
            time slice;
            // O processo vai rodar até o seu time slice acabar
            for ( slice = 0; slice < TIME_SLICE; slice++ ) {
                if ( pcbs[pid].status != p_running )
                    break;
                // Verificando se o processo vai realizar algum I/O no tempo atual
                const Roda_Ret roda_ret = roda(pid, running + slice);
               // Analisando o que aconteceu com o preocesso após ter executado uma unidade de tempo
                switch ( roda_ret.status ) {
                    case Roda_Done: {
                        pcbs[pid].status = p_done;
                    } break;
                    case Roda_NotDone: {
                        // Nothing to do
                    } break;
                    case Roda_IO: {
                        // Ficou com estado bloqueado
                        pcbs[pid].status = p_blocked;
                        const IO_t io = roda_ret.io_type;
                        assert( io < IO_count );
                        // Prioridade do retorno do I/O
                        pcbs[pid].priority = priority_from_io(io);
                        // Colocando o processo na fila de espera do I/O
                        const Queue_Err qerr =
                            Queue_enqueue(qios[io], pid);
                        assert( qerr == Queue_Ok );
                    } break;
                }
                // Analisando se algum processo foi criado
                handle_new_processes(curr_time + slice,
                        qs, numq, pcbs, numpcb);
               
                // Analisando se algum processo que estava fazendo I/O terminou
                handle_blocked_processes(curr_time + slice,
                        qs, numq, qios, iodevs, pcbs, numpcb);
            }
            // Somando uma unidade de tempo no tempo total de execução do processo
            pcbs[pid].running_time += slice;

            // Lendo o status do processo
            switch ( pcbs[pid].status ) {
                case p_done:
                    pcbs[pid].end_time = curr_time + slice;
                    break;
                case p_running:
                    pcbs[pid].status = p_ready;
                    // Processo que sofreu preempção retornando na fila de baixa prioridade  
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

void read_input(u32 *numpcb, u32 *numios) {
    (void) numpcb;
    (void) numios;
    printf("reading input... (but not!)\n");
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

int main() {

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
        const u32 dev_cnt = io_dev_count(i);
        iodevs[i].q = (Queue *) alloc_or_exit("idev.q_i", dev_cnt);
        Queue_init(iodevs[i].q, numpcb);
        iodevs[i].ctx = (IO_Ctx *) alloc_or_exit("idev.ctx_i", dev_cnt);
    }
    PCB *pcbs = (PCB *) alloc_or_exit("pcbs", sizeof(*pcbs)*numpcb);

    //Chamando a função que vai realizar o Robin com Feedback
    robinfeedback(qs, numq, qios, iodevs, pcbs, numpcb);

    printf("main!\n");

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
}
