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
 * File:   task.c
 * Author: Michele Dionisio
 *
 * Created on 18 October 2014
 */

#include "task.h"
#include "task_loop_private.h"
#include "likely.h"

#include "taskev.h"
#include "taskuv.h"

#include <assert.h>

////////////////////////////////////////////////////////////////////////////////
#ifndef DEBUG_SCHED_TASK
#define FAKELOG
#endif

/// Log Macro

#ifndef __FILENAME__
#define __FILENAME__ "task.c"
#endif

#ifdef USING_SPLIT_STACK

/* FIXME: These are not declared anywhere.  */
#define SPIT_STACK_NUMBER_OFFSETS 10

extern void __splitstack_getcontext(void *context[SPIT_STACK_NUMBER_OFFSETS]);

extern void __splitstack_setcontext(void *context[SPIT_STACK_NUMBER_OFFSETS]);

extern void *__splitstack_makecontext(size_t, void *context[SPIT_STACK_NUMBER_OFFSETS], size_t *);

extern void __splitstack_releasecontext(void *context[10]);

extern void * __splitstack_resetcontext(void *context[SPIT_STACK_NUMBER_OFFSETS], size_t *);

extern void __splitstack_block_signals(int *, int *);

extern void __splitstack_block_signals_context(void *context[SPIT_STACK_NUMBER_OFFSETS], int *, int *);

static int
swap(ucontext_t *x, void *x1[SPIT_STACK_NUMBER_OFFSETS], ucontext_t *y, void *y1[SPIT_STACK_NUMBER_OFFSETS]) {
    __splitstack_getcontext(x1);
    __splitstack_setcontext(y1);
    int res = swapcontext(x, y);
    __splitstack_setcontext(x1);
    return res;
}

#define TASK_SWAP_CONTEXT(x,y) swap(&(x->tempData), x->stack_context, &(y->tempData), y->stack_context)

#else
#define TASK_SWAP_CONTEXT(x,y) swapcontext(&(x->tempData),&(y->tempData))
#endif

#define TASK_GET_CONTEXT(x) getcontext(x)
#define TASK_MAKE_CONTEXT(x,y,...) makecontext(x,y, ##__VA_ARGS__)

#include "logmacro.h"

////////////////////////////////////////////////////////////////////////////////

#include <stdlib.h>

// used for list and map management
#include "utlist_ex.h"
#include "../lib/uthash/src/utlist.h"
#include "../lib/uthash/src/uthash.h"

#ifdef THREADSAFE_SCHED
#include <pthread.h>
#endif

// used for error management
#include <errno.h>
#include <string.h>

// used to get stack size
#include <unistd.h>
#include <sys/resource.h>

// *****************************************************************************
// *                        TASK/SCHEDULER DEFINITION                          *
// *****************************************************************************

static tid_t gTaskId = 0; /**< global variable to give unique id to any task */
static tid_t gSchedId = 0; /**< global variable to give unique id to any scheduler */

/** map type for from sid to sched
 */
typedef struct mapSched mapSched_t;

/** map scheduler
 */
struct mapSched {
    sid_t sid; /**< map key */
    sched_t * sched; /**< map value */
    UT_hash_handle hh; /**< makes this structure hashable */
};

typedef struct elemTaskList elemTaskList_t;

struct elemTaskList {
    task_t *task;
    elemTaskList_t *next;
};

/** map type for from tid to task
 */
typedef struct mapTask mapTask_t;

/** map data type for from tid to task
 */
struct mapTask {
    tid_t tid; /**< map key */
    task_t * task; /**< map value */
    elemTaskList_t *listWaiting; /**< list of task waiting the task's end */
    UT_hash_handle hh; /**< makes this structure hashable */
};

static mapTask_t * create_mapTask(task_t * const task) {
    mapTask_t * const newTask = (mapTask_t *) malloc(sizeof (mapTask_t));

    assert(NULL != newTask);

    newTask->task = task;
    newTask->tid = task->tid;
    newTask->listWaiting = NULL;
    return newTask;
}

static void destroy_task(task_t * * const task);

