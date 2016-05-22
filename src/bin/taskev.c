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

#include "taskev.h"
#include "unused.h"

#include <assert.h>

#ifdef THREADSAFE_SCHED
#include <event2/thread.h>
#endif

#include <event2/event.h>
#include <errno.h>
#include <event2/util.h>
#include <event2/buffer.h>

#include "task_loop_private.h"

////////////////////////////////////////////////////////////////////////////////
#ifndef DEBUG_EV_TASK
#define FAKELOG
#endif

/// Log Macro

#ifndef __FILENAME__
#define __FILENAME__ "taskev.c"
#endif

#include "logmacro.h"

#include "lconfig.h"

#include "likely.h"


// TIMEOUT

typedef struct timeoutinfo {
    task_t * task;
    short event;
} timeout_t;

static void ev_timeout_cb(evutil_socket_t UNUSED_PARAMETER(fd), short what, void *arg) {
    assert(NULL != arg);
    timeout_t * const timeout = (timeout_t*) arg;
    assert(NULL != timeout);
    assert(NULL != timeout->task);

    LOG("timeout callback for task: %u", timeout->task->tid);

    timeout->event = what;
    schedule_task(timeout->task);
}

int yield_sleep_ev(const struct timeval * const time) {
    assert(NULL != THIS_TASK);
    assert(NULL != THIS_SCHED);
    assert(NULL != THIS_SCHED_BASE);
    assert(NULL != BASELOOP_TO_EVBASE(THIS_SCHED_BASE));
    assert(NULL != time);

    timeout_t arg;
    arg.event = 0;
    arg.task = THIS_TASK;

    struct event * ev = event_new(BASELOOP_TO_EVBASE(THIS_SCHED_BASE),
            -1, /* no file descriptor */
            EV_TIMEOUT,
            ev_timeout_cb, /* callback */
            &arg); /* extra argument to the callback */
    assert(NULL != ev);

    event_add(ev, time);

    LOG("task %u, wait timeout", THIS_TID);

    suspend_task(); // task suspend;

    LOG("task %u, resume for timeout", THIS_TID);

    event_del(ev);
    event_free(ev);
    ev = NULL;

    if (likely(EV_TIMEOUT == arg.event)) {
        return (int) arg.event;
    } else {
        LOG("task %u, resume for timeout but wrong event %d", THIS_TID, (int) arg.event);
        return -1;
    }
}

int yield_sleep_ev_sec(unsigned int sec) {
    struct timeval time = {0, 0};
    time.tv_sec = sec;
    return yield_sleep_ev(&time);
}

int yield_sleep_ev_msec(unsigned int msec) {
    struct timeval time = {0, 0};
    const unsigned int sec = msec / 1000;
    const unsigned int usec = (msec % 1000) * 1000;
    time.tv_sec = sec;
    time.tv_usec = usec;
    return yield_sleep_ev(&time);
}

// SIGNAL

typedef struct signalinfo {
    task_t * task;
    short event;
    short signal;
} signal_t;

const char* socketReturn_type_to_str(socketReturn_type_t type) {
    static char *toStr[] = {"UNKONWN", "READ", "WRITE", "EVENT"};
    return toStr[type];
}

static void
signal_cb(evutil_socket_t UNUSED_PARAMETER(fd), short UNUSED_PARAMETER(event), void *arg) {
    signal_t * const signal = (signal_t *) arg;
    assert(NULL != signal);
    assert(NULL != signal->task);

    LOG("signal callback for task: %u", signal->task->tid);

    signal->event = EV_SIGNAL;
    schedule_task(signal->task);
}

