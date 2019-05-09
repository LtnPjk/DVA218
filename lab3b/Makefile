CC=gcc
CFLAGS=-I.
DEPS = 
OBJ = lab3.o

%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

lab3: $(OBJ) 
	$(CC) -o $@ $^ $(CFLAGS)
