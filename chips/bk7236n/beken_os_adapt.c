/****************************************************************************
 * vendor/beken/chips/bk7236n/beken_os_adapter.c
 *
 * Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.  The
 * ASF licenses this file to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance with the
 * License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.  See the
 * License for the specific language governing permissions and limitations
 * under the License.
 *
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <inttypes.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <debug.h>
#include <pthread.h>
#include <fcntl.h>
#include <unistd.h>
#include <math.h>
#include <clock/clock.h>
#include <sys/param.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <irq/irq.h>
#include <nuttx/kmalloc.h>
#include <nuttx/mqueue.h>
#include <nuttx/spinlock.h>
#include <nuttx/mutex.h>
#include <nuttx/kthread.h>
#include <nuttx/wdog.h>
#include <nuttx/wqueue.h>
#include <nuttx/sched.h>
#include <nuttx/signal.h>
#include <nuttx/arch.h>
#include <nuttx/wireless/wireless.h>
#include <nuttx/tls.h>

#include "os/os.h"
#include "os/mem.h"
#include "os/str.h"
/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/
#define BEKEN_WAIT_FOREVER                 (0xFFFFFFFF)    /**< Wait Forever */

/****************************************************************************
 * Private Types
 ****************************************************************************/

/* OS interrupt adapter private data */

struct irq_adpt
{
  void (*func)(void *arg);  /* Interrupt callback function */
  void *arg;                /* Interrupt private data */
};

/* time private data */

struct time_adpt
{
  time_t      sec;          /* Second value */
  suseconds_t usec;         /* Micro second value */
};

/*  timer private data */

struct timer_adpt
{
  struct wdog_s wdog;       /* Timer handle */
  bool          repeat;     /* Flags indicate if it is cycle */
  uint32_t      delay;      /* Timeout ticks */
  void          *priv;      /* Beken specifi handler */
};

struct mq_adpt_s
{
  struct file mq;           /* Message queue handle */
  uint32_t    msgsize;      /* Message size */
  char        name[16];     /* Message queue name */
  char        cname[32];
};

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

static inline bk_err_t beken_errno_trans(int ret)
{
  if (!ret)
  {
    return BK_OK;
  }
  else
  {
    return BK_FAIL;
  }
}

static void os_timer_callback(wdparm_t arg)
{
  struct timer_adpt *timer_apt;

  timer_apt = (struct timer_adpt *)arg;
  if (timer_apt->repeat == true)
    {
      beken_timer_t *timer = (beken_timer_t *)timer_apt->priv;
      if (timer->function)
        {
          timer->function(timer->arg);
        }
      wd_start(&timer_apt->wdog, timer_apt->delay, os_timer_callback, arg);
    }
  else
    {
      beken2_timer_t *timer = (beken2_timer_t *)timer_apt->priv;
      if (timer->function)
        {
          timer->function(timer->left_arg, timer->right_arg);
        }
    }
}

/****************************************************************************
 * Public Functions declaration
 ****************************************************************************/


/****************************************************************************
 * Name: rtos_disable_int
 *
 * Description:
 *   This function disable the interrupts.
 *
 * Input Parameters:
 *   None
 *
 * Returned Value:
 *   The current interrupt status
 *
 ****************************************************************************/

uint32_t rtos_disable_int(void)
{
  irqstate_t flags;

  flags = enter_critical_section();

  return flags;
}

/****************************************************************************
 * Name: rtos_enable_int
 *
 * Description:
 *   This function enable/restore the interrupts status.
 *
 * Input Parameters:
 *   int_level - the saved interrupt status
 *
 * Returned Value:
 *   None
 *
 ****************************************************************************/

void rtos_enable_int(uint32_t int_level)
{
  leave_critical_section(int_level);
}

/****************************************************************************
 * Name: rtos_enter_critical
 *
 * Description:
 *   This function disable the interrupts.
 *
 * Input Parameters:
 *   None
 *
 * Returned Value:
 *   The current interrupt status
 *
 ****************************************************************************/

uint32_t rtos_enter_critical(void)
{
  irqstate_t flags;

  flags = enter_critical_section();

  return flags;
}

/****************************************************************************
 * Name: rtos_exit_critical
 *
 * Description:
 *   This function enable/restore the interrupts status.
 *
 * Input Parameters:
 *   int_level - the saved interrupt status
 *
 * Returned Value:
 *   None
 *
 ****************************************************************************/

void rtos_exit_critical(uint32_t int_level)
{
  leave_critical_section(int_level);
}

/****************************************************************************
 * Name: beken_time_get_time
 *
 * Description:
 *   Get system time value in milliseconds.
 *
 * Input Parameters:
 *   time_ptr - The pointer of time value in milliseconds
 *
 * Returned Value:
 *   Zero (BK_OK)
 *
 ****************************************************************************/

bk_err_t beken_time_get_time(beken_time_t *time_ptr)
{
  struct timespec ts;

  clock_systime_timespec(&ts);
  *time_ptr = ts.tv_sec * 1000 + ts.tv_nsec / 1000000;

  return BK_OK;
}

/****************************************************************************
 * Name: rtos_create_thread
 *
 * Description:
 *   Creates and starts a new thread.
 *
 * Input Parameters:
 *   thread     - Pointer to variable that will store the thread handler
 *   priority   - A priority number for thread
 *   name       - A text name for the thread
 *   function   - The main thread function
 *   stack_size - Stack size for this thread
 *   arg        - Argument which will be passed to thread function
 * Returned Value:
 *   Zero (BK_OK) if the thead was successfully signalled. Otherwise,
 *   return -1 (BK_FAIL)
 *
 ****************************************************************************/

