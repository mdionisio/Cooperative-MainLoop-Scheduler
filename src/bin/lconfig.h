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

#ifndef LCONFIG_H
#define LCONFIG_H

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

// set define THREADSAFE_SCHED if you want to have threadsafe scheduler
// to have more than one scheduler working on more than one thread
#ifdef HAVE_PTHREAD
#define THREADSAFE_SCHED 1
#else
#undef THREADSAFE_SCHED
#endif

// #define DEBUG_SCHED_TASK   to have log of task scheduling
#undef DEBUG_SCHED_TASK

// define DEBUG_SCHED_STACK_ALLOC   to have log of stack allocation
#undef DEBUG_SCHED_STACK_ALLOC

// define DEBUG_EV_TASK to have log of libevent
#undef DEBUG_EV_TASK

// define DEBUG_UV_TASK to have log of libevent
#undef DEBUG_UV_TASK

// define stack size
// if not defined it use SIGSTKSZ	/* recommended stack size */
#undef MALLOC_MEMORY_SIZE

#endif