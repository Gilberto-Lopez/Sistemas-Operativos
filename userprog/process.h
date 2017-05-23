#ifndef USERPROG_PROCESS_H
#define USERPROG_PROCESS_H

#include "threads/thread.h"

tid_t process_execute (const char *file_name);
int process_wait (tid_t);
void process_exit (void);
void process_activate (void);
char* coloca(void* file_name);
void imprime(void* inicio, void* fin);
/* Lab 07: increase_stack function. */
bool increase_stack (void);

#endif /* userprog/process.h */
