#include "types.h"
#include "queue.h"

#define MAX_PID         255     // 2^8 - 1 (unsigned char)
typedef u8 PID;

#define TIME_SLICE      4
typedef u16 time;

#define DISK_TIME       3
#define TAPE_TIME       10
#define PRINTER_TIME    17

#define DISK_NUM        1
#define TAPE_NUM        2
#define PRINTER_NUM     3

typedef struct _IO_Ctx {
    PID pid;
    time finish_IO;
} IO_Ctx;

typedef struct _IODev {
    Queue *q;
    IO_Ctx *ctx;
    u16 next_idx;
} IODev;

typedef enum _IO_t {
    IO_Disk = 0, IO_Tape, IO_Printer
    , IO_count
} IO_t;

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
typedef enum _PCB_Status {
    p_done = 0, p_running, p_ready, p_blocked
} PCB_Status;

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
