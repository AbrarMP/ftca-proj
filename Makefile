INCDIR = -I.
DBG    = -g
OPT    = -o2 -static
CPP    = g++
CPPPPC = powerpc-e500v2-linux-gnuspe-g++
CFLAGS = $(DBG)  $(OPT) $(INCDIR)
LINK   = -lm 

.cpp.o:
	$(CPP) $(CFLAGS) -c $< -o $@

all: main mainsimics mainnft mainsimicsnft

main: main.cpp segment-image.h canny.h segment-graph.h disjoint-set.h
	$(CPP) $(CFLAGS) -o main main.cpp $(LINK)

mainnft: main_NFT.cpp segment-image.h canny.h segment-graph.h disjoint-set.h
	$(CPP) $(CFLAGS) -o mainnft main_NFT.cpp $(LINK)

mainsimics: main.cpp segment-image.h canny.h segment-graph.h disjoint-set.h
	$(CPPPPC) $(CFLAGS) -o mainsimics main.cpp $(LINK)

mainsimicsnft: main_NFT.cpp segment-image.h canny.h segment-graph.h disjoint-set.h
	$(CPPPPC) $(CFLAGS) -o mainsimicsnft main_NFT.cpp $(LINK)

clean:
	/bin/rm -f main mainsimics mainsimicsnft mainnft *.o

clean-all: clean
	/bin/rm -f *~ 



