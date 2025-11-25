CC      = mpicc
CFLAGS  = -Wall -O2 -I..
LDFLAGS = -L..
LDLIBS  = -lm -ljpeg -lhistogram

SRC_DIR = src
BIN_DIR = ejecutables

TARGETS = $(BIN_DIR)/demo_cluster $(BIN_DIR)/sobel_cluster

all: $(TARGETS)

$(BIN_DIR):
	mkdir -p $(BIN_DIR)

$(BIN_DIR)/demo_cluster: $(SRC_DIR)/demo_cluster.c | $(BIN_DIR)
	$(CC) $(CFLAGS) $< -o $@

$(BIN_DIR)/sobel_cluster: $(SRC_DIR)/sobel_cluster.c $(SRC_DIR)/image_utils.c | $(BIN_DIR)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS) $(LDLIBS)

clean:
	rm -rf $(BIN_DIR)
