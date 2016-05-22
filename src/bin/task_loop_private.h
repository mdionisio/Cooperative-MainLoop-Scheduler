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
 * File:   task_loop_private.h
 * Author: Michele Dionisio
 *
 * Created on 10 January 2016, 20.06
 */

#ifndef TASK_LOOP_PRIVATE_H
#define TASK_LOOP_PRIVATE_H

#ifdef __cplusplus
extern "C" {
#endif

    struct sched_mainloop_interface {
        void *(*create)(void);
        void (*destroy)(void *);
        void (*stop)(void *);

        void (*event_poll)(void *);
    };

#ifdef __cplusplus
}
#endif

#endif /* TASK_LOOP_PRIVATE_H */