int yield_signal_ev(const short signal, const struct timeval * const timeout) {
    assert(NULL != THIS_TASK);
    assert(NULL != THIS_SCHED);
    assert(NULL != THIS_SCHED_BASE);
    assert(NULL != BASELOOP_TO_EVBASE(THIS_SCHED_BASE));

    timeout_t arg_timeout;
    arg_timeout.event = 0;
    arg_timeout.task = THIS_TASK;

    struct event * ev_timeout = NULL;

    if (NULL != timeout) {
        ev_timeout = event_new(BASELOOP_TO_EVBASE(THIS_SCHED_BASE),
                -1, /* no file descriptor */
                EV_TIMEOUT,
                ev_timeout_cb, /* callback */
                &arg_timeout); /* extra argument to the callback */
        assert(NULL != ev_timeout);

        event_add(ev_timeout, timeout);
    }

    signal_t arg;

    arg.task = THIS_TASK;
    arg.event = 0;
    arg.signal = signal;

    /* call sighup_function on a HUP signal */
    struct event *ev = evsignal_new(BASELOOP_TO_EVBASE(THIS_SCHED_BASE), signal, signal_cb, &arg);
    assert(NULL != ev);

    event_add(ev, NULL);

    LOG("task %u, wait signal", THIS_TID);

    suspend_task(); // task suspend;

    //todo check if signal ...
    LOG("task %u, resume for signal", THIS_TID);

    event_del(ev);
    event_free(ev);
    ev = NULL;

    if (NULL != ev_timeout) {
        event_del(ev_timeout);
        event_free(ev_timeout);
        ev_timeout = NULL;
    }

    if (likely(EV_SIGNAL == arg.event)) {
        return (int) arg.event;
    } else {
        LOG("task %u, resume for signal but timeout event %d",
                THIS_TID, (int) arg.event);
        return -1;
    }
}

// IMMEDIATE WAKEUP

#ifdef THREADSAFE_SCHED

struct callback_param {
    struct event * ev;
    tid_t tid;
};

static void ev_timeout_cb_thread(evutil_socket_t UNUSED_PARAMETER(fd), short UNUSED_PARAMETER(what), void *arg) {
    struct callback_param * param = (struct callback_param*) arg;
    assert(NULL != param);
    assert(NULL != param->ev);

    event_del(param->ev);
    param->ev = NULL;

    task_t * const task = getTask(param->tid);
    if (likely(NULL != task)) {
        LOG("task %u resume", task->tid);
        schedule_task(task);
    } else {
        LOG("task %u resume not dose because task does not exist", param->tid);
    }

    free(param);
    param = NULL;
}

void wakeup_ev_immediate(tid_t tid) {
    static struct timeval const immediate = {0 /* s */, 0 /* us */};

    task_t * const task = getTask(tid);
    if (likely(NULL != task)) {
        assert(NULL != task->sched);
        assert(NULL != task->sched->base_loop);
        if (is_running_sched(task->sched)) {
            assert(NULL != BASELOOP_TO_EVBASE(task->sched->base_loop));
            int retval = evthread_make_base_notifiable(BASELOOP_TO_EVBASE(task->sched->base_loop));
            assert(retval == 0);

            struct callback_param * const param = (struct callback_param *) malloc(sizeof (struct callback_param));
            assert(NULL != param);

            struct event * ev = event_new(BASELOOP_TO_EVBASE(task->sched->base_loop),
                    -1,
                    EV_TIMEOUT,
                    ev_timeout_cb_thread,
                    param);
            assert(NULL != ev);

            param->tid = tid;
            param->ev = ev;

            event_add(ev, &immediate);

            LOG("requested to wakeup task %u", tid);
        } else {
            LOG("requested to wakeup task %u not done because scheduler is not running", tid);
        }
    } else {
        LOG("requested to wakeup task %u not done because task not exist", tid);
    }
}
#endif

// LISTEN

static void init_acceptReturn(acceptReturn_t * const retval, struct evconnlistener * const listener, task_t * const task) {
    assert(NULL != retval);
    assert(NULL != listener);
    assert(NULL != task);
    retval->isError = 0;
    retval->fd = -1;
    retval->err = 0;
    retval->task = task;
}

void reset_acceptReturn(acceptReturn_t * const retval) {
    assert(NULL != retval);
    retval->isError = 0;
    retval->fd = -1;
    retval->err = 0;
}

static void accept_conn_cb(struct evconnlistener *listener, evutil_socket_t fd, struct sockaddr *address, int UNUSED_PARAMETER(socklen), void *ctx) {
    acceptReturn_t * const retval = (acceptReturn_t *) ctx;
    assert(NULL != retval);
    assert(NULL != address);
    assert(NULL != listener);
    assert(NULL != retval->task);
    assert(0 != fd);

    LOG("accept connection callback for task: %u", retval->task->tid);

    retval->isError = 0;
    retval->fd = fd;
    retval->err = 0;
    schedule_task(retval->task);
    // don't call any other callback on this
    evconnlistener_disable(listener);
    evconnlistener_set_cb(listener, NULL, NULL);
    evconnlistener_set_error_cb(listener, NULL);
}

