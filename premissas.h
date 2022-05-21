#include "types.h"
#include "queue.h"

// Limite máximo de processos criados
#define MAX_PID         255     // 2^8 - 1 (unsigned char)
typedef u8 PID;

// O valor da fatia de tempo dada aos processos em execução
#define TIME_SLICE      4
typedef u16 time;

// Duração do I/O
#define DISK_TIME       3
#define TAPE_TIME       10
#define PRINTER_TIME    17

// Quantidade de recursos de I/O
#define DISK_NUM        1
#define TAPE_NUM        2
#define PRINTER_NUM     3

// Contexto de I/O
typedef struct _IO_Ctx {
    PID pid;
    time finish_IO;
} IO_Ctx;

// Dispositivo de I/O
// Cada I/O terá uma fila e um número de contextos
// Simulando os N dispositivos de I/O 
typedef struct _IODev {
    Queue *q;
    IO_Ctx *ctx;
    u16 next_idx;
} IODev;

// Enumerando os tipos de I/O
// IO_count == quantos tipos de I/O existem
typedef enum _IO_t {
    IO_Disk = 0, IO_Tape, IO_Printer
    , IO_count
} IO_t;

// Retornando do I/O para fila do CPU
u32 priority_from_io(IO_t io) {
    u32 ret = 0;
    switch (io) {
        case IO_Disk:
            ret = 1;
            break;
        case IO_Tape:
            ret = 0;
            break;
        case IO_Printer:
            ret = 0;
            break;
        case IO_count: // fallthrough
        default:
            assert( 0 && "unreacheable" );
            break;
    }
    return ret;
}

// Retorna quantos dispositivos temos de cada tipo de I/O
u32 io_dev_count(IO_t io) {
    u32 ret = 0;
    switch (io) {
        case IO_Disk:
            ret = DISK_NUM;
            break;
        case IO_Tape:
            ret = TAPE_NUM;
            break;
        case IO_Printer:
            ret = PRINTER_NUM;
            break;
        case IO_count: // fallthrough
        default:
            assert( 0 && "unreacheable" );
            break;
    }
    return ret;
}

u32 io_time(IO_t io) {
    u32 ret = 0;
    switch (io) {
        case IO_Disk:
            ret = DISK_TIME;
            break;
        case IO_Tape:
            ret = TAPE_TIME;
            break;
        case IO_Printer:
            ret = PRINTER_TIME;
            break;
        case IO_count: // fallthrough
        default:
            assert( 0 && "unreacheable" );
            break;
    }
    return ret;
}

const char* io_name(IO_t io) {
    char *ret = 0;
    switch (io) {
        case IO_Disk:
            ret = "Disco";
            break;
        case IO_Tape:
            ret = "Fita";
            break;
        case IO_Printer:
            ret = "Impressora";
            break;
        case IO_count: // fallthrough
        default:
            assert( 0 && "unreacheable" );
            break;
    }
    return ret;
}

// Gerência de Processos:
// * Novo processo tem o PID do mais recente criado +1
// * PCB

// Enumerando o estados de um processo
typedef enum _PCB_Status {
    p_done = 0, p_running, p_ready, p_blocked
} PCB_Status;

// Process Control Block (Só o Contexto de Software)
typedef struct _PCB {
    PCB_Status status;
    PID pid;
    time start_time;
    time end_time;
    time running_time;
    // TODO: contar tempo bloqueado
    u8 priority;
} PCB;

// Tabela de Entrada
typedef struct _CIO {
    IO_t io_type;
    time begin;
} CIO;

// Linhas da tabela (Processos)
typedef struct _CLine {
    time start;
    time service;
    u16 io_start;
    u16 io_count;
} CLine;

typedef struct _CTable {
    PID len;
    CLine lines[];
} CTable;
