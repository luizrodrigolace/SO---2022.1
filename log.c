typedef enum _Event_t {
    e_new_pid = 0, e_enter_cpu, e_leave_cpu,
    e_ask_IO, e_start_IO, e_from_IO, e_done
    , e_count
} Event_t;

typedef struct _Event {
    time t;
    PID pid;
    Event_t event;
    IO_t io;
} Event;

Event create_event(const time t, const PID pid,
        const Event_t event, IO_t io) {
    Event ret = { .t = t, .pid = pid, .event = event, io = io, };
    return ret;
}

typedef enum _Log_t {
    log_sout = 0x01, log_vgraph = 0x02, log_hgraph = 0x04,
    log_unknown_8 = 0x08,
    log_unknown_16 = 0x10, log_unknown_32 = 0x20,
    log_unknown_64 = 0x40, log_unknown_128 = 0x80
} Log_t;

typedef struct _Log_Ctx {
    Log_t type;
} Log_Ctx;

void print_log(const Log_Ctx log) {
    (void) log;
    printf("\n");
}

void log_event(const Log_Ctx log, Event event) {
    assert( log.type == log_sout );
    switch ( event.event ) {
        case e_new_pid:
            assert( event.io == IO_count );
            printf("[%hu] Novo processo %hhu\n",
                    event.t, event.pid);
            break;
        case e_enter_cpu:
            assert( event.io == IO_count );
            printf("[%hu] Processo %hhu: entrou na cpu\n",
                    event.t, event.pid);
            break;
        case e_leave_cpu:
            assert( event.io == IO_count );
            printf("[%hu] Processo %hhu: saiu da cpu\n",
                    event.t, event.pid);
            break;
        case e_ask_IO:
            printf("[%hu] Processo %hhu: pediu IO (%s)\n",
                    event.t, event.pid, io_name(event.io));
            break;
        case e_start_IO:
            printf("[%hu] Processo %hhu: pedido iniciado (%s)\n",
                    event.t, event.pid, io_name(event.io));
            break;
        case e_from_IO:
            printf("[%hu] Processo %hhu: voltou do IO (%s)\n",
                    event.t, event.pid, io_name(event.io));
            break;
        case e_done:
            assert( event.io == IO_count );
            printf("[%hu] Processo %hhu: terminou\n",
                    event.t, event.pid);
            break;
        case e_count:
        default:
            break;
    }
}

void log_new_pid(const Log_Ctx log, const time t, const PID pid) {
    log_event(log, create_event(t, pid, e_new_pid, IO_count));
}

void log_enter_cpu(const Log_Ctx log, const time t, const PID pid) {
    log_event(log, create_event(t, pid, e_enter_cpu, IO_count));
}

void log_leave_cpu(const Log_Ctx log, const time t, const PID pid) {
    log_event(log, create_event(t, pid, e_leave_cpu, IO_count));
}

void log_ask_IO(const Log_Ctx log, const time t, const PID pid,
        const IO_t io) {
    log_event(log, create_event(t, pid, e_ask_IO, io));
}

void log_start_IO(const Log_Ctx log, const time t, const PID pid,
        const IO_t io) {
    log_event(log, create_event(t, pid, e_start_IO, io));
}

void log_from_IO(const Log_Ctx log, const time t, const PID pid,
        const IO_t io) {
    log_event(log, create_event(t, pid, e_from_IO, io));
}

void log_done(const Log_Ctx log, const time t, const PID pid) {
    log_event(log, create_event(t, pid, e_done, IO_count));
}
