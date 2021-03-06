#include "devices/timer.h"
#include <debug.h>
#include <inttypes.h>
#include <round.h>
#include <stdio.h>
#include "devices/pit.h"
#include "threads/interrupt.h"
#include "threads/synch.h"
#include "threads/thread.h"
/* Lab 1: List for asleep precesses. */
#include "lib/kernel/list.h"
  
/* See [8254] for hardware details of the 8254 timer chip. */

#if TIMER_FREQ < 19
#error 8254 timer requires TIMER_FREQ >= 19
#endif
#if TIMER_FREQ > 1000
#error TIMER_FREQ <= 1000 recommended
#endif

/* Number of timer ticks since OS booted. */
static int64_t ticks;

/* Number of loops per timer tick.
   Initialized by timer_calibrate(). */
static unsigned loops_per_tick;

static intr_handler_func timer_interrupt;
static bool too_many_loops (unsigned loops);
static void busy_wait (int64_t loops);
static void real_time_sleep (int64_t num, int32_t denom);
static void real_time_delay (int64_t num, int32_t denom);

/* Lab 1: List for asleep processes. */
static struct list asleep_list;

/* Lab 1: Structure for asleep threads. */
static struct asleep_thread {
  struct list_elem pcb_elem;    /* Needed for asleep_list. */
  int64_t wake_tick;            /* The tick the thread will wake up at. */
  struct thread *pcb;           /* The thread. */
};

/* Lab 1: Compares two asleep_threads given by A and B.
 * Returns true if A is less than B, iff A->wake_tick < B->wake_tick.
 * Returns false otherwise.
 * Thrid argument, AUX, can be NULL, is not used.
 */
static bool
compare_asleep_threads (const struct list_elem *a,
                        const struct list_elem *b,
                        void *aux UNUSED)
{
  struct asleep_thread *ta = list_entry (a, struct asleep_thread, pcb_elem);
  struct asleep_thread *tb = list_entry (b, struct asleep_thread, pcb_elem);
  return ta->wake_tick < tb->wake_tick;
}

/* Sets up the timer to interrupt TIMER_FREQ times per second,
   and registers the corresponding interrupt. */
void
timer_init (void) 
{
  pit_configure_channel (0, 2, TIMER_FREQ);
  intr_register_ext (0x20, timer_interrupt, "8254 Timer");
  
  /* Lab 1: Initialize asleep_list. */
  list_init(&asleep_list);
}

/* Calibrates loops_per_tick, used to implement brief delays. */
void
timer_calibrate (void) 
{
  unsigned high_bit, test_bit;

  ASSERT (intr_get_level () == INTR_ON);
  printf ("Calibrating timer...  ");

  /* Approximate loops_per_tick as the largest power-of-two
     still less than one timer tick. */
  loops_per_tick = 1u << 10;
  while (!too_many_loops (loops_per_tick << 1)) 
    {
      loops_per_tick <<= 1;
      ASSERT (loops_per_tick != 0);
    }

  /* Refine the next 8 bits of loops_per_tick. */
  high_bit = loops_per_tick;
  for (test_bit = high_bit >> 1; test_bit != high_bit >> 10; test_bit >>= 1)
    if (!too_many_loops (high_bit | test_bit))
      loops_per_tick |= test_bit;

  printf ("%'"PRIu64" loops/s.\n", (uint64_t) loops_per_tick * TIMER_FREQ);
}

/* Returns the number of timer ticks since the OS booted. */
int64_t
timer_ticks (void) 
{
  enum intr_level old_level = intr_disable ();
  int64_t t = ticks;
  intr_set_level (old_level);
  return t;
}

/* Returns the number of timer ticks elapsed since THEN, which
   should be a value once returned by timer_ticks(). */
int64_t
timer_elapsed (int64_t then) 
{
  return timer_ticks () - then;
}

/* Sleeps for approximately TICKS timer ticks.  Interrupts must
   be turned on. */
void
timer_sleep (int64_t ticks) 
{
  /*
  int64_t start = timer_ticks ();
  */

  ASSERT (intr_get_level () == INTR_ON);

  /* Original implementation: busy wait.
   * thread_yield() puts the current thread at
   * the end of ready_list, is executed later
   * to check if the time to wait has passed.
   */
  /*
  while (timer_elapsed (start) < ticks) 
    thread_yield ();
  */

  /* Lab 1: block wait.
   * thread_block() blocks the current thread
   * so it won't execute till thread_unblock()
   * is called.
   * The interruptions must be turned off to ensure
   * no errors will raise while blocking threads.
   * Also, ticks must be positive, otherwise doesn't
   * make sense.
   */
  struct asleep_thread t;
  if (ticks > 0) {
    struct thread *curr = thread_current ();
    t.pcb = curr;
    t.wake_tick = timer_ticks () + ticks;
    /* Use struct asleep_thread instead. */
    //curr->remaining_time = ticks;
    //list_push_back (&asleep_list, &(curr->pcb_elem));
    list_insert_ordered (&asleep_list, &(t.pcb_elem), &compare_asleep_threads, NULL);
    int old = intr_set_level (INTR_OFF);
    thread_block ();
    intr_set_level (old);
  }
}

/* Sleeps for approximately MS milliseconds.  Interrupts must be
   turned on. */
