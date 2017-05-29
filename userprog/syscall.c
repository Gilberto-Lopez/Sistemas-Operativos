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

static void
syscall_handler (struct intr_frame *f UNUSED)
{
  //printf ("system call!\n");
  //thread_exit ();
	uint32_t* esp = f -> esp;
	uint32_t syscall = *esp;
	esp++;

	switch (syscall) {
		case SYS_WRITE: {
			int fd = *esp;
			esp++;
			void* buffer = (void*)*esp;
			esp++;
			unsigned int size = *esp;

			putbuf(buffer, size);

			break;
		}
    case SYS_EXEC:{
      f->eax = process_execute((char*) *esp);
      break;
    }
    case SYS_WAIT:{
      f->eax = process_wait((int) *esp);
      break;
    }
    /*
     * Practice6
     * In case that we have SYS_EXIT we do the following:
     * 1) Obtain the vader of the current thread.
     * 2) If there is no vader, we call thread exit.
     * 3) In the case that vader exists, we look for the actual thread in vader's childs' list.
     * 4) We assign the exit status to the son.
     */
		case SYS_EXIT: {
        int status = (int) * esp;
        struct thread *t = thread_current();
		printf("%s: exit(%d)\n", t->name,status);

      struct thread *vader = t->father;
      if(vader != NULL){
        struct list_elem *sons;
        struct son *luke = NULL;
        for(sons = list_begin(&vader->sons); sons != list_end(&vader->sons); sons = list_next(sons)){
          struct son *leia = list_entry(sons,struct son,son_elem);
          if(leia->id == t->tid && leia->exit_stat != -1){
            luke = leia; //incest jeje SW:A New Hope
            luke->exit_stat = status;
          }
        }
      }

			thread_exit();
			break;
		}
	}
}
