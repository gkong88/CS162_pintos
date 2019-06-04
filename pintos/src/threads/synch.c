/* This file is derived from source code for the Nachos
   instructional operating system.  The Nachos copyright notice
   is reproduced in full below. */

/* Copyright (c) 1992-1996 The Regents of the University of California.
   All rights reserved.

   Permission to use, copy, modify, and distribute this software
   and its documentation for any purpose, without fee, and
   without written agreement is hereby granted, provided that the
   above copyright notice and the following two paragraphs appear
   in all copies of this software.

   IN NO EVENT SHALL THE UNIVERSITY OF CALIFORNIA BE LIABLE TO
   ANY PARTY FOR DIRECT, INDIRECT, SPECIAL, INCIDENTAL, OR
   CONSEQUENTIAL DAMAGES ARISING OUT OF THE USE OF THIS SOFTWARE
   AND ITS DOCUMENTATION, EVEN IF THE UNIVERSITY OF CALIFORNIA
   HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

   THE UNIVERSITY OF CALIFORNIA SPECIFICALLY DISCLAIMS ANY
   WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
   WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
   PURPOSE.  THE SOFTWARE PROVIDED HEREUNDER IS ON AN "AS IS"
   BASIS, AND THE UNIVERSITY OF CALIFORNIA HAS NO OBLIGATION TO
   PROVIDE MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS, OR
   MODIFICATIONS.
*/

#include "threads/synch.h"
#include <stdio.h>
#include <string.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "thread.h"
#include "synch.h"


/*
 * Returns max effective priority of all threads that depend on
 * the locks this thread owns
 */
int thread_get_children_priority() {
    struct list_elem *current_lock_elem;
    struct list_elem *current_thread_elem;
    struct lock *owned_lock;
    struct thread *dependent_thread;

    int max_priority ;

    max_priority = 0;
    current_lock_elem = list_begin(&(thread_current()->priority_holding));
    while(current_lock_elem != list_end(&(thread_current()->priority_holding))) {
        owned_lock = list_entry(current_lock_elem, struct lock, elem);
        current_thread_elem = list_begin(&((owned_lock->semaphore).waiters));
        while (current_thread_elem != list_end(&((owned_lock->semaphore).waiters))) {
            dependent_thread = list_entry(current_thread_elem, struct lock, elem);
            if (dependent_thread->effective_priority > max_priority) {
                max_priority = dependent_thread->effective_priority;
            }
            current_thread_elem = current_thread_elem->next;
        }
        current_lock_elem = current_lock_elem->next;
    }
    return max_priority;
}

/*
 *
 */
void thread_donate_priority(struct thread* thread_recipient, int donated_priority) {
    if (thread_recipient->effective_priority < donated_priority) {
        thread_recipient->effective_priority = donated_priority;
        if (!(thread_recipient->priority_waiting == NULL)) {
            thread_donate_priority(thread_recipient->priority_waiting->holder, donated_priority);
        }
    }
}

/* Initializes semaphore SEMA to VALUE.  A semaphore is a
   nonnegative integer along with two atomic operators for
   manipulating it:

   - down or "P": wait for the value to become positive, then
     decrement it.

   - up or "V": increment the value (and wake up one waiting
     thread, if any). */
void
sema_init (struct semaphore *sema, unsigned value)
{
  ASSERT (sema != NULL);

  sema->value = value;
  list_init (&sema->waiters);
}

/* Down or "P" operation on a semaphore.  Waits for SEMA's value
   to become positive and then atomically decrements it.

   This function may sleep, so it must not be called within an
   interrupt handler.  This function may be called with
   interrupts disabled, but if it sleeps then the next scheduled
   thread will probably turn interrupts back on. */
void
sema_down (struct semaphore *sema)
{
  enum intr_level old_level;

  ASSERT (sema != NULL);
  ASSERT (!intr_context ());

  old_level = intr_disable ();
  while (sema->value == 0)
    {
      list_push_back (&sema->waiters, &thread_current ()->elem);
      thread_block ();
    }
  sema->value--;
  intr_set_level (old_level);
}

/* Down or "P" operation on a semaphore, but only if the
   semaphore is not already 0.  Returns true if the semaphore is
   decremented, false otherwise.

   This function may be called from an interrupt handler. */
bool
sema_try_down (struct semaphore *sema)
{
  enum intr_level old_level;
  bool success;

  ASSERT (sema != NULL);

  old_level = intr_disable ();
  if (sema->value > 0)
    {
      sema->value--;
      success = true;
    }
  else
    success = false;
  intr_set_level (old_level);

  return success;
}

/* Up or "V" operation on a semaphore.  Increments SEMA's value
   and wakes up one thread of those waiting for SEMA, if any.

   This function may be called from an interrupt handler. */