bk_err_t rtos_create_thread(beken_thread_t *thread, uint8_t priority, const char *name,
                        beken_thread_function_t function, uint32_t stack_size, beken_thread_arg_t arg)
{
  pid_t pid = -1;

  if (arg)
    {
      wlerr("Task(%s)'s arg is NOT NULL\n", name);
    }
  else
    {
      pid =  kthread_create(name, SCHED_PRIORITY_DEFAULT + 2 - priority, stack_size, (main_t)function, NULL);
    }

  if (pid <= 0)
    {
      wlerr("ERROR: Failed to create thread(%s): %d\n", name, pid);
      return BK_FAIL;
    }
  else
    {
      if (thread)
        { 
          *thread = (void *)((uintptr_t)pid);
        }
    }

  return BK_OK;
}

bk_err_t rtos_create_sram_thread(beken_thread_t *thread, uint8_t priority,
                                 const char *name,
                                 beken_thread_function_t function,
                                 uint32_t stack_size, beken_thread_arg_t arg)
{
  return rtos_create_thread(thread, priority, name, function, stack_size, arg);
}

/****************************************************************************
 * Name: rtos_delete_thread
 *
 * Description:
 *   Delete a thread.
 *
 * Input Parameters:
 *   thread - The handle of the thread, NULL is itself
 *
 * Returned Value:
 *   Zero (BK_OK)
 *
 ****************************************************************************/

bk_err_t rtos_delete_thread(beken_thread_t *thread)
{
  pid_t pid = 0;

  if (thread)
    {
      pid = (pid_t)((uintptr_t)*thread);
    }

  task_delete(pid);

  return BK_OK;
}

/****************************************************************************
 * Name: rtos_is_current_thread
 *
 * Description:
 *   Checks if a thread is the current thread.
 *
 * Input Parameters:
 *   thread - The handle of the thread
 *
 * Returned Value:
 *   true if the thread is the current thread.Otherwise, return false
 *
 ****************************************************************************/

bool rtos_is_current_thread(beken_thread_t* thread )
{
  pid_t pid;

  pid = nxsched_getpid();

  return (pid == (pid_t)((uintptr_t)*thread));
}

/****************************************************************************
 * Name: rtos_get_current_thread
 *
 * Description:
 *   Get current thread handler.
 *
 * Input Parameters:
 *   None
 *
 * Returned Value:
 *   The thread handler
 *
 ****************************************************************************/

beken_thread_t *rtos_get_current_thread(void)
{
  pid_t pid;

  pid = nxsched_getpid();

  return (beken_thread_t *)pid;
}

/****************************************************************************
 * Name: rtos_thread_join
 *
 * Description:
 *   A thread can await termination of another thread
 *
 * Input Parameters:
 *   thread - The thread handler to wait
 *
 * Returned Value:
 *   BK_OK
 *
 ****************************************************************************/

bk_err_t rtos_thread_join(beken_thread_t *thread)
{
  /* ToDo */
  return BK_OK;
}

/****************************************************************************
 * Name: rtos_init_semaphore
 *
 * Description:
 *   Initialises a counting semaphore and set count to 0.
 *
 * Input Parameters:
 *   semaphore - A pointer of the semaphore handler
 *   max_count - The max count number of this semaphore
 *
 * Returned Value:
 *   Zero (BK_OK) if the semaphore was successfully created. Otherwise,
 *   return -1 (BK_FAIL)
 *
 ****************************************************************************/

bk_err_t rtos_init_semaphore(beken_semaphore_t *semaphore, int max_count)
{
  sem_t *sem = NULL;
  int ret;

  sem  = kmm_malloc(sizeof(sem_t));
  if (!sem)
    {
      wlerr("ERROR: Failed to malloc semaphore\n");
      return BK_FAIL;
    }

  ret = nxsem_init(sem, 0, 0);
  if (ret == OK)
    {
      *semaphore = sem;
    }
  else
    {
      wlerr("ERROR: Failed to create semaphore:%d\n", ret);
    }
  return beken_errno_trans(ret);
}

bk_err_t rtos_init_semaphore_ex(beken_semaphore_t *semaphore, int max_count, int init_count)
{
  sem_t *sem = NULL;
  int ret;

  sem  = kmm_malloc(sizeof(sem_t));
  if (!sem)
    {
      wlerr("ERROR: Failed to malloc semaphore\n");
      return BK_FAIL;
    }

  ret = nxsem_init(sem, 0, init_count);
  if (ret == OK)
    {
      *semaphore = sem;
    }
  else
    {
      wlerr("ERROR: Failed to create semaphore:%d\n", ret);
    }

  return beken_errno_trans(ret);
}

/****************************************************************************
 * Name: rtos_set_semaphore
 *
 * Description:
 *   Set (post/put/increment) a semaphore.
 *
 * Input Parameters:
 *   semaphore - A pointer of the semaphore handler
 *
 * Returned Value:
 *   Zero (BK_OK) if the semaphore was post. Otherwise,
 *   return -1 (BK_FAIL)
 *
 ****************************************************************************/

bk_err_t rtos_set_semaphore(beken_semaphore_t *semaphore)
{
  int ret;
  sem_t *sem = (sem_t *)(*semaphore);

  ret = nxsem_post(sem);
  if (ret != OK)
    {
      wlerr("ERROR: Failed to post semaphore:%d\n", ret);
    }

  return beken_errno_trans(ret);
}

/****************************************************************************
 * Name: rtos_get_semaphore
 *
 * Description:
 *   Get (wait/decrement) a semaphore.
 *
 * Input Parameters:
 *   semaphore  - A pointer of the semaphore handler
 *   timeout_ms - The number of milliseconds to wait before returning
 *
 * Returned Value:
 *   Zero (BK_OK) if the semaphore was taken. Otherwise,
 *   return -1 (BK_FAIL)
 *
 ****************************************************************************/