static void accept_error_cb(struct evconnlistener *listener, void *ctx) {
    acceptReturn_t * const retval = (acceptReturn_t *) ctx;
    assert(NULL != retval);
    assert(NULL != listener);

    LOG("accept connection callback for task: %u (error)", retval->task->tid);

    retval->isError = 1;
    retval->fd = -1;
    retval->err = EVUTIL_SOCKET_ERROR();
    schedule_task(retval->task);
    // don't call any other callback on this
    evconnlistener_disable(listener);
    evconnlistener_set_cb(listener, NULL, NULL);
    evconnlistener_set_error_cb(listener, NULL);
}

int yield_accept_ev(struct evconnlistener * const listen, acceptReturn_t * const returnValue, const struct timeval * const timeout) {
    assert(NULL != returnValue);
    assert(NULL != listen);
    assert(NULL != THIS_TASK);

    timeout_t arg_timeout;
    arg_timeout.event = 0;
    arg_timeout.task = THIS_TASK;

    struct event * ev_timeout = NULL;

    if (NULL != timeout) {
        assert(NULL != THIS_SCHED);
        assert(NULL != THIS_SCHED_BASE);
        assert(NULL != BASELOOP_TO_EVBASE(THIS_SCHED_BASE));
        ev_timeout = event_new(BASELOOP_TO_EVBASE(THIS_SCHED_BASE),
                -1, /* no file descriptor */
                EV_TIMEOUT,
                ev_timeout_cb, /* callback */
                &arg_timeout); /* extra argument to the callback */
        assert(NULL != ev_timeout);

        event_add(ev_timeout, timeout);
    }

    init_acceptReturn(returnValue, listen, THIS_TASK);

    evconnlistener_set_cb(listen, accept_conn_cb, returnValue);
    evconnlistener_set_error_cb(listen, accept_error_cb);
    evconnlistener_enable(listen);

    LOG("task %u wait accept connection", THIS_TID);

    suspend_task(); // task suspend;

    LOG("task %u resume for accept connection", THIS_TID);

    evconnlistener_disable(listen);
    evconnlistener_set_cb(listen, NULL, NULL);
    evconnlistener_set_error_cb(listen, NULL);

    if (NULL != ev_timeout) {
        event_del(ev_timeout);
        event_free(ev_timeout);
        ev_timeout = NULL;
    }

    return returnValue->fd;
}

// SOCKET

static void init_socketReturn(socketReturn_t * const retval, task_t * const task) {
    assert(NULL != retval);
    assert(NULL != task);
    retval->type = UNKONWN;
    retval->task = task;
    retval->events = 0;
}

void reset_socketReturn(socketReturn_t * const retval) {
    assert(NULL != retval);
    retval->type = UNKONWN;
    retval->events = 0;
}

static void echo_read_cb(struct bufferevent *bev, void *ctx) {
    socketReturn_t * const retval = (socketReturn_t *) ctx;
    assert(NULL != retval);
    assert(NULL != retval->task);
    assert(NULL != bev);

    LOG("read callback for task: %u", retval->task->tid);

    retval->type = READ;
    retval->events = EV_READ;
    schedule_task(retval->task);
    // don't call any other callback on this

    bufferevent_disable(bev, EV_READ | EV_WRITE);
    bufferevent_setcb(bev, NULL, NULL, NULL, NULL);
}

static void echo_event_cb(struct bufferevent *bev, short events, void *ctx) {
    socketReturn_t * const retval = (socketReturn_t *) ctx;
    assert(NULL != retval);
    assert(NULL != retval->task);
    assert(NULL != bev);

    LOG("event callback for task: %u", retval->task->tid);

    retval->type = EVENT;
    retval->events = events;
    schedule_task(retval->task);
    // don't call any other callback on this

    bufferevent_disable(bev, EV_READ | EV_WRITE);
    bufferevent_setcb(bev, NULL, NULL, NULL, NULL);
}

