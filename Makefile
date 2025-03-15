CC = qcc
CFLAGS = -Vgcc_ntox86_64 -g -Wall -O0

SRCDIR = src
OBJDIR = obj
BINDIR = bin

PROJECT = byzantine_orchestra

SRC = $(SRCDIR)/$(PROJECT).c
OBJ = $(patsubst $(SRCDIR)/%.c,$(OBJDIR)/%.o,$(SRC))
TARGET = $(BINDIR)/$(PROJECT)

LIBS = -pthread

$(shell mkdir -p $(OBJDIR) $(BINDIR))

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS)
	
$(OBJDIR)/%.o: $(SRCDIR)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf $(OBJDIR)/*.o $(TARGET)

run: $(TARGET)
	$(TARGET) 3

.PHONY: all clean run debug-paths