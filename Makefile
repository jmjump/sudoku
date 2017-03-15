TARGETS=		sudoku

CC=				g++

INCLUDES=		
HDRS=			
OBJS=
EXT_LIBS=		

TEMPLATE=		../template
INCLUDES+=		-I$(TEMPLATE)
HDRS+=			$(TEMPLATE)/Common.h
OBJS+=			$(TEMPLATE)/Common.o
EXT_LIBS+=		

# CLI
HDRS+=			$(TEMPLATE)/CLI.h
OBJS+=			$(TEMPLATE)/CLI.o
EXT_LIBS+=		


CFLAGS=			-g $(INCLUDES)

all:			$(TARGETS)

%.o:			%.cpp $(HDRS)
	$(CC) $(CFLAGS) -c -o $@ $*.cpp

OBJS+=			sudoku.o

sudoku:			$(OBJS) $(HDRS)
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(EXT_LIBS)

clean:
	rm -f *.o $(TARGETS)

tar:
	tar cvfz sudoku.tar.gz *.cpp Makefile *.txt