static void echo_write_cb(struct bufferevent *bev, void *ctx) {
    socketReturn_t * const retval = (socketReturn_t *) ctx;
    assert(NULL != retval);
    assert(NULL != retval->task);
    assert(NULL != bev);

    LOG("write callback for task: %u", retval->task->tid);

    retval->type = WRITE;
    retval->events = EV_WRITE;
    schedule_task(retval->task);
    // don't call any other callback on this

    bufferevent_disable(bev, EV_READ | EV_WRITE);
    bufferevent_setcb(bev, NULL, NULL, NULL, NULL);
}

int yield_socket_ev(struct bufferevent * const bev,
        socketReturn_t * const returnValue,
        const struct timeval * const timeout) {
    assert(NULL != returnValue);
    assert(NULL != bev);
    assert(NULL != THIS_TASK);

    timeout_t arg_timeout;
    arg_timeout.event = 0;
    arg_timeout.task = THIS_TASK;

    struct event * ev_timeout = NULL;

    init_socketReturn(returnValue, THIS_TASK);

    // check if thereis data to drain
    struct evbuffer * const input = bufferevent_get_input(bev);
    if ((NULL != input) && (evbuffer_get_length(input) > 0)) {
        returnValue->type = READ;
        returnValue->events = EV_READ;
        return returnValue->events;
    }

    if (NULL != timeout) {
        assert(NULL != THIS_SCHED);
        assert(NULL != THIS_SCHED_BASE);
        assert(NULL != BASELOOP_TO_EVBASE(THIS_SCHED_BASE));
        ev_timeout = event_new(BASELOOP_TO_EVBASE(THIS_SCHED_BASE),
                -1, /* no file descriptor */
                EV_TIMEOUT,
                ev_timeout_cb, /* callback */
                &arg_timeout); /* extra argument to the callback */
        assert(NULL != ev_timeout);

        event_add(ev_timeout, timeout);
    }

    bufferevent_setcb(bev,
            /* read  */ echo_read_cb,
            /* write */ echo_write_cb,
            /* event */ echo_event_cb, returnValue);

    bufferevent_enable(bev, EV_READ | EV_WRITE);
    LOG("task %u wait read/write", THIS_TID);
    suspend_task(); // task suspend;
    LOG("task %u resume for read/write", THIS_TID);
    bufferevent_disable(bev, EV_READ | EV_WRITE);
    bufferevent_setcb(bev, NULL, NULL, NULL, NULL);

    if (NULL != ev_timeout) {
        event_del(ev_timeout);
        event_free(ev_timeout);
        ev_timeout = NULL;
    }

    return returnValue->events;
}

int yield_socket_ev_connect(struct bufferevent * const bev,
        struct evdns_base * const dns_base,
        int family, const char * hostname, int port,
        socketReturn_t * const returnValue,
        const struct timeval * const timeout) {
    assert(NULL != returnValue);
    assert(NULL != bev);
    assert(NULL != hostname);
    assert(NULL != THIS_TASK);
    init_socketReturn(returnValue, THIS_TASK);

    bufferevent_setcb(bev,
            /* read  */ echo_read_cb,
            /* write */ echo_write_cb,
            /* event */ echo_event_cb, returnValue);

    bufferevent_enable(bev, EV_READ | EV_WRITE);

    if (unlikely(bufferevent_socket_connect_hostname(
            bev, dns_base, family, hostname, port) == -1)) {
        bufferevent_disable(bev, EV_READ | EV_WRITE);
        bufferevent_setcb(bev, NULL, NULL, NULL, NULL);
        returnValue->type = UNKONWN;
        returnValue->events = 0;
        return -1;
    } else {
        timeout_t arg_timeout;
        arg_timeout.event = 0;
        arg_timeout.task = THIS_TASK;

        struct event * ev_timeout = NULL;

        if (NULL != timeout) {
            assert(NULL != THIS_SCHED);
            assert(NULL != THIS_SCHED_BASE);
            assert(NULL != BASELOOP_TO_EVBASE(THIS_SCHED_BASE));
            ev_timeout = event_new(BASELOOP_TO_EVBASE(THIS_SCHED_BASE),
                    -1, /* no file descriptor */
                    EV_TIMEOUT,
                    ev_timeout_cb, /* callback */
                    &arg_timeout); /* extra argument to the callback */
            assert(NULL != ev_timeout);

            event_add(ev_timeout, timeout);
        }
        LOG("task %u wait read/write/event", THIS_TID);
        suspend_task(); // task suspend;
        LOG("task %u wait read/write/event", THIS_TID);
        bufferevent_disable(bev, EV_READ | EV_WRITE);
        bufferevent_setcb(bev, NULL, NULL, NULL, NULL);

        if (NULL != ev_timeout) {
            event_del(ev_timeout);
            event_free(ev_timeout);
            ev_timeout = NULL;
        }

        if (likely((returnValue->type == EVENT) &&
                (0 != (returnValue->events & BEV_EVENT_CONNECTED)))) {
            return 0;
        } else {
            LOG("task %u resumed but with error", THIS_TID);
            return -2;
        }
    }
}

