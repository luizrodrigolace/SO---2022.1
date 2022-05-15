#include "types.h"

#define MAX_PID         65536   // 2^16 (unsigned short)
typedef u16 PID;

#define TIME_SLICE      4

#define Premissa3       ???
#define DISK_TIME       10
#define TAPE_TIME       3
#define PRINTER_TIME    17
typedef enum _IO_t {
    IO_Disk, IO_Tape, IO_Printer
} IO_t;

u32 priority_from_io(IO_t io, u32 numio) {
    u32 ret = 0;
    switch (io) {
        case IO_Disk:
            ret = numio - 1;
            break;
        case IO_Tape:
            ret = 0;
            break;
        case IO_Printer:
            ret = 0;
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
    u32 priority;
    PID ppid;
    PCB_Status status;
} PCB;
// * Fila
#define NUM_QUEUES      4
