CC = gcc
CFLAGS = -Wall -pthread
OUTPUT = udp_forwarder
SRC = udp_forwarder.c

all: $(OUTPUT)

$(OUTPUT): $(SRC)
	$(CC) $(CFLAGS) -o $(OUTPUT) $(SRC)

clean:
	rm -f $(OUTPUT)