int yield_socket_read(struct bufferevent * const bev,
        socketReturn_t * const returnValue,
        void * const data, size_t datalen,
        const struct timeval * const timeout) {
    assert(NULL != returnValue);
    assert(NULL != data);
    assert(0 != datalen);
    assert(NULL != THIS_TASK);
    assert(NULL != bev);

    timeout_t arg_timeout;
    arg_timeout.event = 0;
    arg_timeout.task = THIS_TASK;

    struct event * ev_timeout = NULL;

    init_socketReturn(returnValue, THIS_TASK);

    // check if thereis data to drain
    struct evbuffer * input = bufferevent_get_input(bev);
    if (likely(NULL != input)) {
        if (evbuffer_get_length(input) > 0) {
            returnValue->type = READ;
            returnValue->events = EV_READ;
            goto ENDFUNC;
        }
    }

    if (NULL != timeout) {
        assert(NULL != THIS_SCHED);
        assert(NULL != THIS_SCHED_BASE);
        assert(NULL != BASELOOP_TO_EVBASE(THIS_SCHED_BASE));
        ev_timeout = event_new(BASELOOP_TO_EVBASE(THIS_SCHED_BASE),
                -1, /* no file descriptor */
                EV_TIMEOUT,
                ev_timeout_cb, /* callback */
                &arg_timeout); /* extra argument to the callback */
        assert(NULL != ev_timeout);

        event_add(ev_timeout, timeout);
    }

    bufferevent_setcb(bev,
            /* read  */ echo_read_cb,
            /* write */ NULL,
            /* event */ echo_event_cb, returnValue);

    bufferevent_enable(bev, EV_READ);
    LOG("task %u wait read", THIS_TID);
    suspend_task(); // task suspend;
    LOG("task %u resume for read", THIS_TID);
    bufferevent_disable(bev, EV_READ);
    bufferevent_setcb(bev, NULL, NULL, NULL, NULL);

    if (NULL != ev_timeout) {
        event_del(ev_timeout);
        event_free(ev_timeout);
        ev_timeout = NULL;
    }

ENDFUNC:
    if (likely(returnValue->type == READ)) {
        input = bufferevent_get_input(bev);
        if (likely(NULL != input)) {
            return evbuffer_remove(input, data, datalen);
        } else {
            LOG("task %u resumed but with error", THIS_TID);
        }
    } else {
        LOG("task %u resumed but with error and event: %s", THIS_TID, socketReturn_type_to_str(returnValue->type));
    }
    return -1;
}

int bufferevent_write(struct bufferevent *bufev,
        const void *data, size_t size);