void
sema_up (struct semaphore *sema)
{
  enum intr_level old_level;

  ASSERT (sema != NULL);

  old_level = intr_disable ();
  if (!list_empty (&sema->waiters))
    thread_unblock (pop_max_priority(&sema->waiters));
  sema->value++;
  intr_set_level (old_level);
}

static void sema_test_helper (void *sema_);

/* Self-test for semaphores that makes control "ping-pong"
   between a pair of threads.  Insert calls to printf() to see
   what's going on. */
void
sema_self_test (void)
{
  struct semaphore sema[2];
  int i;

  printf ("Testing semaphores...");
  sema_init (&sema[0], 0);
  sema_init (&sema[1], 0);
  thread_create ("sema-test", PRI_DEFAULT, sema_test_helper, &sema);
  for (i = 0; i < 10; i++)
    {
      sema_up (&sema[0]);
      sema_down (&sema[1]);
    }
  printf ("done.\n");
}

/* Thread function used by sema_self_test(). */
static void
sema_test_helper (void *sema_)
{
  struct semaphore *sema = sema_;
  int i;

  for (i = 0; i < 10; i++)
    {
      sema_down (&sema[0]);
      sema_up (&sema[1]);
    }
}

/* Initializes LOCK.  A lock can be held by at most a single
   thread at any given time.  Our locks are not "recursive", that
   is, it is an error for the thread currently holding a lock to
   try to acquire that lock.

   A lock is a specialization of a semaphore with an initial
   value of 1.  The difference between a lock and such a
   semaphore is twofold.  First, a semaphore can have a value
   greater than 1, but a lock can only be owned by a single
   thread at a time.  Second, a semaphore does not have an owner,
   meaning that one thread can "down" the semaphore and then
   another one "up" it, but with a lock the same thread must both
   acquire and release it.  When these restrictions prove
   onerous, it's a good sign that a semaphore should be used,
   instead of a lock. */
void
lock_init (struct lock *lock)
{
  ASSERT (lock != NULL);

  lock->holder = NULL;
  lock->holder_starting_priority = NULL;
  sema_init (&lock->semaphore, 1);
}

/* Acquires LOCK, sleeping until it becomes available if
   necessary.  The lock must not already be held by the current
   thread.

   This function may sleep, so it must not be called within an
   interrupt handler.  This function may be called with
   interrupts disabled, but interrupts will be turned back on if
   we need to sleep. */
void
lock_acquire (struct lock *lock)
{
  ASSERT (lock != NULL);
  ASSERT (!intr_context ());
  ASSERT (!lock_held_by_current_thread (lock));
  if (!sema_try_down(&lock->semaphore)) {
      struct thread *waiting_on_thread = lock->holder;
      thread_current()->priority_waiting = lock; //register this thread as waiting for a lock. This allows threads wanting to donate priority to this thread recursively donate to the thread this thread waits on.
      thread_donate_priority(waiting_on_thread, thread_current()->effective_priority);
      sema_down (&lock->semaphore); //acquire the semaphore, block as necessary.
      // when blocked on the lock's semaphore, owning thread can query blocking threads to inhereit their priority as necessary
      // lock acquired
      thread_current()->priority_waiting = NULL;// remove the waiting for lock
  }
  lock->holder = thread_current (); //now that you own the lock, make sure everyone knows. that you can inherit priorities for waiters on this lock
  list_push_back(&(thread_current()->priority_holding), &(lock->elem));
  // no need to update priorities now, because any lock waiters must have lower effective priority than current thread priority, or else you wouldnt have been able to compete for the lock and beat them.
}

/* Tries to acquires LOCK and returns true if successful or false
   on failure.  The lock must not already be held by the current
   thread.

   This function will not sleep, so it may be called within an
   interrupt handler. */
bool
lock_try_acquire (struct lock *lock)
{
  bool success;

  ASSERT (lock != NULL);
  ASSERT (!lock_held_by_current_thread (lock));

  success = sema_try_down (&lock->semaphore);
  if (success)
    lock->holder = thread_current ();
  return success;
}

/* Releases LOCK, which must be owned by the current thread.

   An interrupt handler cannot acquire a lock, so it does not
   make sense to try to release a lock within an interrupt
   handler. */