static void destroy_mapTask(mapTask_t * * const delTask) {
    assert(NULL != delTask);
    if (likely(NULL != *delTask)) {
        assert(NULL != (*delTask)->task);
        assert(NULL != (*delTask)->task->sched);

        elemTaskList_t *elIter = NULL;
        elemTaskList_t *elIterNext = NULL;

        LL_FOREACH_SAFE((*delTask)->listWaiting, elIter, elIterNext) {
            assert(NULL != elIter);

            schedule_task(elIter->task);
            LL_DELETE((*delTask)->listWaiting, elIter);
            free(elIter);
        }
        --((*delTask)->task->sched->numAvailableTask); // decrease task counter
        destroy_task(&((*delTask)->task)); // destroy task
        free(*delTask); // release memory
        *delTask = NULL;
    }
}

static mapSched_t *gMapSched = NULL; /**< define map for all available scheduler */
static mapTask_t *gAvailableTask = NULL; /**< define map for all available task */
#ifdef THREADSAFE_SCHED
static pthread_mutex_t gMutexAvailableTask = PTHREAD_MUTEX_INITIALIZER;
#define LOCK_AVAILABLE_TASK pthread_mutex_lock(&gMutexAvailableTask)
#define UNLOCK_AVAILABLE_TASK pthread_mutex_unlock(&gMutexAvailableTask)

static pthread_mutex_t gMutexAvailableSched = PTHREAD_MUTEX_INITIALIZER;
#define LOCK_AVAILABLE_SCHED pthread_mutex_lock(&gMutexAvailableSched)
#define UNLOCK_AVAILABLE_SCHED pthread_mutex_unlock(&gMutexAvailableSched)

#else
#define LOCK_AVAILABLE_TASK
#define UNLOCK_AVAILABLE_TASK

#define LOCK_AVAILABLE_SCHED
#define UNLOCK_AVAILABLE_SCHED
#endif

#define TASKMALLOC malloc
#define TASKFREE free


// *****************************************************************************
// *                          TASK IMPLEMENTATION                              *
// *****************************************************************************

/** Create a new task
 *
 * @param ltaskFunc function pointer to the function to call
 * @param lsched scheduler that is managing this task
 * @param config
 * @param memory if not NULL is the memory used to contain the task info and stacks
 * @param stacksize if memory is NULL this is the size of the STACK. if 0 default size will be used
 *                  if memory is not NULL is the size of the memory
 * @return return a valid task
 */
static task_t * create_task(taskFunc_t ltaskFunc,
        sched_t * const lsched,
        unsigned int config, void *memory, size_t stacksize) {
    assert(NULL != ltaskFunc);
    assert(NULL != lsched);
    task_t * task = NULL;
    if (likely(memory == NULL)) {
        if (stacksize == 0) {
            stacksize = SIGSTKSZ;
            /*
            struct rlimit rl;
            if (0 == getrlimit(RLIMIT_STACK, &rl)) {
                stacksize = rl.rlim_cur;
            }
             */
        }
        size_t min_stacksize = sysconf(_SC_THREAD_STACK_MIN);
        if (stacksize < min_stacksize) {
            stacksize = min_stacksize;
            LOG("stack size incremented to the minimum %zu", stacksize);
        }
#ifdef USING_SPLIT_STACK
        memory = TASKMALLOC(sizeof (task_t));
        assert(NULL != memory);

        task = (task_t *) memory;
        {
            size_t ret_stacksize = stacksize;
            task->stack = __splitstack_makecontext(stacksize,
                    &(task->stack_context[0]),
                    &ret_stacksize);
            stacksize = ret_stacksize;
        }

#else
        memory = TASKMALLOC(sizeof (task_t) + stacksize);

        assert(NULL != task->stack);
        assert(NULL != memory);

        task = (task_t *) memory;
#endif
        task->memory = T_DYNAMIC;
    } else {
#ifdef USING_SPLIT_STACK
        LOG("it is not possible to use static allocated stack with split statk");
        return NULL;
#else
        task = (task_t *) memory;
        task->memory = T_STATIC;
        assert(stacksize > sizeof (task_t));
        stacksize -= sizeof (task_t);
#endif
    }
    {
        LOCK_AVAILABLE_TASK; // safe access to gMutexAvailableTask
        task->tid = ++gTaskId; // set unique id
        UNLOCK_AVAILABLE_TASK; // safe access to gMutexAvailableTask
    }
    task->taskFunc = ltaskFunc; // set function pointer to call
    task->status = WAIT; // set task status
    task->returnValue = 0; // set default return value
    task->sched = lsched; // set scheduler where the task run
    task->data = NULL; // in data for thread
    task->stop = 0; // set request to stop to false
    task->config = config; // set configuration
    task->stack_size = stacksize;


    // initialize takein it from what is running now
    if (unlikely(TASK_GET_CONTEXT(&(task->tempData)) == -1)) {
        LOG("getcontext error (%d): %s", errno, strerror(errno));
    }

    // set the context where to go at the end
    task->tempData.uc_link = &(task->sched->tempData);
    // initialize where to save the stask
    task->tempData.uc_stack.ss_sp = task->stack;
    task->tempData.uc_stack.ss_size = stacksize;
    sigfillset(&(task->tempData.uc_sigmask)); // is used to store the set of signals blocked in the context

#ifdef USING_SPLIT_STACK
    {
        int dont_block_signals = 0;
        __splitstack_block_signals_context(&(task->stack_context[0]),
                &dont_block_signals, NULL);
    }
#endif

    TASK_MAKE_CONTEXT(&(task->tempData), (void (*)(void)) ltaskFunc, 0);

    return task;
}

