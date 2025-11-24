# ============================
#   MAX7219 DRIVER + HISTO LIB + CLIENT MPI
# ============================

# Kernel driver
obj-m += max7219_driver.o
KDIR := /lib/modules/$(shell uname -r)/build
PWD  := $(shell pwd)

# Library
LIB_NAME = libhistogram.a
LIB_SRC  = histogram_lib.c
LIB_OBJ  = histogram_lib.o

# STB implementation
STB_IMPL = stb_impl.o
STB_SRC  = stb_impl.c

# Client
CLIENT     = client
CLIENT_SRC = client.c

# Compiler flags
CC     = gcc
MPICC  = mpicc
CFLAGS = -Wall -Wextra -O2 -std=c11 -I./include

AR      = ar
ARFLAGS = rcs

.PHONY: all driver library client clean install uninstall

# ============================
# Default target
# ============================
all: driver library client

# ============================
# Driver build
# ============================
driver:
	@echo "Building kernel driver..."
	@[ -d "$(KDIR)" ] && make -C $(KDIR) M=$(PWD) modules || echo "Kernel headers not installed. Skipping driver build."

# ============================
# Static library
# ============================
library: $(LIB_NAME)

$(LIB_OBJ): $(LIB_SRC) histogram_lib.h
	$(CC) $(CFLAGS) -c $(LIB_SRC) -o $(LIB_OBJ)

$(LIB_NAME): $(LIB_OBJ)
	$(AR) $(ARFLAGS) $(LIB_NAME) $(LIB_OBJ)

# ============================
# STB implementation
# ============================
$(STB_IMPL): $(STB_SRC)
	$(CC) $(CFLAGS) -c $(STB_SRC) -o $(STB_IMPL)

# ============================
# Build client MPI
# ============================
client: $(CLIENT)

$(CLIENT): $(CLIENT_SRC) $(STB_IMPL) $(LIB_NAME)
	$(MPICC) $(CFLAGS) $(CLIENT_SRC) $(STB_IMPL) -o $(CLIENT) -L. -lhistogram -lm

# ============================
# Driver install / uninstall
# ============================
install: driver
	@echo "Installing driver..."
	-sudo insmod max7219_driver.ko
	@echo "Driver installed."

uninstall:
	@echo "Removing driver..."
	-sudo rmmod max7219_driver
	@echo "Driver removed."

# ============================
# Clean
# ============================
clean:
	@echo "Cleaning..."
	@[ -d "$(KDIR)" ] && make -C $(KDIR) M=$(PWD) clean || echo "Kernel headers not installed, skipping driver clean."
	rm -f *.o *.ko *.mod.* *.symvers *.order .*.cmd
	rm -f $(LIB_NAME) $(CLIENT)
	rm -rf .tmp_versions
	@echo "Done."
