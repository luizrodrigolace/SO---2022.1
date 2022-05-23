#include <stdio.h>
#include <conio.h>
#include <stdlib.h>
#include <assert.h>

#include "types.h"
#include "premissas.h"
#define __QUEUE_IMPL__
#include "queue.h"
#include "log.c"
#include "main.c"

// * Criando a tabela com nProcessos dados
int tabela(int nProcessos){

    const PID len = nProcessos;
    int sorteioTipoIO;
    const u32 nios = 3;
    cheat_table->len = len;

    // Alocando memoria para todos os processos
    cheat_table = (CTable *) alloc_or_exit("cheat_table", sizeof(CTable) + len*sizeof(CLine));

    // Criando as linhas da tabela
    for(int i = 0; i < len; i++){
        cheat_table->lines[i].start = rand() % 4;
        cheat_table->lines[i].service = rand() % 5;
        cheat_table->lines[i].io_start = rand() % 10;
        cheat_table->lines[i].io_count = rand() % 3;
    }

    // Alocando memoria para a tabela de I/O
    cheat_io_table = (CIO *) alloc_or_exit("cheat_io)table", nios*sizeof(CIO));

    // Criando as linhas da tabela de I/O 
    for(int x = 0; x < len; x++){

        // Sorteando o tipo de I/O que o processo receberá
        sorteioTipoIO = rand() % 3;

        if(sorteioTipoIO == 0){
            cheat_io_table->io_type = IO_Disk;
        }
        else if(sorteioTipoIO == 1){
            cheat_io_table->io_type = IO_Tape;
        }
        else if(sorteioTipoIO == 3){
            cheat_io_table->io_type = IO_Printer;
        }
        
        cheat_io_table->begin = rand()%10;
    }
}