task_thread_local task_t * THIS_TASK = NULL;

/** set return value and mark task as terminated
 *
 * @param value
 */
void setReturnValue_task(int value) {
    assert(NULL != THIS_TASK);
    THIS_TASK->returnValue = value;
    THIS_TASK->status = TERMINATED;
}

/** run or wakeup a task
 *
 * @param task
 */
static void run_task(task_t * const task) {
    assert(NULL != task);
    assert(NULL != task->sched);
    LOG("run_task tid=%u %d", task->tid, task->status);

    THIS_TASK = task;

    switch (task->status) {
        case WAIT:
            task->status = RUNNING;
            if (unlikely(TASK_SWAP_CONTEXT(task->sched, task) == -1)) {
                LOG("swap context error (%d): %s", errno, strerror(errno));
            }
            break;
        case RUNNING:
            // if you reach this point means that the task is terminated without call
            // setReturnValue_task. This is teorically a programming problem but it
            // can be easy managed and so ...
            LOG("missing setting of return value");
            setReturnValue_task(0);
            break;
        default:
            LOG("run task in not valid state %d", task->status);
    }

    THIS_TASK = NULL;

}

/** destroy a task
 *
 * @param task task to be destroyed
 */
static void destroy_task(task_t * * const task) {
    assert(NULL != task);
    if (likely(NULL != *task)) {
        // destroy data passed to start
        if (NULL != (*task)->data) {
            free((*task)->data);
            (*task)->data = NULL;
        }
        if ((*task)->memory == T_DYNAMIC) {

#ifdef USING_SPLIT_STACK
            __splitstack_releasecontext((*task)->stack_context);
            (*task)->stack = NULL;
#endif

            TASKFREE(*task);
        }
        *task = NULL;
    }
}

/** put a task in sleep and switch to the scheduler
 *
 */
void yield_task() {
    assert(NULL != THIS_TASK);
    assert(NULL != THIS_SCHED);
    THIS_TASK->status = WAIT;
    if (unlikely(TASK_SWAP_CONTEXT(THIS_TASK, THIS_SCHED) == -1)) {
        LOG("yield swap context error (%d): %s", errno, strerror(errno));
    }
    // if you are here means that you are waking up
}

/** wait
 *
 * @param tid  id of thread to wait end
 */
void wait_task(tid_t tid) {
    assert(NULL != THIS_TASK);
    assert(THIS_TID != tid);

    {
        LOCK_AVAILABLE_TASK; // safe access to gAvailableTask

        mapTask_t *__mapTask__ = NULL;
        HASH_FIND_INT(gAvailableTask, &(tid), __mapTask__);
        assert(NULL != __mapTask__);

        elemTaskList_t * const newWaitCond = (elemTaskList_t *) malloc(sizeof (elemTaskList_t));

        assert(NULL != newWaitCond);

        newWaitCond->next = NULL;
        newWaitCond->task = THIS_TASK;
        LL_PREPEND(__mapTask__->listWaiting, newWaitCond);

        UNLOCK_AVAILABLE_TASK; // safe access to gAvailableTask
    }

    suspend_task();
}

/** put a task in sleep and switch to the scheduler
 *
 */
