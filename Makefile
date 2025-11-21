# Makefile for MAX7219 Driver and Histogram Library

# Kernel module variables
obj-m += max7219_driver.o
KDIR := /lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)

# Library variables
LIB_NAME = libhistogram.a
LIB_SRC = histogram_lib.c
LIB_OBJ = histogram_lib.o
LIB_HEADER = histogram_lib.h

# Compiler flags
CC = gcc
CFLAGS = -Wall -Wextra -O2 -std=c11
AR = ar
ARFLAGS = rcs

# Test program
TEST_PROG = test_histogram
TEST_SRC = test_histogram.c

.PHONY: all driver library test clean install uninstall help

all: driver library test

# Build kernel driver
driver:
	@echo "Building MAX7219 kernel driver..."
	make -C $(KDIR) M=$(PWD) modules

# Build static library
library: $(LIB_NAME)

$(LIB_OBJ): $(LIB_SRC) $(LIB_HEADER)
	$(CC) $(CFLAGS) -c $(LIB_SRC) -o $(LIB_OBJ)

$(LIB_NAME): $(LIB_OBJ)
	$(AR) $(ARFLAGS) $(LIB_NAME) $(LIB_OBJ)

# Build test program
test: $(TEST_PROG)

$(TEST_PROG): $(TEST_SRC) $(LIB_NAME) $(LIB_HEADER)
	$(CC) $(CFLAGS) $(TEST_SRC) -o $(TEST_PROG) -L. -lhistogram

# Install driver module
install: driver
	@echo "Installing MAX7219 driver..."
	sudo insmod max7219_driver.ko
	@echo "Driver installed. Check with: lsmod | grep max7219"
	@echo "Driver interface: /proc/max7219"

# Uninstall driver module
uninstall:
	@echo "Removing MAX7219 driver..."
	-sudo rmmod max7219_driver
	@echo "Driver removed."

# Clean build artifacts
clean:
	@echo "Cleaning build files..."
	make -C $(KDIR) M=$(PWD) clean
	rm -f $(LIB_OBJ) $(LIB_NAME) $(TEST_PROG)
	rm -f *.o *.ko *.mod.* *.symvers *.order .*.cmd
	rm -rf .tmp_versions
	@echo "Clean complete."

# Show help
help:
	@echo "MAX7219 Histogram Driver - Makefile"
	@echo "===================================="
	@echo ""
	@echo "Targets:"
	@echo "  all        - Build driver, library, and test program (default)"
	@echo "  driver     - Build kernel driver module"
	@echo "  library    - Build static library (libhistogram.a)"
	@echo "  test       - Build test program"
	@echo "  install    - Install kernel driver"
	@echo "  uninstall  - Remove kernel driver"
	@echo "  clean      - Remove all build artifacts"
	@echo "  help       - Show this help message"
	@echo ""
	@echo "Usage examples:"
	@echo "  make                    # Build everything"
	@echo "  make install            # Install driver"
	@echo "  sudo ./test_histogram   # Run test (requires driver installed)"
	@echo "  make uninstall          # Remove driver"
	@echo "  make clean              # Clean up"