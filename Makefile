INCDIR = -I.
DBG    = -g
OPT    = -O2 -static
CPP    = g++
CPPPPC = powerpc-e500v2-linux-gnuspe-g++
CFLAGS = $(DBG)  $(OPT) $(INCDIR)
LINK   = -lm 

.cpp.o:
	$(CPP) $(CFLAGS) -c $< -o $@

all: main mainsimics

main: main.cpp segment-image.h canny.h segment-graph.h disjoint-set.h
	$(CPP) $(CFLAGS) -o main main.cpp $(LINK)

mainsimics: main.cpp segment-image.h canny.h segment-graph.h disjoint-set.h
	$(CPPPPC) $(CFLAGS) -o mainsimics main.cpp $(LINK)

clean:
	/bin/rm -f main mainsimics *.o

clean-all: clean
	/bin/rm -f *~ 