void suspend_task() {
    assert(NULL != THIS_TASK);
    assert(NULL != THIS_SCHED);
    THIS_TASK->status = SUSPENDED;
    if (unlikely(TASK_SWAP_CONTEXT(THIS_TASK, THIS_SCHED) == -1)) {
        LOG("yield swapcontext error (%d): %s", errno, strerror(errno));
    }
    // if you are here means that you are waking up
}

unsigned int isStopRequested_task() {
    assert(NULL != THIS_TASK);
    return THIS_TASK->stop;
}

void stopRequest_task(task_t * const task) {
    assert(NULL != task);
    task->stop = 1;
}

///
/// SCHEDULER IMPLEMENTATION
///

/** Create a new scheduler
 *
 * @return return a valid scheduler
 */
sched_t * create_sched(unsigned int config) {
    sched_t * const sched = (sched_t *) malloc(sizeof (sched_t));
    assert(NULL != sched);

    mapSched_t * const newSched = (mapSched_t *) malloc(sizeof (mapSched_t));
    assert(NULL != newSched);

    LOCK_AVAILABLE_SCHED; // safe access to gAvailableSched
    sched->sid = ++gSchedId; // set scheduler id
    sched->numAvailableTask = 0; // set no available task
    sched->readyTask = NULL; // set empty list
    sched->config = config; // configure scheduler
    sched->stop = 0; // set stop to don't stop
    sched->running = 0; // set running to not running

    if (0 != (config & SCHED_LIBEVENT)) {
        sched->mainloop = &task_libevent;
    } else if (0 != (config & SCHED_LIBUV)) {
        sched->mainloop = &task_libuv;
    }// ADD HEAR OTHER MAINLOOP LIBRARY
    else {
        UNLOCK_AVAILABLE_SCHED; // safe access to gAvailableSched

        free(newSched);
        free(sched);

        return NULL;
    }


    if (unlikely(sched->mainloop == NULL)) {
        LOG("missing mainloop library");
    }
    if ((NULL != sched->mainloop) && (NULL != sched->mainloop->create)) {
        sched->base_loop = sched->mainloop->create();
    } else {
        sched->base_loop = NULL;
    }

    newSched->sid = sched->sid;
    newSched->sched = sched;
    HASH_ADD_INT(gMapSched, sched->sid, newSched);

    UNLOCK_AVAILABLE_SCHED; // safe access to gAvailableSched

    return sched;
}

unsigned int isStopRequested_sched(const sched_t * const sched) {
    assert(NULL != sched);
    return sched->stop;
}

void stopRequest_sched(sched_t * const sched) {
    assert(NULL != sched);
    if (sched->stop == 0) {
        if ((NULL != sched->mainloop) && (NULL != sched->mainloop->stop)) {
            if (NULL != sched->base_loop) {
                sched->mainloop->stop(sched->base_loop);
            }
        }
    }
    sched->stop = 1;
}

/** destroy a scheduler
 *
 * @param sched scheduler to be destroyed
 */
void destroy_sched(sched_t * * const sched) {
    assert(NULL != sched);
    if (likely(NULL != *sched)) {
        stopRequest_sched(*sched);
        LOCK_AVAILABLE_SCHED; // safe access to gAvailableSched
        mapSched_t * delSched = NULL;
        HASH_FIND_INT(gMapSched, &((*sched)->sid), delSched);
        assert(NULL != delSched);
        HASH_DEL(gMapSched, delSched);
        assert((*sched) == (delSched->sched));
        free(delSched);
        UNLOCK_AVAILABLE_SCHED; // safe access to gAvailableSched
        if ((NULL != (*sched)->mainloop) && (NULL != (*sched)->mainloop->destroy) && (NULL != (*sched)->base_loop)) {
            (*sched)->mainloop->destroy((*sched)->base_loop);
        }
        (*sched)->base_loop = NULL;
        free(*sched);
        *sched = NULL;
    }
}

/** put a task in the list of ready to run
 *
 * @param task task ready
 */
