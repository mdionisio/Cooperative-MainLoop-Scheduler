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
 * File:   taskev.c
 * Author: Michele Dionisio
 *
 * Created on 18 October 2014
 */

#include "taskuv.h"
#include "unused.h"

#include <assert.h>

#include <uv.h>
#include "task_loop_private.h"

////////////////////////////////////////////////////////////////////////////////
#ifndef DEBUG_UV_TASK
#define FAKELOG
#endif

/// Log Macro

#ifndef __FILENAME__
#define __FILENAME__ "taskuv.c"
#endif

#include "logmacro.h"

#include "lconfig.h"

#include "likely.h"

#define NO_EVENT      (0)
#define TIMEOUT_EVENT (1)
#define SIGNAL_EVENT  (2)

/// TIMEOUT

typedef struct timeoutinfo {
    task_t * task;
    short event;
} timeoutinfo_t;

static void timer_cb(uv_timer_t* thandle) {
    assert(NULL != thandle);
    uv_handle_t * const p_handler = (uv_handle_t *) thandle;
    assert(NULL != p_handler->data);
    timeoutinfo_t * const timeout = (timeoutinfo_t*) (p_handler->data);
    assert(NULL != timeout);
    assert(NULL != timeout->task);

    LOG("timeout callback for task: %u", timeout->task->tid);

    timeout->event = TIMEOUT_EVENT;
    uv_timer_stop(thandle);
    schedule_task(timeout->task);
}

int yield_sleep_uv(const struct timeval * const time) {
    assert(NULL != THIS_TASK);
    assert(NULL != THIS_SCHED);
    assert(NULL != THIS_SCHED_BASE);
    assert(NULL != BASELOOP_TO_UVLOOP(THIS_SCHED_BASE));
    assert(NULL != time);

    uint64_t timems = ((uint64_t) time->tv_sec * 1000ULL) + (time->tv_usec / 1000);

    uv_timer_t uvtimer;
    uv_handle_t *p_handler = (uv_handle_t *) (&uvtimer);
    assert(0 == uv_timer_init(BASELOOP_TO_UVLOOP(THIS_SCHED_BASE), &uvtimer));

    timeoutinfo_t arg;
    arg.event = NO_EVENT;
    arg.task = THIS_TASK;

    p_handler->data = &arg;
    assert(0 == uv_timer_start(&uvtimer, timer_cb, timems, 0));

    LOG("task %u, wait timeout", THIS_TID);

    suspend_task(); // task suspend;

    if (likely(arg.event == TIMEOUT_EVENT)) {
        LOG("task %u, resume for timeout", THIS_TID);
    } else {
        uv_timer_stop(&uvtimer);
        LOG("task %u, resume not for timeout", THIS_TID);
    }

    return (arg.event);
}

int yield_sleep_uv_sec(unsigned int sec) {
    struct timeval time = {0, 0};
    time.tv_sec = sec;
    return yield_sleep_uv(&time);
}

int yield_sleep_uv_msec(unsigned int msec) {
    struct timeval time = {0, 0};
    const unsigned int sec = msec / 1000;
    const unsigned int usec = (msec % 1000) * 1000;
    time.tv_sec = sec;
    time.tv_usec = usec;
    return yield_sleep_uv(&time);
}

/// SIGNAL

typedef struct signalinfo {
    task_t * task;
    short event;
} signalinfo_t;

static void signal_cb(uv_signal_t* shandle, int signum) {
    assert(NULL != shandle);
    uv_handle_t * const p_handler = (uv_handle_t *) shandle;
    assert(NULL != p_handler->data);
    signalinfo_t * const signalarg = (signalinfo_t *) (p_handler->data);
    assert(NULL != signalarg);
    assert(NULL != signalarg->task);

    LOG("timeout callback for task: %u (signal %d)", signalarg->task->tid, signum);

    signalarg->event = SIGNAL_EVENT;
    uv_signal_stop(shandle);
    schedule_task(signalarg->task);
}

int yield_signal_uv(const short signal, const struct timeval * const time) {
    assert(NULL != THIS_TASK);
    assert(NULL != THIS_SCHED);
    assert(NULL != THIS_SCHED_BASE);
    assert(NULL != BASELOOP_TO_UVLOOP(THIS_SCHED_BASE));

    timeoutinfo_t arg_timeout;
    arg_timeout.event = NO_EVENT;
    arg_timeout.task = THIS_TASK;

    struct event * ev_timeout = NULL;
    uv_timer_t uvtimer;

    if (NULL != time) {
        uint64_t timems = ((uint64_t) time->tv_sec * 1000ULL) + (time->tv_usec / 1000);

        uv_handle_t *p_handler = (uv_handle_t *) (&uvtimer);
        assert(0 == uv_timer_init(BASELOOP_TO_UVLOOP(THIS_SCHED_BASE), &uvtimer));

        p_handler->data = &arg_timeout;
        assert(0 == uv_timer_start(&uvtimer, timer_cb, timems, 0));
    }

    signalinfo_t arg_signal;
    arg_signal.event = NO_EVENT;
    arg_signal.task = THIS_TASK;

    uv_signal_t uvsignal;
    uv_handle_t *p_handler = (uv_handle_t *) (&uvsignal);
    assert(0 == uv_signal_init(BASELOOP_TO_UVLOOP(THIS_SCHED_BASE), &uvsignal));

    p_handler->data = &arg_signal;
    assert(0 == uv_signal_start(&uvsignal, signal_cb, (int) signal));

    LOG("task %u, wait signal", THIS_TID);

    suspend_task(); // task suspend;

    //todo check if signal ...
    LOG("task %u, resume for signal", THIS_TID);

    if (NULL != time) {
        if (unlikely(arg_timeout.event != TIMEOUT_EVENT)) {
            uv_timer_stop(&uvtimer);
        }
    }
    if (unlikely(arg_signal.event != SIGNAL_EVENT)) {
        uv_signal_stop(&uvsignal);
    }

    return (arg_signal.event);
}

