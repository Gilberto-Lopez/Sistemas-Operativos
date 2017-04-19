#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"

static void syscall_handler (struct intr_frame *);

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

/* Lab 05
    Temporary implementation of syscall_handler
*/
static void
syscall_handler (struct intr_frame *f UNUSED) 
{
  uint32_t* esp = f->esp;
  uint32_t syscall = *esp;
  esp++;

  switch(syscall) {
    case SYS_WRITE: {
      int fd = *esp;
      esp++;
      void* buffer = (void*)*esp;
      esp++;
      unsigned int size = *esp;

      putbuf(buffer, size);

      break;
    }
    case SYS_EXIT: {
      printf("%s: exit(0)\n", thread_current()->name);
      thread_exit();

      break;
    }
  }
}
