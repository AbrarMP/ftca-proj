INCDIR = -I.
DBG    = -g
OPT    = -O3
CPP    = g++
CFLAGS = $(DBG) $(OPT) $(INCDIR)
LINK   = -lm 

.cpp.o:
	$(CPP) $(CFLAGS) -c $< -o $@

all: main

main: main.cpp segment-image.h canny.h segment-graph.h disjoint-set.h
	$(CPP) $(CFLAGS) -o main main.cpp $(LINK)

clean:
	/bin/rm -f main *.o

clean-all: clean
	/bin/rm -f *~ 