bk_err_t rtos_get_semaphore(beken_semaphore_t *semaphore, uint32_t timeout_ms)
{
  int ret;
  sem_t *sem = (sem_t *)(*semaphore);

  if (timeout_ms == 0)
    {
      ret = nxsem_trywait(sem);
    }
  else if (timeout_ms == BEKEN_WAIT_FOREVER)
    {
      ret = nxsem_wait(sem);
    }
  else
    {
      ret = nxsem_tickwait(sem, MSEC2TICK(timeout_ms));
    }

  if (ret != OK)
    {
      wlerr("ERROR: Failed to get semaphore:%d\n", ret);
    }
  
  return beken_errno_trans(ret);
}

/****************************************************************************
 * Name: De-initialise a semaphore
 *
 * Description:
 *   Delete a semaphore.
 *
 * Input Parameters:
 *   semaphore  - A pointer of the semaphore handler
 *
 * Returned Value:
 *   Zero (BK_OK) if the semaphore was taken. Otherwise,
 *   return -1 (BK_FAIL)
 *
 ****************************************************************************/

bk_err_t rtos_deinit_semaphore(beken_semaphore_t *semaphore)
{
  int ret;
  sem_t *sem = (sem_t *)(*semaphore);

  ret = nxsem_destroy(sem);
  if (ret != OK)
    {
      wlerr("ERROR: Failed to destory semaphore:%d\n", ret);
    }
  kmm_free(sem);

  return beken_errno_trans(ret);
}

/****************************************************************************
 * Name: rtos_init_mutex
 *
 * Description:
 *   Initialises a mutex.
 *
 * Input Parameters:
 *   mtx - A pointer of the mutex handler
 *
 * Returned Value:
 *   Zero (BK_OK) if the semaphore was successfully created. Otherwise,
 *   return -1 (BK_FAIL)
 *
 ****************************************************************************/

bk_err_t rtos_init_mutex(beken_mutex_t *mtx)
{
  int ret;
  mutex_t *mutex;

  mutex = kmm_malloc(sizeof(mutex_t));
  if (!mutex)
    {
      wlerr("ERROR: Failed to kmm_malloc\n");
      return -1;
    }

  ret = nxmutex_init(mutex);
  if (ret == OK)
    {
      *mtx = mutex;
    }
  else
    {
      wlerr("ERROR: Failed to create mutex,ret:%d\n", ret);
      kmm_free(mutex);
    }

  return beken_errno_trans(ret);
}

/****************************************************************************
 * Name: rtos_lock_mutex
 *
 * Description:
 *   Obtains the lock on a mutex.
 *
 * Input Parameters:
 *   mtx - A pointer of the mutex handler
 *
 * Returned Value:
 *   Zero (BK_OK) if the semaphore was successfully created. Otherwise,
 *   return -1 (BK_FAIL)
 *
 ****************************************************************************/

bk_err_t rtos_lock_mutex(beken_mutex_t *mtx)
{
  int ret;
  mutex_t *mutex = (mutex_t *)*mtx;

  ret = nxmutex_lock(mutex);
  if (ret != OK)
    {
      wlerr("ERROR: Failed to lock mutex:%d\n", ret);
    }

  return beken_errno_trans(ret);
}

/****************************************************************************
 * Name: rtos_unlock_mutex
 *
 * Description:
 *   Releases the mutex.
 *
 * Input Parameters:
 *   mtx - A pointer of the mutex handler
 *
 * Returned Value:
 *   Zero (BK_OK) if the semaphore was successfully created. Otherwise,
 *   return -1 (BK_FAIL)
 *
 ****************************************************************************/

bk_err_t rtos_unlock_mutex(beken_mutex_t *mtx)
{
  int ret;
  mutex_t *mutex = (mutex_t *)*mtx;

  ret = nxmutex_unlock(mutex);
  if (ret != OK)
    {
      wlerr("ERROR: Failed to unlock mutex:%d\n", ret);
    }

  return beken_errno_trans(ret);
}

/****************************************************************************
 * Name: rtos_deinit_mutex
 *
 * Description:
 *   Destory the mutex.
 *
 * Input Parameters:
 *   mtx - A pointer of the mutex handler
 *
 * Returned Value:
 *   Zero (BK_OK)
 *
 ****************************************************************************/

bk_err_t rtos_deinit_mutex(beken_mutex_t *mtx)
{
  mutex_t *mutex = (mutex_t *)*mtx;
  
  nxmutex_destroy(mutex);
  kmm_free(mutex);

  return BK_OK;
}

/****************************************************************************
 * Name: rtos_init_queue
 *
 * Description:
 *   Initialises a Queue.
 *
 * Input Parameters:
 *   queue               - A pointer of the Queue handler
 *   name                - UNUSED
 *   message_size        - size in bytes of one message
 *   number_of_messages  - depth of the queue
 *
 * Returned Value:
 *   Zero (BK_OK) if the semaphore was successfully created. Otherwise,
 *   return -1 (BK_FAIL)
 *
 ****************************************************************************/