int schedule_task(task_t * const task) {
    assert(NULL != task);

    // already running
    if (unlikely(task->status == RUNNING)) {
        return ERR_ALREADY_RUNNING;
    }

    if (unlikely(task->status == TERMINATED)) {
        return ERR_ALREADY_TERMINATED;
    }


    if (likely(NULL != task->sched)) { // if exist scheduler check if the task is already
        // plan  to be scheduled.
        // In that case nothing has to be done
        queueTask_t * elIter = NULL;

        CDL_FOREACH(task->sched->readyTask, elIter) {
            assert(NULL != elIter);
            if (unlikely(elIter->task == task)) {
                return ERR_ALREADY_READY; // task already in the ready list
            }
        }
    }

    task->status = WAIT;

    if (likely(NULL != task->sched)) {
        // create new element for ready list
        queueTask_t * const newElem = (queueTask_t *) malloc(sizeof (queueTask_t));
        assert(NULL != newElem);

        newElem->task = task;

        // add task to the ready list
        CDL_PREPEND(task->sched->readyTask, newElem);

        return ERR_NO;
    } else {
        return ERR_NO_SCHEDULER_AVAILABLE;
    }

}

/** Create a new schedulable task
 *
 * @param sched scheduler
 * @param ltaskFunc function fointer to the task function
 * @param config task config
 * @param stacksize stacksize (0 means default)
 */
tid_t new_task_sched(sched_t * const sched,
        taskFunc_t ltaskFunc,
        unsigned int config, size_t stacksize) {
    assert(NULL != sched);
    assert(NULL != ltaskFunc);
    // create new element for available map
    mapTask_t * const newTask = create_mapTask(create_task(ltaskFunc, sched, config, NULL, stacksize));
    assert(NULL != newTask);

    //
    LOG("-> newtask ttid=%u", newTask->tid);

    {
        LOCK_AVAILABLE_TASK; // safe access to gAvailableTask
        // add new element to the map
        HASH_ADD_INT(gAvailableTask, tid, newTask);
        ++(sched->numAvailableTask);
        UNLOCK_AVAILABLE_TASK; // safe access to gAvailableTask
    }

    // schede created task it it is not have TASK_CREATE_NOT_READY property
    if ((config & TASK_CREATE_NOT_READY) == 0) {
        schedule_task(newTask->task);
    }

    return newTask->tid;
}

/** scheduler mainloop
 *
 * @param sched
 */
void mainloop_sched(sched_t * const sched) {
    assert(NULL != sched);
    queueTask_t * task = NULL;

    mapTask_t *delTask = NULL; // tmp variable to delete terminated task

#ifdef USING_SPLIT_STACK
    int block = 0;
    __splitstack_block_signals(&block, NULL);
#endif

    LOCK_AVAILABLE_SCHED;
    sched->running = 1;
    UNLOCK_AVAILABLE_SCHED;

    // loop while there is available task

    while (((!isStopRequested_sched(sched)) ||
            ((sched->numAvailableTask >= 0) && ((sched->config & SCHED_END_WITHOUT_PENDIG) != 0))) && // is not stop requested
            !(((sched->config & SCHED_END_WITHOUT_TASKS) != 0) && // is configured to end if there is no task
            (sched->numAvailableTask == 0))) { // if there is some task running
        if (likely(NULL == task)) {
            // get a task that is waiting to run
            CDL_POP_LAST(sched->readyTask, task);
        }

        if (unlikely(NULL == task)) { // there is no task available to be scheduled
            LOG("no task ready to be scheduled (sched %p) (n. task %u)", sched, sched->numAvailableTask);
            // task dispaching
            if (likely((NULL != sched->mainloop) &&
                    (sched->mainloop->event_poll != NULL))) {
                sched->mainloop->event_poll(sched->base_loop);
            }
        } else { // there is a task that can be scheduled
            assert(NULL != task->task);
            LOG("scheduler work on task ttid=%u status=%d",
                    task->task->tid, task->task->status);
            int wakeup = 0;
            // loop to check status before and after a task execution
            do {
                switch (task->task->status) {
                    case TERMINATED:
                        // task is terminated so has to be removed
                        // from the list of available task
                        LOG("task(%u) terminated with return: %d "
                                "so remove it from availale task",
                                task->task->tid,
                                task->task->returnValue);
                        delTask = NULL;

                    {
                        LOCK_AVAILABLE_TASK; // safe access to gAvailableTask
                        HASH_FIND_INT(gAvailableTask, &(task->task->tid), delTask);
                        assert(delTask != NULL);
                        HASH_DEL(gAvailableTask, delTask);
                        UNLOCK_AVAILABLE_TASK; // safe access to gAvailableTask
                    }

                        destroy_mapTask(&delTask);
                        free(task);
                        task = NULL;
                        wakeup = 0; // stop check status
                        break;
                    case WAIT:
                        if (0 == wakeup) {
                            run_task(task->task);
                            wakeup = 1; // check status again
                        } else {
                            CDL_PREPEND(sched->readyTask, task);
                            wakeup = 0; // stop check status
                        }
                        break;
                    case RUNNING:
                        LOG("task ttid=%u terminate without call setReturnValue_task", task->task->tid);
                        task->task->returnValue = 0;
                        task->task->status = TERMINATED;
                        CDL_PREPEND(sched->readyTask, task);
                        wakeup = 0; // stop check status
                        break;
                    case SUSPENDED:
                        free(task);
                        task = NULL;
                        break;
                }
            } while ((NULL != task) && (0 != wakeup));

            task = NULL; // it is important to force next loop to choose another task
        }
    } // end mainloop

    LOG("end loop condition: isStopRequested_sched->%d", isStopRequested_sched(sched));
    LOG("end loop condition: numAvailableTask->%u", sched->numAvailableTask);
    LOG("end loop condition: SCHED_END_WITHOUT_PENDIG->%d", ((sched->config & SCHED_END_WITHOUT_PENDIG) != 0));
    LOG("end loop condition: SCHED_END_WITHOUT_TASKS->%d", ((sched->config & SCHED_END_WITHOUT_TASKS) != 0));

    LOCK_AVAILABLE_SCHED;
    sched->running = 0;
    UNLOCK_AVAILABLE_SCHED;
}

