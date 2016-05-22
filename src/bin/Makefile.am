
bin_PROGRAMS = schedevev schedevuv

schedevev_SOURCES = mainev.c task.c taskev.c taskuv.c
schedevuv_SOURCES = mainuv.c task.c taskev.c taskuv.c

schedevev_LDADD =
schedevuv_LDADD =

if PTHREAD
schedevev_LDADD += -lpthread
schedevuv_LDADD += -lpthread
endif

AM_CFLAGS = -Wall -I$(top_srcdir)/src/bin -I$(top_srcdir)/src/lib/uthash/include -I$(top_srcdir)/src/lib/libevent/include
AM_CPPFLAGS = -Wall -I$(top_srcdir)/src/bin -I$(top_srcdir)/src/lib/uthash/include -I$(top_srcdir)/src/lib/libevent/include
AM_CXXFLAGS = -Wall -I$(top_srcdir)/src/bin -I$(top_srcdir)/src/lib/uthash/include -I$(top_srcdir)/src/lib/libevent/include

if DEBUG
AM_CFLAGS += -g3 -ggdb3 -O0 -fstack-check
AM_CXXFLAGS += -g3 -ggdb3 -O0 -fstack-check
AM_CPPFLAGS += -g3 -ggdb3 -O0 -fstack-check
else
AM_CFLAGS += -O3
AM_CXXFLAGS += -O3
AM_CPPFLAGS += -O3
endif

if PROFILE
AM_CFLAGS += -pg
AM_CXXFLAGS += -pg
AM_CPPFLAGS += -pg
endif

schedevev_CFLAGS = $(LIBEVENT_CFLAGS) $(LIBEVENTPTHREAD_CFLAGS) $(LIBUV_CFLAGS)
schedevev_CPPFLAGS = $(LIBEVENT_CPPFLAGS) $(LIBEVENTPTHREAD_CPPFLAGS) $(LIBUV_CPPFLAGS)
schedevev_CXXFLAGS = $(LIBEVENT_CXXFLAGS) $(LIBEVENTPTHREAD_CXXFLAGS) $(LIBUV_CXXFLAGS)
schedevev_LDADD += $(LIBEVENT_LIBS) $(LIBEVENTPTHREAD_LIBS) $(LIBUV_LIBS)

schedevuv_CFLAGS = $(LIBEVENT_CFLAGS) $(LIBEVENTPTHREAD_CFLAGS) $(LIBUV_CFLAGS)
schedevuv_CPPFLAGS = $(LIBEVENT_CPPFLAGS) $(LIBEVENTPTHREAD_CPPFLAGS) $(LIBUV_CPPFLAGS)
schedevuv_CXXFLAGS = $(LIBEVENT_CXXFLAGS) $(LIBEVENTPTHREAD_CXXFLAGS) $(LIBUV_CXXFLAGS)
schedevuv_LDADD += $(LIBEVENT_LIBS) $(LIBEVENTPTHREAD_LIBS) $(LIBUV_LIBS)
