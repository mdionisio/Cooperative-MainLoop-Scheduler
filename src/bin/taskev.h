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
 * File:   taskev.h
 * Author: Michele Dionisio
 *
 * Created on 18 October 2014
 */

#ifndef __TASKEV_H__
#define __TASKEV_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "task.h"

#include <event2/event.h>
#include <event2/listener.h>
#include <event2/bufferevent.h>

#include <signal.h>
#include <sys/time.h>

    // SIGNAL

    int yield_signal_ev(const short signal, const struct timeval * const timeout);

#define YIELD_EVSIGNAL(x) yield_signal_ev(x, NULL)
#define YIELD_EVSIGNAL_TIMEOUT(x, timeout) yield_signal_ev(x, &timeout)

    // TIMER

    int yield_sleep_ev(const struct timeval * const time);

#define YIELD_EVSLEEP(x) yield_sleep_ev(&x)

    int yield_sleep_ev_sec(unsigned int sec);

#define YIELD_EVSLEEP_SEC(x) yield_sleep_ev_sec(x)

    int yield_sleep_ev_msec(unsigned int msec);

#define YIELD_EVSLEEP_MSEC(x) yield_sleep_ev_msec(x)

    // SOCKET

    typedef struct acceptReturn {
        int isError;
        evutil_socket_t fd;
        int err;
        task_t * task;
    } acceptReturn_t;

    void reset_acceptReturn(acceptReturn_t * const retval);

    int yield_accept_ev(struct evconnlistener * const listen,
            acceptReturn_t * const returnValue,
            const struct timeval * const timeout);

#define DEF_LISTEN_RETVAL(x) acceptReturn_t x; reset_acceptReturn(&x)
#define RESET_LISTEN_RETVAL(x) reset_acceptReturn(&x)
#define YIELD_EVACCEPT(x, y) yield_accept_ev(x, &y, NULL)
#define YIELD_EVACCEPT_TIMEOUT(x, y, timeout) yield_accept_ev(x, &y, &timeout)

    typedef enum {
        UNKONWN = -1, READ = 1, WRITE = 2, EVENT = 3
    } socketReturn_type_t;

    const char* socketReturn_type_to_str(socketReturn_type_t type);

    typedef struct socketReturn {
        socketReturn_type_t type;
        short events;
        task_t * task;
    } socketReturn_t;

    void reset_socketReturn(socketReturn_t * const retval);

    int yield_socket_ev(struct bufferevent * const bev,
            socketReturn_t * const returnValue,
            const struct timeval * const timeout);

    int yield_socket_read(struct bufferevent * const bev,
            socketReturn_t * const returnValue,
            void * const data,
            size_t datalen,
            const struct timeval * const timeout);

    int yield_socket_ev_connect(struct bufferevent * const bev,
            struct evdns_base * const dns_base,
            int family, const char * hostname, int port,
            socketReturn_t * const returnValue,
            const struct timeval * const timeout);

#define DEF_SOCKET_RETVAL(x) socketReturn_t x; reset_socketReturn(&x)
#define RESET_SOCKET_RETVAL(x) reset_socketReturn(&x)
#define YIELD_EVSOCKET(x, y) yield_socket_ev(x, &y, NULL)
#define YIELD_EVSOCKET_TIMEOUT(x, y, timeout) yield_socket_ev(x, &y, &timeout)
#define YIELD_EVSOCKET_READ(x, y, data, sizedata) yield_socket_read(x, &y, data, sizedata, NULL)
#define YIELD_EVSOCKET_READ_TIMEOUT(x, y, data, sizedata, timeout) yield_socket_read(x, &y, data, sizedata, &timeout)

#define YIELD_EVSOCKET_CONNECT(b, d, f, h, p, y) yield_socket_ev_connect(b, d, f, h, p, &y, NULL)
#define YIELD_EVSOCKET_CONNECT_TIMEOUT(b, d, f, h, p, y, timeout) yield_socket_ev_connect(b, d, f, h, p, &y, &timeout)

    int yield_socket_write(struct bufferevent *bufev,
            void * const data,
            size_t size,
            socketReturn_t * const returnValue,
            const struct timeval * const timeout);

#define YIELD_EVSOCKET_WRITE(bev, data, len, y) yield_socket_write(bev, data, len, &y, NULL)
#define YIELD_EVSOCKET_WRITE_TIMEOUT(bev, data, len, y, timeout) yield_socket_write(bev, data, len, &y, &timeout)

#ifdef THREADSAFE_SCHED

    // MULTITHREAD SCHEDULER

    void wakeup_ev_immediate(tid_t tid);
#define RESUME_EV_FROM_THREAD(tid) wakeup_ev_immediate(tid)

#endif

    // GENERAL

    typedef struct ev_genericinfo {
        struct event_base * base;
        struct event * ev_idle;
    } ev_genericinfo_t;

#define BASELOOP_TO_EVBASE(x) (((struct ev_genericinfo *)x)->base)

    extern struct sched_mainloop_interface const task_libevent;


#ifdef __cplusplus
}
#endif

#endif /* __TASKEV_H__ */