int is_running_sched(sched_t * const sched) {
    int retval;

    LOCK_AVAILABLE_SCHED;
    retval = sched->running;
    UNLOCK_AVAILABLE_SCHED;

    return sched->running;
}

task_t * getTask(tid_t tid) {
    mapTask_t *__mapTask__ = NULL;
    {
        LOCK_AVAILABLE_TASK; // safe access to gAvailableTask
        HASH_FIND_INT(gAvailableTask, &(tid), __mapTask__);
        UNLOCK_AVAILABLE_TASK; // safe access to gAvailableTask
    }
    if (likely(NULL != __mapTask__)) {
        return __mapTask__->task;
    } else {
        return NULL;
    }
}

size_t getStackSize(const task_t *task) {
    assert(NULL != task);
    return task->stack_size;
}

/** iterate over all the task available
 *
 * @param f function to execute over each thread
 */
void iterAllTask(iterOnTask f) {
    static mapTask_t *current_task, *tmp;
    assert(NULL != f);
    {
        LOCK_AVAILABLE_TASK; // safe access to gAvailableTask

        HASH_ITER(hh, gAvailableTask, current_task, tmp) {
            assert(NULL != current_task);
            (*f)(current_task->task);
        }
        UNLOCK_AVAILABLE_TASK; // safe access to gAvailableTask
    }
}

/** iterate over all the task available for the selected scheduler
 *
 * @param f function to execute over each thread
 * @param sched scheduler
 */
void iterSchedTask(iterOnTask f, const sched_t * const sched) {
    static mapTask_t *current_task, *tmp;
    assert(NULL != f);
    assert(NULL != sched);
    {
        LOCK_AVAILABLE_TASK; // safe access to gAvailableTask

        HASH_ITER(hh, gAvailableTask, current_task, tmp) {
            assert(NULL != current_task);
            if (current_task->task->sched == sched) {
                (*f)(current_task->task);
            }
        }
        UNLOCK_AVAILABLE_TASK; // safe access to gAvailableTask
    }
}

int schedIsThreadSafe() {
#ifdef THREADSAFE_SCHED
    return 1;
#else
    return 0;
#endif
}

int schedInit() {
#ifdef THREADSAFE_SCHED
    pthread_attr_t attr;
    size_t min_stacksize = sysconf(_SC_THREAD_STACK_MIN);
    pthread_attr_setstacksize(&attr, min_stacksize);
    LOG("THREADSAFE_SCHED ON: %zu", min_stacksize);
#endif
#ifdef USING_SPLIT_STACK
    LOG("USING_SPLIT_STACK ON");
#endif
    return 0;
}

int schedDeInit() {
    return 0;
}
