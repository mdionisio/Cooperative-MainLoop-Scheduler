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
 * File:   unused.h
 * Author: Michele Dionisio
 *
 * Created on 24 October 2015, 17.14
 */

#ifndef UNUSED_H
#define UNUSED_H

#ifdef __cplusplus
extern "C" {
#endif

#define UNUSED(x) (void)(x)

#ifdef __GNUC__
#define UNUSED_PARAMETER(x) UNUSED_ ## x __attribute__((__unused__))
#else
#define UNUSED_PARAMETER(x) UNUSED_ ## x
#endif

#ifdef __GNUC__
#define UNUSED_FUNCTION(x) __attribute__((__unused__)) UNUSED_ ## x
#else
#define UNUSED_FUNCTION(x) UNUSED_ ## x
#endif


#ifdef __cplusplus
}
#endif

#endif /* UNUSED_H */