bk_err_t rtos_init_queue(beken_queue_t *queue, const char *name, uint32_t message_size,
                        uint32_t number_of_messages)
{
  struct mq_attr attr;
  struct mq_adpt_s *mq_adpt;
  int ret;

  mq_adpt = kmm_malloc(sizeof(struct mq_adpt_s));
  if (!mq_adpt)
    {
      wlerr("ERROR: Failed to kmm_malloc\n");
      return BK_FAIL;
    }
  memset(mq_adpt, 0x0, sizeof(struct mq_adpt_s));
  if (name)
    {
      snprintf(mq_adpt->name, sizeof(mq_adpt->name), "/tmp/%s", name);
    }
  else
    {
      snprintf(mq_adpt->name, sizeof(mq_adpt->name), "/tmp/%p", mq_adpt);
    }
  attr.mq_maxmsg  = number_of_messages;
  attr.mq_msgsize = message_size;
  attr.mq_curmsgs = 0;
  attr.mq_flags   = 0;
  strncpy(mq_adpt->cname, name, sizeof(mq_adpt->cname) - 1);
  ret = file_mq_open(&mq_adpt->mq, mq_adpt->name,
                     O_RDWR | O_CREAT, 0644, &attr);
  if (ret < 0)
    {
      wlerr("ERROR: Failed to create mqueue\n");
      kmm_free(mq_adpt);
      return beken_errno_trans(ret);
    }

  mq_adpt->msgsize = message_size;
  *queue           = mq_adpt;

  return beken_errno_trans(ret);
}

/****************************************************************************
 * Name: rtos_push_to_queue
 *
 * Description:
 *   Pushes an object/message onto a Queue.
 *
 * Input Parameters:
 *   queue       - A pointer of the Queue handler
 *   message     - The object to be added to the queue
 *   timeout_ms  - The number of milliseconds to wait before returning
 *
 * Returned Value:
 *   Zero (BK_OK) if the semaphore was successfully created. Otherwise,
 *   return -1 (BK_FAIL)
 *
 ****************************************************************************/

bk_err_t rtos_push_to_queue(beken_queue_t *queue, void *message, uint32_t timeout_ms)
{
  int ret;
  struct timespec timeout;
  struct mq_adpt_s *mq_adpt = (struct mq_adpt_s *)*queue;

  if (timeout_ms == BEKEN_WAIT_FOREVER || timeout_ms == 0)
    {
      ret = file_mq_send(&mq_adpt->mq, (const char *)message,
                         mq_adpt->msgsize, 0);
      if (ret < 0)
        {
          wlerr("Failed to send message to mqueue error=%d\n", ret);
        }
    }
  else
    {
      ret = clock_gettime(CLOCK_REALTIME, &timeout);
      if (ret < 0)
        {
          wlerr("Failed to get time\n");
          return false;
        }

      timeout.tv_sec  += timeout_ms / 1000;
      timeout.tv_nsec += (timeout_ms % 1000 ) * 1000 * 1000;

      if (timeout.tv_nsec >= 1000000000)
        {
          timeout.tv_sec += timeout.tv_nsec / 1000000000;
          timeout.tv_nsec %= 1000000000;
        }

      ret = file_mq_timedsend(&mq_adpt->mq, (const char *)message,
                              mq_adpt->msgsize, 0, &timeout);
      if (ret < 0)
        {
          wlerr("Failed to timedsend message to mqueue(%s) error=%d\n", mq_adpt->cname, ret);
        }
    }

  return beken_errno_trans(ret);
}

/****************************************************************************
 * Name: rtos_pop_from_queue
 *
 * Description:
 *   Pops an object/message from a Queue.
 *
 * Input Parameters:
 *   queue      - A pointer of the Queue handler
 *   message    - Pointer to a buffer that will keep the object from the Queue
 *   timeout_ms - The number of milliseconds to wait before returning
 *
 * Returned Value:
 *   Zero (BK_OK) if the semaphore was successfully created. Otherwise,
 *   return -1 (BK_FAIL)
 *
 ****************************************************************************/

bk_err_t rtos_pop_from_queue(beken_queue_t *queue, void *message, uint32_t timeout_ms)
{
  int ret;
  struct timespec timeout;
  struct mq_adpt_s *mq_adpt = (struct mq_adpt_s *)*queue;

  if (timeout_ms == BEKEN_WAIT_FOREVER)
    {
      ret = file_mq_receive(&mq_adpt->mq, message,
                         mq_adpt->msgsize, 0);
      if (ret < 0)
        {
          wlerr("Failed to send message to mqueue error=%d\n", ret);
        }
    }
  else
    {
      ret = clock_gettime(CLOCK_REALTIME, &timeout);
      if (ret < 0)
        {
          wlerr("Failed to get time:%d\n", ret);
          return beken_errno_trans(ret);
        }

      timeout.tv_sec  += timeout_ms / 1000;
      timeout.tv_nsec += (timeout_ms % 1000 ) * 1000 * 1000;
      if (timeout.tv_nsec >= 1000000000)
        {
          timeout.tv_sec += timeout.tv_nsec / 1000000000;
          timeout.tv_nsec %= 1000000000;
        }

      ret = file_mq_timedreceive(&mq_adpt->mq, message,
                              mq_adpt->msgsize, 0, &timeout);
      if (ret < 0)
        {
          wlerr("Failed to timedreceive message from mqueue(%s) error=%d\n", mq_adpt->cname, ret);
        }
    }

  return ret > 0 ? BK_OK : BK_FAIL;
}

/****************************************************************************
 * Name: rtos_deinit_queue
 *
 * Description:
 *   Destory the Queue.Relese the resource.
 *
 * Input Parameters:
 *   queue - A pointer of the Queue handler
 *
 * Returned Value:
 *   Zero (BK_OK)
 *
 ****************************************************************************/

bk_err_t rtos_deinit_queue(beken_queue_t *queue)
{
  struct mq_adpt_s *mq_adpt = (struct mq_adpt_s *)*queue;

  file_mq_close(&mq_adpt->mq);
  file_mq_unlink(mq_adpt->name);
  kmm_free(mq_adpt);

  return OK;
}

