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
 * File:   mainuv.c
 * Author: Michele Dionisio
 *
 * Created on 18 October 2014
 */

#include "task.h"
#include "taskuv.h"
#include "taskev.h"

#include <stdio.h>
#include <ctype.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <assert.h>

#include "unused.h"

#if THREADSAFE_SCHED
#include <pthread.h>
#endif

////////////////////////////////////////////////////////////////////////////////

/// Log Macro

#ifndef __FILENAME__
#define __FILENAME__ "mainuv.c"
#endif

#include "logmacro.h"

////////////////////////////////////////////////////////////////////////////////

#include <sys/time.h>                // for gettimeofday()

static int async_function_sleep(int i) {
    struct timeval const five_sec = {5, 0};
    if (YIELD_UVSLEEP(five_sec)) { // task wakeup for timeout
        LOG("async function");
    }
    return (i + 1);
}

/** task that show how sleep works
 *
 */
static void task_counter() {

    struct timeval t1;
    struct timeval t2;
    double elapsedTime;

    struct timeval const five_sec = {5, 0};

    int i = 0;
    int running = 1;
    while (running && !ISSTOPREQUESTTASK) {
        int app = async_function_sleep(2);
        LOG("app %d", app);

        gettimeofday(&t1, NULL);
        if (YIELD_UVSLEEP(five_sec)) { // task wakeup for timeout
            gettimeofday(&t2, NULL);

            // compute and print the elapsed time in millisec
            elapsedTime = (t2.tv_sec - t1.tv_sec) * 1000.0; // sec to ms
            elapsedTime += (t2.tv_usec - t1.tv_usec) / 1000.0; // us to ms

            LOG("counter %d (%lf ms)", i++, elapsedTime);
        } else { // task wakeup for other reason
            LOG("operation canceled");
            running = 0;
        }
    } // end while

    LOG("end");
    RETURN_TASK(0);
}

////////////////////////////////////////////////////////////////////////////////

/** task that manage SIGINT signal
 *
 */
static void task_signal() {

    int running = 1;
    while (running && !ISSTOPREQUESTTASK) {
        if (YIELD_UVSIGNAL(SIGINT)) { // task wakeup for signal received
            LOG("signal received");
            // iter over all Scheduled task and mark it to be stopped
            iterSchedTask(stopRequest_task, THIS_SCHED);
            // iter over all Scheduled task and wakeup it
            iterSchedTask(schedule_task_void, THIS_SCHED);
        } else { // task wakeup for other reason
            LOG("operation canceled");
            running = 0;
        }
    } // end while

    LOG("end");
    RETURN_TASK(0);
}

////////////////////////////////////////////////////////////////////////////////

static void task_sleepev() {

    struct timeval t1;
    struct timeval t2;
    double elapsedTime;

    int running = 1;
    while (running && !ISSTOPREQUESTTASK) {
        LOG("sleepev suspended %d", THIS_TID);
        gettimeofday(&t1, NULL);
        //YIELD_EVSLEEP_SEC(100);
        SUSPEND;
        gettimeofday(&t2, NULL);

        // compute and print the elapsed time in millisec
        elapsedTime = (t2.tv_sec - t1.tv_sec) * 1000.0; // sec to ms
        elapsedTime += (t2.tv_usec - t1.tv_usec) / 1000.0; // us to ms

        LOG("sleepev %d wakeup after %lf", THIS_TID, elapsedTime);
    } // end while

    LOG("sleepev %d end", THIS_TID);
    RETURN_TASK(0);
}

static void task_sleep() {

    struct timeval t1;
    struct timeval t2;
    double elapsedTime;

    int running = 1;
    while (running && !ISSTOPREQUESTTASK) {
        LOG("sleep suspended %d", THIS_TID);
        gettimeofday(&t1, NULL);
        YIELD_UVSLEEP_SEC(100);
        //SUSPEND;
        gettimeofday(&t2, NULL);

        // compute and print the elapsed time in millisec
        elapsedTime = (t2.tv_sec - t1.tv_sec) * 1000.0; // sec to ms
        elapsedTime += (t2.tv_usec - t1.tv_usec) / 1000.0; // us to ms

        LOG("sleep %d wakeup after: %lf", THIS_TID, elapsedTime);
    } // end while

    LOG("sleep %d end", THIS_TID);
    RETURN_TASK(0);
}

#define MIXEDLOOP 0

/** task that show how sleep works
 *
 */
