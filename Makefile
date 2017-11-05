CC=gcc
CFLAGS=-Wall
LDFLAGS=-lreadline -pthread
BIN_DIR=./bin
OBJ_DIR=$(BIN_DIR)/obj
EXEC=$(BIN_DIR)/tesh
OBJ=$(addprefix $(OBJ_DIR)/, main.o tesh.o)

all: make_dir $(EXEC)

make_dir:
	mkdir -p $(BIN_DIR)
	mkdir -p $(OBJ_DIR)

$(EXEC): $(OBJ)
	$(CC) -o $@ $^ $(LDFLAGS)

$(OBJ_DIR)/%.o: %.c
	$(CC) -o $@ -c $< $(CFLAGS)

clean:
	rm -rf $(OBJ_DIR)/*.o

mrproper: clean
	rm -rf $(EXEC)