void
timer_msleep (int64_t ms) 
{
  real_time_sleep (ms, 1000);
}

/* Sleeps for approximately US microseconds.  Interrupts must be
   turned on. */
void
timer_usleep (int64_t us) 
{
  real_time_sleep (us, 1000 * 1000);
}

/* Sleeps for approximately NS nanoseconds.  Interrupts must be
   turned on. */
void
timer_nsleep (int64_t ns) 
{
  real_time_sleep (ns, 1000 * 1000 * 1000);
}

/* Busy-waits for approximately MS milliseconds.  Interrupts need
   not be turned on.

   Busy waiting wastes CPU cycles, and busy waiting with
   interrupts off for the interval between timer ticks or longer
   will cause timer ticks to be lost.  Thus, use timer_msleep()
   instead if interrupts are enabled. */
void
timer_mdelay (int64_t ms) 
{
  real_time_delay (ms, 1000);
}

/* Sleeps for approximately US microseconds.  Interrupts need not
   be turned on.

   Busy waiting wastes CPU cycles, and busy waiting with
   interrupts off for the interval between timer ticks or longer
   will cause timer ticks to be lost.  Thus, use timer_usleep()
   instead if interrupts are enabled. */
void
timer_udelay (int64_t us) 
{
  real_time_delay (us, 1000 * 1000);
}

/* Sleeps execution for approximately NS nanoseconds.  Interrupts
   need not be turned on.

   Busy waiting wastes CPU cycles, and busy waiting with
   interrupts off for the interval between timer ticks or longer
   will cause timer ticks to be lost.  Thus, use timer_nsleep()
   instead if interrupts are enabled.*/
void
timer_ndelay (int64_t ns) 
{
  real_time_delay (ns, 1000 * 1000 * 1000);
}

/* Prints timer statistics. */
void
timer_print_stats (void) 
{
  printf ("Timer: %"PRId64" ticks\n", timer_ticks ());
}

/* Timer interrupt handler. */
static void
timer_interrupt (struct intr_frame *args UNUSED)
{
  /* Original implementation: busy wait.
   * Counts ticks and checks every time if the
   * thread can continue its execution.
   * Still we need to count ticks.
   */
  ticks++;

  /* Lab 1: block wait.
   * For every asleep thread in asleep_list:
   * 1. Decrement the remaining_time.
   * 2. If remaining_time is 0 then unblock the thread
   *    and remove it from asleep_list.
   * 3. Otherwise continue with the next thread.
   * If using struct asleep_thread, only unblock threads
   * that must wake up at the current timer tick
   * (asleep_list is ordered).
   */
  struct list_elem *e;
  for (e = list_begin (&asleep_list);
       e != list_end (&asleep_list);
       e = list_next (e)) {
    /* Use struct asleep_thread instead. */
    /*
    struct thread *asleep_t = list_entry (e, struct thread, pcb_elem);
    asleep_t->remaining_time--;
    if (!asleep_t->remaining_time) {
      thread_unblock (asleep_t);
      list_remove (e);
    }
    */
    struct asleep_thread *asleep_t = list_entry (e, struct asleep_thread, pcb_elem);
    if (asleep_t->wake_tick == ticks) {
      thread_unblock (asleep_t->pcb);
      list_remove (e);
    } else {
      break;
    }
  }

  thread_tick ();
}

/* Returns true if LOOPS iterations waits for more than one timer
   tick, otherwise false. */
static bool
too_many_loops (unsigned loops) 
{
  /* Wait for a timer tick. */
  int64_t start = ticks;
  while (ticks == start)
    barrier ();

  /* Run LOOPS loops. */
  start = ticks;
  busy_wait (loops);

  /* If the tick count changed, we iterated too long. */
  barrier ();
  return start != ticks;
}

/* Iterates through a simple loop LOOPS times, for implementing
   brief delays.

   Marked NO_INLINE because code alignment can significantly
   affect timings, so that if this function was inlined
   differently in different places the results would be difficult
   to predict. */
static void NO_INLINE
busy_wait (int64_t loops) 
{
  while (loops-- > 0)
    barrier ();
}

/* Sleep for approximately NUM/DENOM seconds. */
static void
real_time_sleep (int64_t num, int32_t denom) 
{
  /* Convert NUM/DENOM seconds into timer ticks, rounding down.
          
        (NUM / DENOM) s          
     ---------------------- = NUM * TIMER_FREQ / DENOM ticks. 
     1 s / TIMER_FREQ ticks
  */
  int64_t ticks = num * TIMER_FREQ / denom;

  ASSERT (intr_get_level () == INTR_ON);
  if (ticks > 0)
    {
      /* We're waiting for at least one full timer tick.  Use
         timer_sleep() because it will yield the CPU to other
         processes. */                
      timer_sleep (ticks); 
    }
  else 
    {
      /* Otherwise, use a busy-wait loop for more accurate
         sub-tick timing. */
      real_time_delay (num, denom); 
    }
}

/* Busy-wait for approximately NUM/DENOM seconds. */
static void
real_time_delay (int64_t num, int32_t denom)
{
  /* Scale the numerator and denominator down by 1000 to avoid
     the possibility of overflow. */
  ASSERT (denom % 1000 == 0);
  busy_wait (loops_per_tick * num / 1000 * TIMER_FREQ / (denom / 1000)); 
}
