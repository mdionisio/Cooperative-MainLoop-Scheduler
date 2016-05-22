/*
 *   This file is part of Cooperative-Mainloop-Scheduler.
 *
 *   Cooperative-Mainloop-Scheduler is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU Lesser General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 *   Cooperative-Mainloop-Scheduler is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU Lesser General Public License for more details.
 *
 *   You should have received a copy of the GNU Lesser General Public License
 *   along with Cooperative-Mainloop-Scheduler.  If not, see <http://www.gnu.org/licenses/>.
 */

/*
 * File:   task.h
 * Author: Michele Dionisio
 *
 * Created on 18 October 2014
 */

#ifndef __TASK_H__
#define __TASK_H__

#ifdef __cplusplus
extern "C" {
#endif

    // if you build with -std=c99 you need the following header
    // otherwise ucontext.h will be different
    // if you uild with -std=gnu99 everything is ok
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#else
#if _POSIX_C_SOURCE < 200809L
#error require posix compatibility up to 200809L
#endif
#endif

#ifndef _XOPEN_SOURCE
#define _XOPEN_SOURCE 700
#else
#if _XOPEN_SOURCE < 700
#error require _XOPEN_SOURCE compatibility up to 700
#endif
#endif

#include <stdlib.h>

#include "lconfig.h"

    //used to have functions for context management
#include <ucontext.h>

#include <inttypes.h>

#ifdef USING_SPLIT_STACK
    /* FIXME: These are not declared anywhere.  */
#define SPIT_STACK_NUMBER_OFFSETS 10
#endif

    // *****************************************************************************
    // *                        TASK/SCHEDULER DEFINITION                          *
    // *****************************************************************************

    typedef unsigned int tid_t; /**< type for unique task identifier */
    typedef unsigned int sid_t; /**< type for unique scheduler identifier */

    typedef struct task task_t; /**< task type */
    typedef struct sched sched_t; /**< scheduler type */

    typedef void (*taskFunc_t)(); /**< prototype for any task functions */

    /** task status type
     */
    typedef enum taskStatus {
        RUNNING = 0, /**< task is running */
        TERMINATED, /**< task terminated and waiting to be removed from available tasks */
        WAIT, /**< task is waiting to be scheduled */
        SUSPENDED /**< task suspended from the scheduler */
    } taskStatus_t;

    /** task data type
     */
    struct task {
        tid_t tid; /**< unique thread identifier */
        volatile taskStatus_t status; /**< task status */
        int returnValue; /**< return value valid if status == TERMINATED */
        taskFunc_t taskFunc; /**< pointer to the task function */
        sched_t *sched; /**< scheduler that manage this task */
        ucontext_t tempData; /**< structure to save task data when interrupted */
        void * data; /** taskParameter */

        unsigned int config; /**< configuration */
        volatile unsigned int stop; /**< true if ask to close */

        enum {
            T_DYNAMIC, T_STATIC
        } memory; /**< if STATIC memory will be not free on destroy */

        size_t stack_size; /**< stack size */
#ifdef USING_SPLIT_STACK
        void *stack_context[SPIT_STACK_NUMBER_OFFSETS]; /**< split task context */
        uint8_t * stack; /**< task stack */
#else
        uint8_t stack[0]; /**< task stack */
#endif
    };

    typedef struct queueTask queueTask_t; /**< task queue type */

    /** element of task list used as task queue
     */
    struct queueTask {
        task_t *task; /**< element */
        queueTask_t *prev; /**< needed for a doubly-linked list only */
        queueTask_t *next; /**< needed for singly- or doubly-linked lists */
    };

#define SCHED_NONE                (0)
#define SCHED_END_WITHOUT_TASKS   (1)
#define SCHED_END_WITHOUT_PENDIG  (2)


#define SCHED_LIBEVENT            (8)
#define SCHED_LIBUV               (16)


#define TASK_NONE                 (0)
#define TASK_CREATE_NOT_READY     (1)

#define ERR_NO                     (0)
#define ERR_ALREADY_RUNNING        (-1)
#define ERR_ALREADY_READY          (-2)
#define ERR_ALREADY_TERMINATED     (-3)
#define ERR_ALREADY_WAITING        (-4)
#define ERR_NO_SCHEDULER_AVAILABLE (-10)

    /** scheduler type
     */
    struct sched {
        sid_t sid; /**< unique number to identify scheduler */
        unsigned int numAvailableTask; /**< number of task managed */
        unsigned int config; /**< configuration */
        unsigned int stop; /**< true if ask to close */
        unsigned int running; /**< true mainloop is running */
        queueTask_t *readyTask; /**< list of ready to run task  (unused as queue) */
        ucontext_t tempData; /**< structure to save task data when  interrupted */
#ifdef USING_SPLIT_STACK
        void *stack_context[SPIT_STACK_NUMBER_OFFSETS]; /**< split task context */
#endif

        const struct sched_mainloop_interface *mainloop;
        void *base_loop;
    };

    // task
    void setReturnValue_task(int value);

    void yield_task();

    void wait_task(tid_t tid);

    void suspend_task();

    int schedule_task(task_t * const task);

    static inline void schedule_task_void(task_t * const task) {
        schedule_task(task);
    };

    unsigned int isStopRequested_task();

    void stopRequest_task(task_t * const task);

    // schedule
    sched_t * create_sched(unsigned int config);

    void destroy_sched(sched_t * * const sched);

    tid_t new_task_sched(sched_t * const sched,
            taskFunc_t ltaskFunc,
            unsigned int config,
            size_t stacksize);

    void mainloop_sched(sched_t * const sched);

    unsigned int isStopRequested_sched(const sched_t * const sched);

    void stopRequest_sched(sched_t * const sched);

    int is_running_sched(sched_t * const sched);

    // untiles
    task_t * getTask(tid_t tid);

    size_t getStackSize(const task_t *);

#ifdef THREADSAFE_SCHED
#define task_thread_local __thread
#else
#define task_thread_local
#endif

    extern task_thread_local task_t * THIS_TASK;

#define THIS_TID THIS_TASK->tid

#define RETURN_TASK(x) setReturnValue_task(x); return

#define YIELD yield_task()
#define WAITTASK(x) wait_task(x)
#define STOP_REQUEST(tid) stopRequest_task(getTask(tid))
#define SUSPEND suspend_task()
#define RESUME(tid) schedule_task(getTask(tid))
#define ISSTOPREQUESTTASK isStopRequested_task()
#define THIS_SCHED THIS_TASK->sched
#define THIS_SCHED_BASE THIS_SCHED->base_loop

#define RESUMEP(ptid) schedule_task(ptid)

    typedef void (*iterOnTask)(task_t * const task);
    void iterAllTask(iterOnTask f);
    void iterSchedTask(iterOnTask f, const sched_t * const sched);

    int schedInit();
    int schedDeInit();

    int schedIsThreadSafe();

#ifdef __cplusplus
}
#endif

#endif /* __TASK_H__ */