static void task_wateup() {

    struct timeval t1;
    struct timeval t2;
    double elapsedTime;

    struct timeval const five_sec = {5, 0};

    LOG("wakeup %d: Create sleep Task", THIS_TID);
    tid_t tid = new_task_sched(THIS_SCHED, (taskFunc_t) task_sleep, TASK_NONE, 0 /* STACK SIZE */);
    LOG("wakeup %d: Created sleep Task: %d", THIS_TID, tid);

#if THREADSAFE_SCHED
#if MIXEDLOOP == 0
    LOG("wakeup %d: Create sleep Task", THIS_TID);
    sched_t *sched1 = create_sched(SCHED_NONE | SCHED_END_WITHOUT_TASKS | SCHED_LIBUV | SCHED_END_WITHOUT_PENDIG);
    assert(NULL != sched1);
    tid_t tid1 = new_task_sched(sched1, (taskFunc_t) task_sleep, TASK_NONE, 0 /* STACK SIZE */);
    LOG("wakeup %d: Created sleep Task: %d", THIS_TID, tid1);
    uv_thread_t new_thread1;
    uv_thread_create(&new_thread1, (uv_thread_cb) (mainloop_sched), sched1);
#else
    LOG("wakeup %d: Create sleepev Task", THIS_TID);
    sched_t *sched2 = create_sched(SCHED_NONE | SCHED_END_WITHOUT_TASKS | SCHED_LIBEVENT);
    assert(NULL != sched2);
    tid_t tid2 = new_task_sched(sched2, (taskFunc_t) task_sleepev, TASK_NONE, 0 /* STACK SIZE */);
    LOG("wakeup %d: Created sleepev Task: %d", THIS_TID, tid2);
    pthread_t new_thread2;
    pthread_create(&new_thread2, NULL, mainloop_sched, sched2);
#endif
#endif

    int i = 0;
    int running = 1;
    while (running && !ISSTOPREQUESTTASK) {

        gettimeofday(&t1, NULL);
        if (YIELD_UVSLEEP(five_sec)) { // task wakeup for timeout
            gettimeofday(&t2, NULL);

            // compute and print the elapsed time in millisec
            elapsedTime = (t2.tv_sec - t1.tv_sec) * 1000.0; // sec to ms
            elapsedTime += (t2.tv_usec - t1.tv_usec) / 1000.0; // us to ms

            LOG("wakeup %d: ask to wakeup %d (%lf ms)", THIS_TID, tid, elapsedTime);
            RESUME(tid);
#if THREADSAFE_SCHED
#if MIXEDLOOP == 0
            LOG("wakeup %d: ask to wakeup from other thread %d", THIS_TID, tid1);
            RESUME_UV_FROM_THREAD(tid1);
#else
            LOG("wakeup %d: ask to wakeup from other thread %d", THIS_TID, tid2);
            RESUME_EV_FROM_THREAD(tid2);
#endif
#endif
        } else { // task wakeup for other reason
            LOG("wakeup %d: operation canceled", THIS_TID);
            running = 0;
        }
    } // end while

#if THREADSAFE_SCHED
#if MIXEDLOOP == 0
    LOG("wakeup %d: stop all thread on sched1", THIS_TID);
    // scheduling is terminated so the scheduler can be detroyed
    // iter over all Scheduled task and mark it to be stopped
    iterSchedTask(stopRequest_task, sched1);
    // wakeup task to be sure to stop ip
    RESUME_UV_FROM_THREAD(tid1);
    LOG("wakeup %d: join thread", THIS_TID);
    uv_thread_join(&new_thread1);
    LOG("wakeup %d: scheduler running: %d", THIS_TID, sched1->running);
    LOG("wakeup %d: scheduler running tasks: %u", THIS_TID, sched1->numAvailableTask);
    LOG("wakeup %d: Destroy Scheduler", THIS_TID);
    destroy_sched(&sched1);
#else
    LOG("wakeup %d: stop all thread on sched2", THIS_TID);
    // scheduling is terminated so the scheduler can be detroyed
    // iter over all Scheduled task and mark it to be stopped
    iterSchedTask(stopRequest_task, sched2);
    // wakeup task to be sure to stop ip
    RESUME_EV_FROM_THREAD(tid2);
    LOG("wakeup %d: join thread", THIS_TID);
    pthread_join(new_thread2, NULL);
    LOG("wakeup %d: scheduler running: %d", THIS_TID, sched2->running);
    LOG("wakeup %d: scheduler running tasks: %u", THIS_TID, sched2->numAvailableTask);
    LOG("wakeup %d: Destroy Scheduler", THIS_TID);
    destroy_sched(&sched2);
#endif

#endif

    LOG("wakeup %d: end", THIS_TID);
    RETURN_TASK(0);
}


////////////////////////////////////////////////////////////////////////////////

/** MAIN
 *
 * @param argc
 * @param argv
 * @return
 */
int main(int UNUSED_PARAMETER(argc), char ** UNUSED_PARAMETER(argv)) {
    LOG("Create Scheduler");

    if (schedInit() != 0) {
        LOG("Scheduler init error");
        return 1;
    }

    // create a new cooperative scheduler
    sched_t *sched = create_sched(SCHED_NONE | SCHED_END_WITHOUT_TASKS | SCHED_LIBUV | SCHED_END_WITHOUT_PENDIG);
    assert(NULL != sched);

    LOG("Create Signal Task");
    new_task_sched(sched, (taskFunc_t) task_signal, TASK_NONE, 0 /* DEFAULT STACK SIZE */);

    LOG("Create Counter Task");
    new_task_sched(sched, (taskFunc_t) task_counter, TASK_NONE, 8192 /* STACK SIZE */);

    LOG("Create Wakeup Task");
    new_task_sched(sched, (taskFunc_t) task_wateup, TASK_NONE, 16 * 8192 /* STACK SIZE */);

    LOG("Start Mainloop");
    // start scheduler
    mainloop_sched(sched);

    LOG("Destroy Scheduler");
    // scheduling is terminated so the scheduler can be detroyed
    destroy_sched(&sched);

    if (schedDeInit() != 0) {
        LOG("Scheduler deinit error");
        return 1;
    }

    LOG("mainloop terminated");

    return 0;
}
