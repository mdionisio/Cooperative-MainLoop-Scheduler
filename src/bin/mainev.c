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
 * File:   mainev.c
 * Author: Michele Dionisio
 *
 * Created on 18 October 2014
 */

#include "task.h"
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
#define __FILENAME__ "mainev.c"
#endif

#include "logmacro.h"

////////////////////////////////////////////////////////////////////////////////

#undef TEST_CONNECTION
#undef TEST_SIMPLE_CONNECTION

////////////////////////////////////////////////////////////////////////////////

/// Start Example
static char const MESSAGE[] = "Hello, World!\n";

// the following macro work on gcc because it supports
// Statements and Declarations in Expressions
// https://gcc.gnu.org/onlinedocs/gcc/Statement-Exprs.html
//
#define YIELD_EVSOCKET_READ_LINE(bev, retVal, buffer, maxsize) ({              \
    char *b = buffer;                                                          \
    size_t toberead = maxsize;                                                 \
    int datalen = 0;                                                           \
    while (toberead > 0) {                                                     \
        int dl = YIELD_EVSOCKET_READ(bev, retVal, (void *) b, 1);              \
        if ((retVal.type == READ) && (dl == 1)) {                              \
            if (*b == '\n') {                                                  \
                break;                                                         \
            }                                                                  \
            b ++; datalen++; toberead--;                                       \
        } else { break; }                                                      \
    }                                                                          \
    datalen;                                                                   \
    });

#define YIELD_EVSOCKET_READ_LINE_TIMEOUT(bev, retVal, buffer, maxsize, t) ({   \
    char *b = buffer;                                                          \
    size_t toberead = maxsize;                                                 \
    int datalen = 0;                                                           \
    while (toberead > 0) {                                                     \
        int dl = YIELD_EVSOCKET_READ_TIMEOUT(bev, retVal, (void *) b, 1, t);   \
        if ((retVal.type == READ) && (dl == 1)) {                              \
            if (*b == '\n') {                                                  \
                break;                                                         \
            }                                                                  \
            b ++; datalen++; toberead--;                                       \
        } else { break; }                                                      \
    }                                                                          \
    datalen;                                                                   \
    });

/** task that read from socket
 *
 */
static void task_socket() {
    uint8_t readingBuffer[1024];
    int datalen;
    int i;

    struct timeval const ten_sec = {10, 0};

    evutil_socket_t * const fd = (evutil_socket_t *) (THIS_TASK->data);
    /* We got a new connection! Set up a bufferevent for it. */
    struct bufferevent *bev = bufferevent_socket_new(BASELOOP_TO_EVBASE(THIS_SCHED_BASE), *fd, BEV_OPT_CLOSE_ON_FREE);

    DEF_SOCKET_RETVAL(retval);

    int running = 1;

    while ((NULL != bev) && running && !ISSTOPREQUESTTASK) {
        //datalen = YIELD_EVSOCKET_READ_TIMEOUT(bev, retval, (void *) readingBuffer, sizeof (readingBuffer), ten_sec);
        datalen = YIELD_EVSOCKET_READ_LINE_TIMEOUT(bev, retval, (void *) readingBuffer, sizeof (readingBuffer), ten_sec);

checkreturn: // label to mark check returned value
        switch (retval.type) {
            case READ:
                if (datalen > 0) {
                    LOG("receive %d data:", datalen);
                    BEGINLOG();
                    for (i = 0; i < datalen; i++) {
                        if (isprint(readingBuffer[i])) {
                            SIMPLELOG("%c", (char) readingBuffer[i]);
                        } else {
                            SIMPLELOG("\\x%02x", (int) readingBuffer[i]);
                        }
                    }
                    ENDLOG();

                    RESET_SOCKET_RETVAL(retval);
                    YIELD_EVSOCKET_WRITE(bev, (void *) MESSAGE, sizeof (MESSAGE), retval);
                    goto checkreturn; // goto check feedback
                } else { // there is something strange
                    LOG("no data to read any more");
                    running = 0;
                }
                break;
            case WRITE:
                LOG("data write");
                break;
            case EVENT:
                if (retval.events & BEV_EVENT_ERROR) {
                    perror("Error from bufferevent\n");
                }
                if (retval.events & (BEV_EVENT_EOF | BEV_EVENT_ERROR)) {
                    LOG("operation canceled for close client");
                    running = 0;
                }
                break;
            case UNKONWN:
                LOG("operation canceled");
                running = 0;
                break;
        }
        RESET_SOCKET_RETVAL(retval);
    } // end while

    if (NULL != bev) {
        bufferevent_free(bev);
        bev = NULL;
    }

    LOG("end");

    RETURN_TASK(2);
}

////////////////////////////////////////////////////////////////////////////////

/** task that accept new socket
 *
 */
