#
# Makefile - makefile to build the base system
#

# Locations

# Make environment
CC=gcc
CFLAGS=-c -Wall -Wextra -pedantic -Werror
#CFLAGS += -D_XOPEN_SOURCE=500
CFLAGS += -g 
LIBS= -lpthread -lcurses
# Suffix rules
.SUFFIXES: .c .o

.c.o:
	$(CC) $(CFLAGS)  -o $@ $<  
	
# Files
OBJECT_FILES= rake-c.o
OBJECT_FILES1= rakeserver.o
# Productions
all : rake-c rakeserver

rake-c : $(OBJECT_FILES)
	$(CC)  $(OBJECT_FILES) -o $@ $(LIBS)

rakeserver : $(OBJECT_FILES1)
	$(CC)  $(OBJECT_FILES1) -o $@ $(LIBS)	

clean : 
	rm -f rake-c rakeserver $(OBJECT_FILES)
	