#ifdef THREADSAFE_SCHED

/// MULTITHREAD SCHEDULER

struct callback_param {
    uv_cond_t callback_raise_cond;
    uv_mutex_t callback_raise_cond_lock;
    volatile int callback_raise;
    tid_t tid;
};

static void async_cb(uv_async_t* handle) {
    assert(NULL != handle);
    uv_handle_t * const p_handler = (uv_handle_t *) handle;
    assert(NULL != p_handler->data);
    struct callback_param * param_cb = (struct callback_param *) (p_handler->data);

    // copy in data
    tid_t tid = param_cb->tid;

    // unlock caller
    uv_mutex_lock(&param_cb->callback_raise_cond_lock);
    param_cb->callback_raise = 1;
    uv_cond_broadcast(&param_cb->callback_raise_cond);
    uv_mutex_unlock(&param_cb->callback_raise_cond_lock);

    // don't accett to any data incaming from handle because not
    // that data can be already destroyed

    task_t * const task = getTask(tid);
    if (likely(NULL != task)) {
        schedule_task(task);
    }

}

void wakeup_uv_immediate(tid_t tid) {
    task_t * const task = getTask(tid);
    if (likely(NULL != task)) {
        assert(NULL != task->sched);
        assert(NULL != task->sched->base_loop);
        assert(NULL != BASELOOP_TO_UVLOOP(task->sched->base_loop));
        if (is_running_sched(task->sched)) {
            uv_async_t param;
            uv_handle_t *p_handler = (uv_handle_t *) (&param);
            struct callback_param param_cb;
            param_cb.tid = tid;
            param_cb.callback_raise = 0;
            assert(0 == uv_mutex_init(&param_cb.callback_raise_cond_lock));
            assert(0 == uv_cond_init(&param_cb.callback_raise_cond));
            p_handler->data = &param_cb;

            assert(0 == uv_async_init(BASELOOP_TO_UVLOOP(task->sched->base_loop), &param, async_cb));

            // wakeup other thread
            assert(0 == uv_async_send(&param));

            // wait that the send is received
            uv_mutex_lock(&param_cb.callback_raise_cond_lock);
            while (param_cb.callback_raise == 0) {
                uv_cond_wait(&param_cb.callback_raise_cond, &param_cb.callback_raise_cond_lock);
            }
            assert(param_cb.callback_raise == 1);
            uv_mutex_unlock(&param_cb.callback_raise_cond_lock);

            uv_cond_destroy(&param_cb.callback_raise_cond);
            uv_mutex_destroy(&param_cb.callback_raise_cond_lock);

            LOG("requested to wakeup task %u", tid);
        } else {
            LOG("requested to wakeup task %u not done because scheduler is not running", tid);
        }
    } else {
        LOG("requested to wakeup task %u not done because task not exist", tid);
    }
}


#endif


/////////////////////////////////

static void idle_cb(uv_timer_t* thandle) {
    //LOG("timeout idle_cb *****************************");
}

static void * taskuv_create(void) {
    uv_genericinfo_t *loop = (uv_genericinfo_t *) malloc(sizeof (uv_genericinfo_t));
    assert(NULL != loop);
    BASELOOP_TO_UVLOOP(loop) = malloc(sizeof (uv_loop_t));
    assert(NULL != BASELOOP_TO_UVLOOP(loop));
    uv_loop_init(BASELOOP_TO_UVLOOP(loop));

    LOG("create libuv loop %p(%p)", loop, BASELOOP_TO_UVLOOP(loop));

    assert(0 == uv_timer_init(BASELOOP_TO_UVLOOP(loop), &loop->thandle));

    assert(0 == uv_timer_start(&loop->thandle, idle_cb, 1000, 1000));

    return loop;
}

static void taskuv_destroy(void *loop) {
    assert(NULL != loop);
    assert(NULL != BASELOOP_TO_UVLOOP(loop));
    LOG("destroy libuv loop %p(%p)", loop, BASELOOP_TO_UVLOOP(loop));
    uv_timer_stop(&(((uv_genericinfo_t *) loop)->thandle));
    uv_stop(BASELOOP_TO_UVLOOP(loop));
    uv_loop_close(BASELOOP_TO_UVLOOP(loop));
    free(BASELOOP_TO_UVLOOP(loop));
    free(loop);
}

static void taskuv_event_poll(void *loop) {
    assert(NULL != loop);
    assert(NULL != BASELOOP_TO_UVLOOP(loop));
    uv_run(BASELOOP_TO_UVLOOP(loop), UV_RUN_ONCE);
}

static void taskuv_event_stop(void *loop) {
    assert(NULL != loop);
    assert(NULL != BASELOOP_TO_UVLOOP(loop));
    uv_timer_stop(&(((uv_genericinfo_t *) loop)->thandle));
}

const struct sched_mainloop_interface task_libuv = {
    /* create = */ taskuv_create,
    /* destroy = */ taskuv_destroy,
    /* stop = */ taskuv_event_stop,
    /* event_poll = */taskuv_event_poll
};
