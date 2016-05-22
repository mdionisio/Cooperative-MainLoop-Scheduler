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

#ifndef __TASKUV_H__
#define __TASKUV_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "task.h"

#include <uv.h>

#include <signal.h>
#include <sys/time.h>

    // TIMER

    int yield_sleep_uv(const struct timeval * const time);

#define YIELD_UVSLEEP(x) yield_sleep_uv(&x)

    int yield_sleep_uv_sec(unsigned int sec);

#define YIELD_UVSLEEP_SEC(x) yield_sleep_uv_sec(x)

    int yield_sleep_uv_msec(unsigned int msec);

    // SIGNAL

    int yield_signal_uv(const short signal, const struct timeval * const timeout);

#define YIELD_UVSIGNAL(x) yield_signal_uv(x, NULL)
#define YIELD_UVSIGNAL_TIMEOUT(x, timeout) yield_signal_uv(x, &timeout)

#ifdef THREADSAFE_SCHED

    // MULTITHREAD SCHEDULER

    void wakeup_uv_immediate(tid_t tid);
#define RESUME_UV_FROM_THREAD(tid) wakeup_uv_immediate(tid)

#endif

    // GENERAL

    typedef struct uv_genericinfo {
        uv_loop_t *loop;
        uv_timer_t thandle;
    } uv_genericinfo_t;

#define BASELOOP_TO_UVLOOP(x) (((struct uv_genericinfo *)x)->loop)

    extern struct sched_mainloop_interface const task_libuv;

#ifdef __cplusplus
}
#endif

#endif /* __TASKEV_H__ */