static void task_listen() {

    struct sockaddr_in sin;

    /* Clear the sockaddr before using it, in case there are extra
     * platform-specific fields that can mess us up. */
    memset(&sin, 0, sizeof (sin));
    sin.sin_family = AF_INET;
    sin.sin_addr.s_addr = INADDR_ANY;
    sin.sin_port = htons(1234);

    /* Create a new listener */
    struct evconnlistener *listener = evconnlistener_new_bind(BASELOOP_TO_EVBASE(THIS_SCHED_BASE), NULL, NULL,
            LEV_OPT_CLOSE_ON_FREE | LEV_OPT_REUSEABLE, -1,
            (struct sockaddr *) &sin, sizeof (sin));

    if (!listener) {
        perror("Couldn't create listener\n");
        RETURN_TASK(1);
    }

    DEF_LISTEN_RETVAL(returnValue);
    int running = 1;

    while (running && !ISSTOPREQUESTTASK) {
        if (YIELD_EVACCEPT(listener, returnValue) > 0) { // task wakeup because something received
            if (returnValue.isError == 0) { // if there is no error
                LOG("connection accepted, create new task to manage connection");

                // create new task but do not automatically start the tasck
                tid_t const tid = new_task_sched(THIS_SCHED,
                        (taskFunc_t) task_socket,
                        TASK_CREATE_NOT_READY,
                        0 /* DEFAULT STACK SIZE */);

                // create and set data (socket) to send to the task
                evutil_socket_t * const pTmp = (evutil_socket_t *) malloc(sizeof (evutil_socket_t));
                *(pTmp) = returnValue.fd;

                task_t * const newReadtask = getTask(tid);
                // this memory will be free when the task will be destroyed
                newReadtask->data = (void *) pTmp;

                // now all the data is configured so we are ready to start the task
                schedule_task(newReadtask);
            } else { // there was en error in socket
                fprintf(stderr, "Got an error %d (%s) on the listener. "
                        "Shutting down.\n", returnValue.err, evutil_socket_error_to_string(returnValue.err));
                running = 0;
            }
        } else { // task wakeup for other reason
            LOG("operation canceled");
            running = 0;
        }
        RESET_LISTEN_RETVAL(returnValue);
    } // end while

    evconnlistener_free(listener);
    listener = NULL;

    LOG("end");

    RETURN_TASK(0);
}

////////////////////////////////////////////////////////////////////////////////

#if defined(TEST_CONNECTION) || defined(TEST_SIMPLE_CONNECTION)

/** task that connect to a listening socket
 *
 */
static void task_connect() {

    DEF_SOCKET_RETVAL(retval);

    LOG("start");

    struct bufferevent *bev = bufferevent_socket_new(BASELOOP_TO_EVBASE(THIS_SCHED_BASE), -1, BEV_OPT_CLOSE_ON_FREE);

    if ((NULL != bev) && YIELD_EVSOCKET_CONNECT(bev, NULL, AF_UNSPEC, "127.0.0.1", 1234, retval) == 0) {
        LOG("connected");
        RESET_SOCKET_RETVAL(retval);
        YIELD_EVSOCKET_WRITE(bev, (void *) MESSAGE, sizeof (MESSAGE), retval);
        // wait answer
        RESET_SOCKET_RETVAL(retval);

        uint8_t readingBuffer[1024];
        int datalen = YIELD_EVSOCKET_READ(bev, retval, (void *) readingBuffer, sizeof (readingBuffer));

        if (datalen > 0) {
            LOG("Stress receive %d data:", datalen);
            BEGINLOG();
            for (int i = 0; i < datalen; i++) {
                if (isprint(readingBuffer[i])) {
                    SIMPLELOG("%c", (char) readingBuffer[i]);
                } else {
                    SIMPLELOG("\\x%02x", (int) readingBuffer[i]);
                }
            }
            ENDLOG();
        }
    } else {
        LOG("error connecting");
    }

    if (bev) {
        bufferevent_free(bev);
        bev = NULL;
    }

    LOG("end");

    RETURN_TASK(0);
}

#endif

////////////////////////////////////////////////////////////////////////////////

#ifdef TEST_CONNECTION

/** task that show how sleep works
 *
 */
static void task_stress() {

    struct timeval const five_sec = {0, 500000 /* us */};

    int i = 0;
    int running = 1;
    while (running && !ISSTOPREQUESTTASK) {
        if (YIELD_EVSLEEP(five_sec)) { // task wakeup for timeout

            // create new task but do not automatically start the tasck

            LOG("stress connect %d", i);

            tid_t const tid = new_task_sched(THIS_SCHED,
                    (taskFunc_t) task_connect,
                    TASK_NONE,
                    0 /* DEFAULT STACK SIZE */);

            LOG("stress created connect %d %u", i++, tid);


        } else { // task wakeup for other reason
            LOG("operation canceled");
            running = 0;
        }
    } // end while

    LOG("end");
    RETURN_TASK(0);
}

#endif

////////////////////////////////////////////////////////////////////////////////


#include <sys/time.h>                // for gettimeofday()

