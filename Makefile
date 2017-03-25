TARGETS=		sudoku

CC=				g++

INCLUDE_PATH=
HDRS=			sudoku.h
OBJS=
EXT_OBJS=
EXT_LIBS=		

GENLIB=			../genlib
INCLUDE_PATH+=	-I$(GENLIB)
HDRS+=			$(GENLIB)/Common.h
EXT_OBJS+=		$(GENLIB)/Common.o
EXT_LIBS+=		

# CLI
HDRS+=			$(GENLIB)/CLI.h
EXT_OBJS+=		$(GENLIB)/CLI.o
EXT_LIBS+=		


CFLAGS=			-g $(INCLUDE_PATH)

all:			$(TARGETS)

%.o:			%.cpp $(HDRS)
	$(CC) $(CFLAGS) -c -o $@ $*.cpp

OBJS+=			sudoku.o main.o Permutator.o

sudoku:			$(OBJS) $(HDRS)
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(EXT_OBJS) $(EXT_LIBS)

clean:
	rm -f *.o $(TARGETS)

tar:
	tar cvfz sudoku.tar.gz *.cpp *.h Makefile *.txt
