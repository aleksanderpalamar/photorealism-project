#pragma once

#include <windows.h>

// Log do modulo auxiliar FSR/AA, em <dll>/photorealism-plugin/photorealism-fsr.log.
//
// A escrita e sincrona e faz I/O de arquivo, entao nao pode acontecer no
// caminho de Present nem por draw: o caminho quente enfileira o trabalho para
// o worker de diagnostico, que e quem chama log_message.

// Define de qual modulo deriva o diretorio do log. Chamado no DLL_PROCESS_ATTACH,
// antes de qualquer log; sem isso o caminho cai no diretorio corrente.
void fsr_log_set_module(HMODULE module);

void log_message(const char* format, ...);