/****************************************************************************
 * Name: rtos_is_queue_empty
 *
 * Description:
 *   Check if a queue is empty.
 *
 * Input Parameters:
 *   queue - A pointer of the Queue handler
 *
 * Returned Value:
 *   true if the queue is empty. Otherwise, return false. 
 *
 ****************************************************************************/
 
bool rtos_is_queue_empty(beken_queue_t *queue)
{
  struct mq_adpt_s *mq_adpt = (struct mq_adpt_s *)*queue;
  struct mq_attr mq_attr;

  file_mq_getattr(&mq_adpt->mq, &mq_attr);
  return (mq_attr.mq_curmsgs == 0);
}

/****************************************************************************
 * Name: rtos_is_queue_full
 *
 * Description:
 *   Check if a queue is full.
 *
 * Input Parameters:
 *   queue - A pointer of the Queue handler
 *
 * Returned Value:
 *   true if the queue is full. Otherwise, return false. 
 *
 ****************************************************************************/

bool rtos_is_queue_full(beken_queue_t *queue)
{
  struct mq_adpt_s *mq_adpt = (struct mq_adpt_s *)*queue;
  struct mq_attr mq_attr;

  file_mq_getattr(&mq_adpt->mq, &mq_attr);

  return (mq_attr.mq_curmsgs == mq_attr.mq_maxmsg);
}

/****************************************************************************
 * Name: rtos_get_time
 *
 * Description:
 *   Gets time in miiliseconds since RTOS starts.
 *
 * Input Parameters:
 *   None
 *
 * Returned Value:
 *   Time in milliseconds since RTOS starts.
 *
 ****************************************************************************/

