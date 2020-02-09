
PROJDIR = $(CURDIR)/..
INCLUDE += -I$(PROJDIR)/include
INCLUDE += -I$(PROJDIR)/test

#---------------------------FOR CSP INTEGRATION---------------------------------
#path to source includes
INCLUDE += -I$(PROJDIR)/libcsp/include/csp
INCLUDE += -I$(PROJDIR)/libcsp/include
INCLUDE += -I$(PROJDIR)/libcsp/build/include
#-------------------------------------------------------------------------------


CC = gcc 

#---------------------------File Names---------------------------
CFILES += $(PROJDIR)/src/utils.c
CFILES +=  $(PROJDIR)/src/filesystem_funcs.c
CFILES += $(PROJDIR)/src/port.c
CFILES += $(PROJDIR)/src/server.c
CFILES += $(PROJDIR)/src/protocol_handler.c
CFILES += $(PROJDIR)/src/mib.c
CFILES += $(PROJDIR)/src/requests.c
CFILES += $(PROJDIR)/src/tasks.c
CFILES += $(PROJDIR)/src/file_delivery_app.c
CFILES += $(PROJDIR)/src/list.c
CFILES += $(PROJDIR)/src/packet.c


CFILES += $(PROJDIR)/test/packet_tests.c
CFILES += $(PROJDIR)/test/test.c
#CFILES += $(PROJDIR)/test/mock.c
#CFILES += $(PROJDIR)/test/request_tests.c
#CFILES += $(PROJDIR)/test/tasks_tests.c
#CFILES += $(PROJDIR)/test/list_tests.c

#CFILES += $(PROJDIR)/test/filesystem_tests.c
#CFILES += $(PROJDIR)/test/protocol_handler_tests.c
#CFILES += $(PROJDIR)/test/list_tests.c
#CFILES += $(PROJDIR)/test/server_tests.c


#entrypoint
MAIN = main

#---------------------------Compiler Warnings---------------------------
#CWARNS += -W
#CWARNS += -Wall
#CWARNS += -Werror
#CWARNS += -Wextra
#CWARNS += -Wformat
#CWARNS += -Wmissing-braces
#CWARNS += -Wno-cast-align
#CWARNS += -Wparentheses
#CWARNS += -Wshadow
#CWARNS += -Wno-sign-compare
#CWARNS += -Wswitch
CWARNS += -Wuninitialized
#CWARNS += -Wunknown-pragmas
#CWARNS += -Wunused-function
#CWARNS += -Wunused-label
#CWARNS += -Wunused-parameter
#CWARNS += -Wunused-value
#CWARNS += -Wunused-variable
#CWARNS += -Wmissing-prototypes

#---------------------------Libs---------------------------
LINKFLAGS = 
LIBS = -pthread
STATIC_FILES += $(PROJDIR)/libcsp/build/libcsp.a

#---------------------------Compiler flags---------------------------

#32 bit machines, leave this out if your machine is 64 bit
CFLAGS += -m32
CFLAGS += -g -UUSE_STDIO -D__GCC_POSIX__=1
CFLAGS += -pthread
CFLAGS += -DDEBUG=1

# MAX_NUMBER_OF_TASKS = max pthreads used in the POSIX port. 
# Default value is 64 (_POSIX_THREAD_THREADS_MAX), the minimum number required by POSIX.
CFLAGS += -DMAX_NUMBER_OF_TASKS=300
CFLAGS += $(INCLUDE) $(CWARNS)

#---------------------------Build---------------------------
OBJS_FILES = $(patsubst %.c, %.o, $(CFILES))

all: $(MAIN)

.PHONY: clean 

$(MAIN): $(OBJS_FILES) $(STATIC_FILES)

clean: 
	rm -f *.o $(MAIN)