static int async_function_sleep(int i) {
    struct timeval const five_sec = {5, 0};
    if (YIELD_EVSLEEP(five_sec)) { // task wakeup for timeout
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
        if (YIELD_EVSLEEP(five_sec)) { // task wakeup for timeout
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
        if (YIELD_EVSIGNAL(SIGINT)) { // task wakeup for signal received
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

static void task_sleep() {

    struct timeval t1;
    struct timeval t2;
    double elapsedTime;

    int running = 1;
    while (running && !ISSTOPREQUESTTASK) {
        LOG("sleep suspended %d", THIS_TID);
        gettimeofday(&t1, NULL);
        SUSPEND;
        gettimeofday(&t2, NULL);

        // compute and print the elapsed time in millisec
        elapsedTime = (t2.tv_sec - t1.tv_sec) * 1000.0; // sec to ms
        elapsedTime += (t2.tv_usec - t1.tv_usec) / 1000.0; // us to ms

        LOG("sleep %d wakeup after %lf", THIS_TID, elapsedTime);
    } // end while

    LOG("sleep %d end", THIS_TID);
    RETURN_TASK(0);
}

/** task that show how sleep works
 *
 */
static void task_wateup() {

    struct timeval t1;
    struct timeval t2;
    double elapsedTime;

    struct timeval const five_sec = {5, 0};

    LOG("wakeup %d: Create Counter Task", THIS_TID);
    tid_t tid = new_task_sched(THIS_SCHED, (taskFunc_t) task_sleep, TASK_NONE, 0 /* STACK SIZE */);
    LOG("wakeup %d: Created Counter Task: %d", THIS_TID, tid);

#if THREADSAFE_SCHED
    LOG("wakeup %d: Create Counter Task", THIS_TID);
    sched_t *sched1 = create_sched(SCHED_NONE | SCHED_END_WITHOUT_TASKS | SCHED_LIBEVENT);
    assert(NULL != sched1);
    tid_t tid1 = new_task_sched(sched1, (taskFunc_t) task_sleep, TASK_NONE, 0 /* STACK SIZE */);
    LOG("wakeup %d: Created Counter Task: %d", THIS_TID, tid1);
    pthread_t new_thread;
    pthread_create(&new_thread, NULL, mainloop_sched, sched1);
#endif

    int i = 0;
    int running = 1;
    while (running && !ISSTOPREQUESTTASK) {

        gettimeofday(&t1, NULL);
        if (YIELD_EVSLEEP(five_sec)) { // task wakeup for timeout
            gettimeofday(&t2, NULL);

            // compute and print the elapsed time in millisec
            elapsedTime = (t2.tv_sec - t1.tv_sec) * 1000.0; // sec to ms
            elapsedTime += (t2.tv_usec - t1.tv_usec) / 1000.0; // us to ms

            LOG("wakeup %d: ask to wakeup %d (%lf ms)", THIS_TID, tid, elapsedTime);
            RESUME(tid);
#if THREADSAFE_SCHED
            LOG("wakeup %d: ask to wakeup from other thread %d", THIS_TID, tid1);
            RESUME_EV_FROM_THREAD(tid1);
#endif
        } else { // task wakeup for other reason
            LOG("wakeup %d: operation canceled", THIS_TID);
            running = 0;
        }
    } // end while

#if THREADSAFE_SCHED
    LOG("wakeup %d: stop all thread on sched1", THIS_TID);
    // scheduling is terminated so the scheduler can be detroyed
    // iter over all Scheduled task and mark it to be stopped
    iterSchedTask(stopRequest_task, sched1);
    // wakeup task to be sure to stop ip
    RESUME_EV_FROM_THREAD(tid1);
    LOG("wakeup %d: join thread", THIS_TID);
    pthread_join(new_thread, NULL);
    LOG("wakeup %d: Destroy Scheduler", THIS_TID);
    destroy_sched(&sched1);
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
    sched_t *sched = create_sched(SCHED_NONE | SCHED_END_WITHOUT_TASKS | SCHED_LIBEVENT);
    assert(NULL != sched);

    LOG("Create Signal Task");
    new_task_sched(sched, (taskFunc_t) task_signal, TASK_NONE, 0 /* DEFAULT STACK SIZE */);

    LOG("Create Listening Task");
    new_task_sched(sched, (taskFunc_t) task_listen, TASK_NONE, 0 /* DEFAULT STACK SIZE */);

    LOG("Create Counter Task");
    new_task_sched(sched, (taskFunc_t) task_counter, TASK_NONE, 8192 /* STACK SIZE */);

#ifdef TEST_SIMPLE_CONNECTION
    LOG("Create Connect Task");
    new_task_sched(sched, (taskFunc_t) task_connect, TASK_NONE, 0 /* DEFAULT STACK SIZE */);
#endif

#ifdef TEST_CONNECTION
    LOG("Create Stress Connection Task");
    new_task_sched(sched, (taskFunc_t) task_stress, TASK_NONE, 0 /* DEFAULT STACK SIZE */);
#endif

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