void
lock_release (struct lock *lock)
{
    int children_priority;
  ASSERT (lock != NULL);
  ASSERT (lock_held_by_current_thread (lock));

  lock->holder = NULL;
  list_remove(&(lock->elem);
  sema_up (&lock->semaphore);
  children_priority = thread_get_children_priority();
  if (thread_current() -> priority > children_priority) {
      thread_current() ->effective_priority = thread_current() -> priority;
  }
  else {
      thread_current() ->effective_priority = children_priority;
  }
//    thread_donate_priority(thread_current(), 0); //update the thread you were waiting on to reduce their priority
}

/* Returns true if the current thread holds LOCK, false
   otherwise.  (Note that testing whether some other thread holds
   a lock would be racy.) */
bool
lock_held_by_current_thread (const struct lock *lock)
{
  ASSERT (lock != NULL);

  return lock->holder == thread_current ();
}

/* One semaphore in a list. */
struct semaphore_elem
  {
    struct list_elem elem;              /* List element. */
    struct semaphore semaphore;         /* This semaphore. */
  };






/* Searches list, a list of threads, for the thread with maximum priority
 * */
struct thread *pop_max_priority_waiter(struct list *sema_list) {
    struct list_elem *current_elem;
    struct list_elem *max_elem;
    struct semaphore current_semaphore;
    struct semaphore max_priority_semaphore;
    struct thread *current_thread;

    int max_priority = -1;

    current_elem = list_begin(sema_list);
    while (current_elem != list_end(sema_list)) {
        // a little different from thread list_entry, where threads already had a list_elem
        // semaphores needed a new struct (think container) that wraps a semaphore and list_elem

        //what you get  is a pointer to semaphore_elem, which we then use to access the semaphore attribute
        current_semaphore = list_entry(current_elem, struct semaphore_elem, elem)->semaphore; //this is a semaphore
        // semaphores have a waiters list attribute
        // lets get the address of that list and throw it into the list_begin function
        // then we'll deference the list_elem to it's wrapping thread object, where it is called "elem"
        // this gives us a pointer to the thread
        current_thread = list_entry(list_begin(&(current_semaphore.waiters)), struct thread, elem);
        if (current_thread->effective_priority > max_priority) { // of the waiters in semaphore
            max_elem = current_elem;
            max_priority = current_thread->effective_priority;
            max_priority_semaphore = current_semaphore;
        }
        current_elem = current_elem->next;
    }
    list_remove(max_elem);
    return &max_priority_semaphore;
}

/* Initializes condition variable COND.  A condition variable
   allows one piece of code to signal a condition and cooperating
   code to receive the signal and act upon it. */
void
cond_init (struct condition *cond) // initialize the condition variable
{
  ASSERT (cond != NULL);

  list_init (&cond->waiters);
}

/* Atomically releases LOCK and waits for COND to be signaled by
   some other piece of code.  After COND is signaled, LOCK is
   reacquired before returning.  LOCK must be held before calling
   this function.

   The monitor implemented by this function is "Mesa" style, not
   "Hoare" style, that is, sending and receiving a signal are not
   an atomic operation.  Thus, typically the caller must recheck
   the condition after the wait completes and, if necessary, wait
   again.

   A given condition variable is associated with only a single
   lock, but one lock may be associated with any number of
   condition variables.  That is, there is a one-to-many mapping
   from locks to condition variables.

   This function may sleep, so it must not be called within an
   interrupt handler.  This function may be called with
   interrupts disabled, but interrupts will be turned back on if
   we need to sleep. */
void
cond_wait (struct condition *cond, struct lock *lock)
{
  struct semaphore_elem waiter;

  ASSERT (cond != NULL);
  ASSERT (lock != NULL);
  ASSERT (!intr_context ());
  ASSERT (lock_held_by_current_thread (lock));

  sema_init (&waiter.semaphore, 0); // create a semaphore just for this.
  list_push_back (&cond->waiters, &waiter.elem); //put this semaphore on the semaphore-waiter-list
  lock_release (lock); // give up your lock
  sema_down (&waiter.semaphore); //block on your semaphore, you will wake up when a signaling event pulls you off the list
  lock_acquire (lock); // acquire the lock
}

/* If any threads are waiting on COND (protected by LOCK), then
   this function signals one of them to wake up from its wait.
   LOCK must be held before calling this function.

   An interrupt handler cannot acquire a lock, so it does not
   make sense to try to signal a condition variable within an
   interrupt handler. */
void
cond_signal (struct condition *cond, struct lock *lock UNUSED)
{
  ASSERT (cond != NULL);
  ASSERT (lock != NULL);
  ASSERT (!intr_context ());
  ASSERT (lock_held_by_current_thread (lock));
// condition variables have a waiters linked list (of semaphores)
// waiters linked list consists of semaphore_elem, which link to a semaphore
// semaphores have a list of waiting threads
//
  if (!list_empty (&cond->waiters))
    sema_up (&list_entry (list_pop_front (&cond->waiters),
                          struct semaphore_elem, elem)->semaphore);
}

/* Wakes up all threads, if any, waiting on COND (protected by
   LOCK).  LOCK must be held before calling this function.

   An interrupt handler cannot acquire a lock, so it does not
   make sense to try to signal a condition variable within an
   interrupt handler. */
void
cond_broadcast (struct condition *cond, struct lock *lock)
{
  ASSERT (cond != NULL);
  ASSERT (lock != NULL);
  // no change is necessary. the scheduler will prioritize who it lets run
  // based off of priority
  while (!list_empty (&cond->waiters))
    cond_signal (cond, lock);
}

