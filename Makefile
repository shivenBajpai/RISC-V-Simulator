SHELL=/bin/bash
CC = gcc
CCFLAGS = -g -Wno-deprecated-declarations
CLFLAGS = -g -lncurses

SRCDIR=src
OBJDIR=build
OUTDIR=bin
TARGET=riscv_sim

# DO NOT EDIT BELOW

SRCS=$(wildcard ./$(SRCDIR)/*.c) $(wildcard ./$(SRCDIR)/*/*.c)
OBJ_NAMES=$(patsubst %.c,%.o,$(SRCS))
OBJS=$(patsubst ./$(SRCDIR)%,./$(OBJDIR)%,$(OBJ_NAMES))
TARGET_PATH=./$(OUTDIR)/$(TARGET)

.PHONY: build
build: $(TARGET_PATH)
	@cp bin/$(TARGET) ./$(TARGET)

run: $(TARGET_PATH)
	@cd bin && ./$(TARGET)

debug: $(TARGET_PATH)
	@cd bin && ./$(TARGET) -d

test: $(TARGET_PATH)
	@cd tests && ./test.bash

$(TARGET_PATH): $(OBJS)
	@echo "Linking..."
	@$(CC) -o $(TARGET_PATH) $(OBJS) $(CLFLAGS) 
	@echo "Binary generated in /bin"

./build/%.o: ./$(SRCDIR)/%.c
	@echo "Compiling $<..."
	@$(CC) $(CCFLAGS) -c $< -o $@

.PHONY: report
report:
	@cd report && ./run.sh

.PHONY: clean
clean:
	@echo "Removing Build and Test files..."
	-@rm $(OBJS)
	-@rm ./$(OUTDIR)/$(TARGET)
	-@rm ./$(TARGET)
