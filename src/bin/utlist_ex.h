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
 * File:   utlist_ex.h
 * Author: miki
 *
 * Created on 2 gennaio 2016, 13.59
 */

#ifndef UTLIST_EX_H
#define UTLIST_EX_H

#include "../lib/uthash/src/utlist.h"

#define CDL_POP_LAST(head, out)                                                \
  do {                                                                         \
    if ((head) != NULL) {                                                      \
      (out) = (head)->prev;                                                    \
      if ((out) == (head)) {                                                   \
        (head) = NULL;                                                         \
      } else {                                                                 \
        (head)->prev = (out)->prev;                                            \
        (head)->prev->next = (head);                                           \
      }                                                                        \
      (out)->prev = (out);                                                     \
      (out)->next = (out);                                                     \
    } else {                                                                   \
      (out) = NULL;                                                            \
    }                                                                          \
  } while (0)

#endif /* UTLIST_EX_H */

