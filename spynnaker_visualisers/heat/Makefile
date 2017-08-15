CXX = g++		# Set a default compiler for C++ 
CC = g++		# Necessary for correct linking

INCLUDEDIRS += /opt/X11/include /opt/local/include
LIBDIRS += /opt/X11/lib /opt/local/lib
LIBRARIES += GLU GL glut pthread config

FLAGS += -Wall -Wextra
CXXFLAGS += $(INCLUDEDIRS:%=-I%) -std=c++11 $(FLAGS)
LDFLAGS += $(LIBDIRS:%=-L%) $(LIBRARIES:%=-l%)

OBJS := visualiser.o

all: visualiser
visualiser: $(OBJS)

# Dependencies
visualiser.o: visualiser.cpp config.cpp display.cpp events.cpp menu.cpp \
	sdp.cpp state.h 

clean:
	-rm $(OBJS)

.PHONY: all clean
