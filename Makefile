CC=gcc
CFLAGS=-Wall
LDFLAGS=-ldl -pthread
BIN_DIR=./bin
OBJ_DIR=$(BIN_DIR)/obj
EXEC=tesh
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

debug: CFLAGS += -g -DDEBUG
debug: $(EXEC)

static_readline: CFLAGS+= -DREADLINE_STATIC_LOAD
static_readline: LDFLAGS+= -lreadline
static_readline: $(EXEC)

debug_static_readline: CFLAGS+= -g -DDEBUG -DREADLINE_STATIC_LOAD
debug_static_readline: LDFLAGS+= -lreadline
debug_static_readline: $(EXEC)