int yield_socket_write(struct bufferevent * const bev,
        void * const data,
        size_t size,
        socketReturn_t * const returnValue,
        const struct timeval * const timeout) {
    assert(NULL != returnValue);
    assert(NULL != THIS_TASK);
    assert(NULL != bev);
    if (likely((size > 0) && (data != NULL))) {
        timeout_t arg_timeout;
        arg_timeout.event = 0;
        arg_timeout.task = THIS_TASK;

        struct event * ev_timeout = NULL;

        if (NULL != timeout) {
            assert(NULL != THIS_SCHED);
            assert(NULL != THIS_SCHED_BASE);
            assert(NULL != BASELOOP_TO_EVBASE(THIS_SCHED_BASE));
            ev_timeout = event_new(BASELOOP_TO_EVBASE(THIS_SCHED_BASE),
                    -1, /* no file descriptor */
                    EV_TIMEOUT,
                    ev_timeout_cb, /* callback */
                    &arg_timeout); /* extra argument to the callback */
            assert(NULL != ev_timeout);

            event_add(ev_timeout, timeout);
        }

        init_socketReturn(returnValue, THIS_TASK);
        bufferevent_setcb(bev, /* read */ NULL,
                /* write */ echo_write_cb,
                /* event */ echo_event_cb, returnValue);

        bufferevent_enable(bev, EV_WRITE);
        bufferevent_write(bev, data, size);
        LOG("task %u wait write", THIS_TID);
        suspend_task(); // task suspend;
        LOG("task %u resume for write", THIS_TID);
        bufferevent_disable(bev, EV_WRITE);
        bufferevent_setcb(bev, NULL, NULL, NULL, NULL);

        if (NULL != ev_timeout) {
            event_del(ev_timeout);
            event_free(ev_timeout);
            ev_timeout = NULL;
        }

        return returnValue->events;
    } else {
        returnValue->type = UNKONWN;
        returnValue->events = 0;
        return -1;
    }
}

static void ev_idle_cb(evutil_socket_t UNUSED_PARAMETER(fd), short what, void *arg) {
    //LOG("timeout ev_idle_cb *****************************");
}

static void * taskev_create(void) {
    static int lib_event_initialized = 0;

    if (unlikely(0 == lib_event_initialized)) {

#ifdef THREADSAFE_SCHED
#ifdef WIN32
        LOG("initialize lib event for windows thread");
        evthread_use_windows_threads();
#else
        LOG("initialize lib event for pthread");
        evthread_use_pthreads();
#endif
#else
        LOG("initialize lib event for no thread support");
#endif
        lib_event_initialized = 1;
    }

    ev_genericinfo_t * retval = (ev_genericinfo_t*) malloc(sizeof (ev_genericinfo_t));
    assert(NULL != retval);
    BASELOOP_TO_EVBASE(retval) = event_base_new();
    assert(NULL != BASELOOP_TO_EVBASE(retval));

    struct timeval time = {1, 0};
    retval->ev_idle = event_new(BASELOOP_TO_EVBASE(retval),
            -1, /* no file descriptor */
            EV_TIMEOUT | EV_PERSIST,
            ev_idle_cb, /* callback */
            NULL); /* extra argument to the callback */
    assert(NULL != retval->ev_idle);

    event_add(retval->ev_idle, &time);

    LOG("created libevent base %p(%p)", retval, BASELOOP_TO_EVBASE(retval));

    return (void *) retval;
}

static void taskev_destroy(void *base) {
    assert(NULL != base);
    assert(NULL != BASELOOP_TO_EVBASE(base));

    if (NULL != ((ev_genericinfo_t *) base)->ev_idle) {
        event_del(((ev_genericinfo_t *) base)->ev_idle);
        event_free(((ev_genericinfo_t *) base)->ev_idle);
        ((ev_genericinfo_t *) base)->ev_idle = NULL;
    }

    LOG("destroy libevent base %p(%p)", base, BASELOOP_TO_EVBASE(base));
    event_base_free(BASELOOP_TO_EVBASE(base));
    free(base);
}

static void taskev_event_poll(void *base) {
    assert(NULL != base);
    assert(NULL != BASELOOP_TO_EVBASE(base));
    //LOG("libevent poll base %p(%p)", base, BASELOOP_TO_EVBASE(base));
    event_base_loop(BASELOOP_TO_EVBASE(base), EVLOOP_ONCE /* EVLOOP_NONBLOCK */);
}

static void taskev_event_stop(void *base) {
    assert(NULL != base);
    if (NULL != ((ev_genericinfo_t *) base)->ev_idle) {
        event_del(((ev_genericinfo_t *) base)->ev_idle);
        event_free(((ev_genericinfo_t *) base)->ev_idle);
        ((ev_genericinfo_t *) base)->ev_idle = NULL;
    }
}

const struct sched_mainloop_interface task_libevent = {
    /* create = */ taskev_create,
    /* destroy = */ taskev_destroy,
    /* stop = */ taskev_event_stop,
    /* event_poll = */taskev_event_poll
};
