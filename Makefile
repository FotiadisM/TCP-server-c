CC = gcc

OFLAGS = -O3
CFLAGS = -g3 -Wall -Wextra
LDFLAGS =

BDIR = bin
ODIR = build
IDIR = include
SDIR = src

EXECUTABLE = master
EXEC2 = whoServer
EXEC3 = whoClient

_DEPS = diseaseAggregator.h worker.h pipes.h stats.h list.h hashTable.h AVL.h patient.h date.h network.h
DEPS = $(patsubst %,$(IDIR)/%,$(_DEPS))

_OBJ = main.o diseaseAggregator.o worker.o pipes.o stats.o list.o hashTable.o AVL.o patient.o date.o network.o
OBJ = $(patsubst %,$(ODIR)/%,$(_OBJ))

$(ODIR)/%.o: $(SDIR)/%.c $(DEPS)
	$(CC) $(OFLAGS) $(CFLAGS) -c $< -o $@ $(LDFLAGS)

all: $(BDIR)/$(EXECUTABLE) $(BDIR)/$(EXEC2) $(BDIR)/$(EXEC3)

$(BDIR)/$(EXECUTABLE): $(OBJ)
	$(CC) $(OFLAGS) $(CFLAGS) $^ -o $@ $(LDFLAGS)

$(BDIR)/$(EXEC2): $(SDIR)/server.c $(SDIR)/fnctl.c $(IDIR)/fnctl.h $(SDIR)/pipes.c $(SDIR)/network.c $(SDIR)/list.c $(SDIR)/patient.c $(SDIR)/date.c
	$(CC) $(OFLAGS) $(CFLAGS) $^ -o $@ -lpthread

$(BDIR)/$(EXEC3): $(SDIR)/client.c $(SDIR)/pipes.c $(SDIR)/network.c
	$(CC) $(OFLAGS) $(CFLAGS) $^ -o $@ -lpthread

.PHONY: clean run1 run2 run3 valgrind1 valgrind2 valgrind3

run1:
	./$(BDIR)/$(EXEC2) -q 4000 -s 4010 -w 10 -b 10

run2:
	./$(BDIR)/$(EXECUTABLE) -w 4 -b 5 -i ./data

run3:
	./$(BDIR)/$(EXEC3) -q assets/queries.txt -w 2 -sp 4000 -sip 127.0.0.1

valgrind1:
	valgrind --leak-check=full --track-origins=yes --show-leak-kinds=all ./$(BDIR)/$(EXEC2) -q 4000 -s 4010 -w 10 -b 10

valgrind2:
	valgrind --leak-check=full --track-origins=yes --show-leak-kinds=all ./$(BDIR)/$(EXECUTABLE) -w 4 -b 5 -s 127.0.0.1 -p 4010 -i ./data 

valgrind3:
	valgrind --leak-check=full --track-origins=yes --show-leak-kinds=all ./$(BDIR)/$(EXEC3) -q assets/queries.txt -w 2 -sp 4000 -sip 127.0.0.1

clean:
	rm -f logs/log*
	rm -f pipes/r_* pipes/w_*
	rm -f $(ODIR)/*.o
	rm -f $(BDIR)/$(EXECUTABLE)
	rm -f $(BDIR)/$(EXEC2)
	rm -f $(BDIR)/$(EXEC3)