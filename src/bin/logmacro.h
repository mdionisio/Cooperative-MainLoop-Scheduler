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
 * File:   logmacro.h
 * Author: Michele Dionisio
 *
 * Created on 23 August 2015, 8.29
 */

#ifndef __LOGMACRO_H__
#define __LOGMACRO_H__

#ifdef __cplusplus
extern "C" {
#endif

#ifndef __FILENAME__
#define __FILENAME__ __FILE__
#endif

#ifdef FAKELOG

#define BEGINLOG()
#define SIMPLELOG(format, ...)
#define ENDLOG()

#else

#include <stdio.h>

#define BEGINLOG() printf("%s:%d (%s) ", __FILENAME__, __LINE__, __FUNCTION__)
#define SIMPLELOG(format, ...) printf(format, ##__VA_ARGS__ )
#define ENDLOG() printf("\n")

#endif

#define LOG(format, ...) BEGINLOG(); SIMPLELOG(format, ##__VA_ARGS__); ENDLOG();

#ifdef __cplusplus
}
#endif

#endif /* __LOGMACRO_H__ */