uint64_t rtos_get_time(void)
{
  struct timespec ts;

  clock_systime_timespec(&ts);

  return ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

/****************************************************************************
 * Name: Initialize a RTOS timer
 *
 * Description:
 *   Create one timer
 *   Timer does not start running until beken_start_timer is called
 *
 * Input Parameters:
 *   timer    - A pointer of the timer
 *   time_ms  - Timer period in milliseconds
 *   function - The callback handler function when timeout
 *   arg      - argument that will be passed to the callback function
 *
 * Returned Value:
 *   Zero (BK_OK) if the semaphore was successfully created. Otherwise,
 *   return -1 (BK_FAIL)
 *
 ****************************************************************************/

bk_err_t rtos_init_timer(beken_timer_t *timer, uint32_t time_ms, timer_handler_t function, void *arg)
{
  struct timer_adpt *timer_apt = NULL;

  timer_apt     = kmm_malloc(sizeof(struct timer_adpt));
  if (!timer_apt)
    {
      wlerr("ERROR: Failed to malloc struct timer_adpt\n");
      return BK_FAIL;
    }

  memset(timer_apt ,0x0, sizeof(struct timer_adpt));
  memset(timer,     0x0, sizeof(beken_timer_t));
  timer_apt->delay  = MSEC2TICK(time_ms);
  timer_apt->priv   = timer;
  timer_apt->repeat = true;

  timer->handle     = timer_apt;
  timer->function   = function;
  timer->arg        = arg;

  return BK_OK;
}

/****************************************************************************
 * Name: rtos_start_timer
 *
 * Description:
 *   Starts a RTOS timer.
 *
 * Input Parameters:
 *   timer    - A pointer of the timer
 *
 * Returned Value:
 *   Zero (BK_OK) if the semaphore was successfully created. Otherwise,
 *   return -1 (BK_FAIL)
 *
 ****************************************************************************/

bk_err_t rtos_start_timer(beken_timer_t *timer)
{
  int ret;
  struct timer_adpt *timer_apt = (struct timer_adpt *)timer->handle;

  ret = wd_start(&timer_apt->wdog,
                  timer_apt->delay,
                  os_timer_callback,
                  (wdparm_t)timer_apt);
  if (ret != OK)
    {
      wlerr("ERROR: Failed to start timer:%d\n", ret);
    }

  return beken_errno_trans(ret);
}

/****************************************************************************
 * Name: rtos_stop_timer
 *
 * Description:
 *   Stop a RTOS timer.
 *
 * Input Parameters:
 *   timer    - A pointer of the timer
 *
 * Returned Value:
 *   Zero (BK_OK)
 *
 ****************************************************************************/

bk_err_t rtos_stop_timer(beken_timer_t *timer)
{
  int ret = OK;
  struct timer_adpt *timer_apt;

  if (timer && timer->handle)
    {
      timer_apt =  (struct timer_adpt *)timer->handle;
      if (WDOG_ISACTIVE(&timer_apt->wdog))
        {
          ret = wd_cancel(&timer_apt->wdog);
          if (ret != OK)
            {
              wlerr("WARN: Failed to cancel timer:%d\n", ret);
            }
        }
    }

  return BK_OK;
}

/****************************************************************************
 * Name: rtos_reload_timer
 *
 * Description:
 *   Restart a RTOS timer.
 *
 * Input Parameters:
 *   timer    - A pointer of the timer
 *
 * Returned Value:
 *   Zero (BK_OK)
 *
 ****************************************************************************/

bk_err_t rtos_reload_timer(beken_timer_t *timer)
{
  rtos_stop_timer(timer);
  rtos_start_timer(timer);

  return OK;
}

/****************************************************************************
 * Name: rtos_deinit_timer
 *
 * Description:
 *   Destroy a RTOS timer.
 *
 * Input Parameters:
 *   timer    - A pointer of the timer
 *
 * Returned Value:
 *   Zero (BK_OK)
 *
 ****************************************************************************/

bk_err_t rtos_deinit_timer(beken_timer_t *timer)
{
  rtos_stop_timer(timer);
  kmm_free(timer->handle);

  return OK;
}

/****************************************************************************
 * Name: rtos_is_timer_init
 *
 * Description:
 *   Check if an RTOS timer is initilaized .
 *
 * Input Parameters:
 *   timer    - A pointer of the timer
 *
 * Returned Value:
 *   true if the timer is intiliazed. Otherwise, retutrn false.
 *
 ****************************************************************************/

bool rtos_is_timer_init(beken_timer_t *timer)
{
  return (timer->handle) ? true : false;
}

/****************************************************************************
 * Name: rtos_is_timer_init
 *
 * Description:
 *   Check if an RTOS timer is running.
 *
 * Input Parameters:
 *   timer    - A pointer of the timer
 *
 * Returned Value:
 *   true if the timer is running. Otherwise, retutrn false.
 *
 ****************************************************************************/

bool rtos_is_timer_running(beken_timer_t *timer)
{
  struct timer_adpt *timer_apt;

  timer_apt =  timer->handle;

  return (wd_gettime(&timer_apt->wdog) > 0);
}

/****************************************************************************
 * Name: Initialize a RTOS timer
 *
 * Description:
 *   Create one timer
 *   Timer does not start running until beken_start_timer is called
 *
 * Input Parameters:
 *   timer    - A pointer of the timer handler
 *   time_ms  - Timer period in milliseconds
 *   function - The callback handler function when timeout
 *   larg     - One argument that will be passed to the callback function
 *   rarg     - Another argument that will be passed to the callback function
 *
 * Returned Value:
 *   Zero (BK_OK) if the semaphore was successfully created. Otherwise,
 *   return -1 (BK_FAIL)
 *
 ****************************************************************************/

bk_err_t rtos_init_oneshot_timer(beken2_timer_t *timer, uint32_t time_ms,
                                timer_2handler_t function, void *larg, void *rarg)

{
  struct timer_adpt *timer_apt = NULL;

  timer_apt = kmm_malloc(sizeof(struct timer_adpt));
  if (!timer_apt)
    {
      wlerr("ERROR: Failed to malloc struct timer_adpt\n");
      return BK_FAIL;
    }

  memset(timer_apt ,0x0, sizeof(struct timer_adpt));
  memset(timer,     0x0, sizeof(beken2_timer_t));

  timer_apt->delay  = MSEC2TICK(time_ms);
  timer_apt->priv   = timer;
  timer_apt->repeat = false;

  timer->handle     = timer_apt;
  timer->function   = function;
  timer->left_arg   = larg;
  timer->right_arg  = rarg;

  return BK_OK;
}

/****************************************************************************
 * Name: rtos_start_oneshot_timer
 *
 * Description:
 *   Starts a one-shot RTOS timer.
 *
 * Input Parameters:
 *   timer    - A pointer of the timer
 *
 * Returned Value:
 *   Zero (BK_OK) if the semaphore was successfully created. Otherwise,
 *   return -1 (BK_FAIL)
 *
 ****************************************************************************/

bk_err_t rtos_start_oneshot_timer(beken2_timer_t *timer)
{
  int ret;
  struct timer_adpt *timer_apt = (struct timer_adpt *)timer->handle;

  ret = wd_start(&timer_apt->wdog,
                  timer_apt->delay,
                  os_timer_callback,
                  (wdparm_t)timer_apt);
  if (ret != OK)
    {
      wlerr("ERROR: Failed to start timer:%d\n", ret);
    }

  return beken_errno_trans(ret);
}

/****************************************************************************
 * Name: rtos_stop_oneshot_timer
 *
 * Description:
 *   Stop a one-shot RTOS timer.
 *
 * Input Parameters:
 *   timer    - A pointer of the timer
 *
 * Returned Value:
 *   Zero (BK_OK)
 *
 ****************************************************************************/

bk_err_t rtos_stop_oneshot_timer(beken2_timer_t *timer)
{
  int ret = OK;
  struct timer_adpt *timer_apt;

  if (timer && timer->handle)
    {
      timer_apt = (struct timer_adpt *)timer->handle;
      if (WDOG_ISACTIVE(&timer_apt->wdog))
        {
          ret = wd_cancel(&timer_apt->wdog);
          if (ret != OK)
            {
              wlerr("WARN: Failed to stop one-shot timer:%d\n", ret);
            }
        }
    }

  return BK_OK;
}

/****************************************************************************
 * Name: rtos_oneshot_reload_timer
 *
 * Description:
 *   Restart a one-shot RTOS timer.
 *
 * Input Parameters:
 *   timer    - A pointer of the timer
 *
 * Returned Value:
 *   Zero (BK_OK)
 *
 ****************************************************************************/

bk_err_t rtos_oneshot_reload_timer(beken2_timer_t *timer)
{
  rtos_stop_oneshot_timer(timer);
  rtos_start_oneshot_timer(timer);

  return OK;
}

/****************************************************************************
 * Name: rtos_change_period
 *
 * Description:
 *   Restart a one-shot RTOS timer.
 *
 * Input Parameters:
 *   timer    - A pointer of the timer
 *
 * Returned Value:
 *   Zero (BK_OK)
 *
 ****************************************************************************/
bk_err_t rtos_change_period(beken_timer_t* timer, uint32_t time_ms)
{
  struct timer_adpt *timer_apt = (struct timer_adpt *)timer->handle;

  rtos_stop_timer(timer);

  timer_apt->delay = time_ms;

  rtos_start_timer(timer);

  return OK;
}

/****************************************************************************
 * Name: rtos_oneshot_reload_timer_ex
 *
 * Description:
 *   Restart a one-shot RTOS timer.
 *
 * Input Parameters:
 *   timer    - A pointer of the timer
 *
 * Returned Value:
 *   Zero (BK_OK)
 *
 ****************************************************************************/

bk_err_t rtos_oneshot_reload_timer_ex(beken2_timer_t *timer, uint32_t time_ms,
                                        timer_2handler_t function, void *larg, void *rarg)
{
  struct timer_adpt *timer_apt = (struct timer_adpt *)timer->handle;

  rtos_stop_oneshot_timer(timer);

  timer_apt->delay  = MSEC2TICK(time_ms);
  timer_apt->priv   = timer;
  timer_apt->repeat = false;

  timer->handle     = timer_apt;
  timer->function   = function;
  timer->left_arg   = larg;
  timer->right_arg  = rarg;

  rtos_start_oneshot_timer(timer);

  return OK;
}

/****************************************************************************
 * Name: rtos_deinit_oneshot_timer
 *
 * Description:
 *   Destroy a one-shot RTOS timer.
 *
 * Input Parameters:
 *   timer    - A pointer of the timer
 *
 * Returned Value:
 *   Zero (BK_OK)
 *
 ****************************************************************************/

bk_err_t rtos_deinit_oneshot_timer(beken2_timer_t *timer)
{
  rtos_stop_oneshot_timer(timer);
  kmm_free(timer->handle);

  return BK_OK;
}

/****************************************************************************
 * Name: os_malloc
 *
 * Description:
 *   Allocate memory from heap.
 *
 * Input Parameters:
 *   size    - The number in bytes needed
 *
 * Returned Value:
 *   The pointer of the memory. Return NULL if the memory is not enough.
 *
 ****************************************************************************/

void *os_malloc(size_t size)
{
  void * p = NULL;

  p = kmm_malloc(size);
  if (!p)
    {
      wlerr("ERROR: Failed to malloc %d\n", size);
    }

  return p;
}

/****************************************************************************
 * Name: os_free
 *
 * Description:
 *   Free memory to heap.
 *
 * Input Parameters:
 *   ptr - The pointer of the memory
 *
 * Returned Value:
 *   None
 *
 ****************************************************************************/

void os_free(void *ptr)
{
  kmm_free(ptr);
}

/****************************************************************************
 * Name: os_zalloc
 *
 * Description:
 *   Allocate memory from heap and zero it.
 *
 * Input Parameters:
 *   size    - The number in bytes needed
 *
 * Returned Value:
 *   The pointer of the memory. Return NULL if the memory is not enough.
 *
 ****************************************************************************/

void *os_zalloc(size_t size)
{
  return kmm_zalloc(size);
}

/****************************************************************************
 * Name: os_malloc_debug / os_free_debug
 *
 * Description:
 *   Debug memory allocators used by prebuilt armino libs (e.g. libpsa_mbedtls)
 *   that were built with CONFIG_MEM_DEBUG enabled.  Delegate to the normal
 *   heap APIs and ignore the debug location parameters.
 *
 ****************************************************************************/

void *os_malloc_debug(const char *func_name, int line, size_t size,
                      int need_zero)
{
  (void)func_name;
  (void)line;

  return need_zero ? os_zalloc(size) : os_malloc(size);
}

void os_free_debug(const char *func_name, int line, void *ptr)
{
  (void)func_name;
  (void)line;

  os_free(ptr);
}

/****************************************************************************
 * Name: os_strdup
 *
 * Description:
 *   Duplicate a string using heap memory.
 *
 ****************************************************************************/

char *os_strdup(const char *s)
{
  size_t len;
  char *d;

  if (s == NULL)
    {
      return NULL;
    }

  len = strlen(s) + 1;
  d = os_malloc(len);
  if (d == NULL)
    {
      return NULL;
    }

  memcpy(d, s, len);
  return d;
}

/****************************************************************************
 * Name: os_realloc
 *
 * Description:
 *   Reallocoate more memory from heap.
 *
 * Input Parameters:
 *   ptr  - the memory malloced before
 *   size - new size
 *
 * Returned Value:
 *   The pointer of the memory. Return NULL if the memory is not satisfied.
 *
 ****************************************************************************/

void *os_realloc(void *ptr, size_t size)
{
  return kmm_realloc(ptr, size);
}

/****************************************************************************
 * Name: os_memcmp
 *
 * Description:
 *   Wrapper for memcmp.Compare the two memory areas
 *
 * Input Parameters:
 *   s1 - One memory
 *   s2 - Another memory
 *   n  - the length to compare
 *
 * Returned Value:
 *   Zero if the two memory areas are same. Otherwise, return non-zero
 *
 ****************************************************************************/

INT32 os_memcmp(const void *s1, const void *s2, UINT32 n)
{
	return memcmp(s1, s2, (unsigned int)n);
}

/****************************************************************************
 * Name: os_memmove
 *
 * Description:
 *   Wrapper for memmove.
 *
 * Input Parameters:
 *   in  - The source address
 *   out - The dest address
 *   n   - The length to move
 *
 * Returned Value:
 *   The pointer of the dest address
 *
 ****************************************************************************/
 
void *os_memmove(void *out, const void *in, UINT32 n)
{
	return memmove(out, in, n);
}

/****************************************************************************
 * Name: os_memcpy
 *
 * Description:
 *   Wrapper for memcpy.
 *
 * Input Parameters:
 *   in  - The source address
 *   out - The dest address
 *   n   - The length to move
 *
 * Returned Value:
 *   The pointer of the dest address
 *
 ****************************************************************************/

void *os_memcpy(void *out, const void *in, UINT32 n)
{
	return memcpy(out, in, n);
}

/****************************************************************************
 * Name: os_memset
 *
 * Description:
 *   Wrapper for memset.
 *
 * Input Parameters:
 *   b   - The memory address
 *   c   - the content to set
 *   len - The length to set
 *
 * Returned Value:
 *   The pointer of the memory address
 *
 ****************************************************************************/

int os_memcmp_const(const void *a, const void *b, size_t len)
{
  return memcmp(a, b, len);
}

/****************************************************************************
 * Name: os_memset
 *
 * Description:
 *   Wrapper for memset.
 *
 * Input Parameters:
 *   b   - The memory address
 *   c   - the content to set
 *   len - The length to set
 *
 * Returned Value:
 *   The pointer of the memory address
 *
 ****************************************************************************/

void *os_memset(void *b, int c, UINT32 len)
{
	return (void *)memset(b, c, (unsigned int)len);
}

/****************************************************************************
 * Name: rtos_delay_milliseconds
 *
 * Description:
 *   Yield CPU for some time.
 *
 * Input Parameters:
 *   num_ms - A time in Milliseconds
 *
 * Returned Value:
 *   Zero (BK_OK)
 *
 ****************************************************************************/

bk_err_t rtos_delay_milliseconds(uint32_t num_ms)
{

  nxsig_usleep(num_ms * 1000);
  return BK_OK;
}

#if 0
/****************************************************************************
 * Name: bk_printf_ext
 *
 * Description:
 *   Synchronous printing in Beken SDK
 *
 * Input Parameters:
 *   level - Print level
 *   tag   - tag
 *   fmt   - parameters
 *
 * Returned Value:
 *   None
 * 
 * In the future, the printing functionality of the Beken SDK will be 
 * adapted to integrate with Vela's printing.
 *
 ****************************************************************************/

void bk_printf_ext(int level, char * tag, const char *fmt, ...)
{
  /* ToDo */
}

/****************************************************************************
 * Name: bk_printf_raw
 *
 * Description:
 *   Formatted print in Beken SDK.
 *
 * Input Parameters:
 *   level - Print level
 *   tag   - tag
 *   fmt   - parameters
 *
 * Returned Value:
 *   None
 *
 ****************************************************************************/

void bk_printf_raw(int level, char *tag, const char *fmt, ...)
{
  /* ToDo */
}

/****************************************************************************
 * Name: bk_printf
 *
 * Description:
 *   Print in Beken SDK.
 *
 * Input Parameters:
 *   fmt   - parameters
 *
 * Returned Value:
 *   None
 *
 ****************************************************************************/

void bk_printf(const char *fmt, ...)
{
  printf(fmt);
}
#endif

/****************************************************************************
 * Name: rtos_is_in_interrupt_context
 *
 * Description:
 *   Check if it's in interrupt state.
 *
 * Input Parameters:
 *   None
 *
 * Returned Value:
 *   True if the CPU is currently executing in interrupt context.
 *
 ****************************************************************************/
bool rtos_is_in_interrupt_context(void)
{
  return up_interrupt_context();
}

/****************************************************************************
 * Name: bk_get_tick
 *
 * Description:
 *   Get the tick counters since power up.
 *
 * Input Parameters:
 *   None
 *
 * Returned Value:
 *   The tick counters
 *
 ****************************************************************************/
uint64_t bk_get_tick(void)
{
  clock_t t = clock();
  return t;
}

/****************************************************************************
 * Name: bk_get_ticks_per_second
 *
 * Description:
 *   Get the number of ticks per second
 *
 * Input Parameters:
 *   None
 *
 * Returned Value:
 *   The number of ticks per second
 *
 ****************************************************************************/
uint32_t bk_get_ticks_per_second(void)
{
  return 1000000/CONFIG_USEC_PER_TICK;
}

/****************************************************************************
 * Name: rtos_get_total_heap_size
 *
 * Description:
 *   Get total heap size
 *
 * Input Parameters:
 *   None
 *
 * Returned Value:
 *   The total heap size
 *
 ****************************************************************************/

size_t rtos_get_total_heap_size(void)
{
  struct mallinfo info;

  info = kmm_mallinfo();
  return (info.fordblks + info.uordblks);
}

/****************************************************************************
 * Name: rtos_get_free_heap_size
 *
 * Description:
 *   Get total free heap size
 *
 * Input Parameters:
 *   None
 *
 * Returned Value:
 *   The total free heap size
 *
 ****************************************************************************/

size_t rtos_get_free_heap_size(void)
{
  struct mallinfo info;

  info = kmm_mallinfo();
  return info.fordblks;
}

/****************************************************************************
 * Name: rtos_get_minimum_free_heap_size
 *
 * Description:
 *   Get the ever number of minimum free space
 *
 * Input Parameters:
 *   None
 *
 * Returned Value:
 *   The minimum free space
 *
 ****************************************************************************/

size_t rtos_get_minimum_free_heap_size(void)
{
  struct mallinfo info;

  info = kmm_mallinfo();
  return (info.fordblks + info.uordblks - info.usmblks);
}

/****************************************************************************
 * Name: bk_get_second
 *
 * Description:
 *   Gets time in seconds since RTOS starts.
 *
 * Input Parameters:
 *   None
 *
 * Returned Value:
 *   Time in seconds since RTOS starts.
 *
 ****************************************************************************/

uint64_t bk_get_second(void)
{
  struct timespec ts;

  clock_systime_timespec(&ts);

  return ts.tv_sec;
}

/****************************************************************************
 * Private Data
 ****************************************************************************/

/****************************************************************************
 * Public Data
 ****************************************************************************/

/****************************************************************************
 * Private Functions and Public Functions only used by libraries
 ****************************************************************************/


