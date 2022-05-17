#include "types.h"

#define MAX_PID         65536   // 2^16 (unsigned short)
typedef u16 PID;

#define TIME_SLICE      4


#define DISK_TIME       3
#define TAPE_TIME       10
#define PRINTER_TIME    17

#define DISK_NUM        1
#define TAPE_NUM        2
#define PRINTER_NUM     3

typedef enum _IO_t {
    IO_Disk = 0, IO_Tape, IO_Printer
    , IO_count
} IO_t;

typedef struct _IODev {
} IODev;

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
        case IO_count:
            assert( 0 && "unreacheable" );
            break;
    }
    return ret;
}

// Gerência de Processos:
// * Novo processo tem o PID do mais recente criado +1
// * PCB
typedef u16 time;
typedef enum _PCB_Status {
    p_done = 0, p_running, p_ready, p_blocked
} PCB_Status;

typedef struct _PCB {
    PCB_Status status;
    PID pid;
    time start_time;
    time end_time;
    time running_time;
    u8 priority;
} PCB;
