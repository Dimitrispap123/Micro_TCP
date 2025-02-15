#local_client: make local_client
#local_server: make local_server 

#tcp_client: make bandwidth_tcp_client
#tcp_server: make bandwidth_tcp_server

#micro_tcp_client: make bandwidth_microtcp_client
#micro_tcp_server: make bandwidth_microtcp_server

#clean: make clean


# Compiler
CC = gcc
CFLAGS = -lpthread -DDEBUG

# Paths
LIB_DIR = ../lib
LIB_SRC = $(LIB_DIR)/microtcp.c
LIB_HEADER = $(LIB_DIR)/microtcp.h

# Executables
LOCAL_CLIENT = microtcp_client
LOCAL_SERVER = microtcp_server
BANDWIDTH_TEST = bandwidth_test

# Test source files
LOCAL_CLIENT_SRC = test_microtcp_client.c
LOCAL_SERVER_SRC = test_microtcp_server.c
BANDWIDTH_SRC = bandwidth_test.c

# Default rule
all: local_client local_server bandwidth_tcp bandwidth_microtcp

# Local Tests (Compile & Run)
local_client:
	$(CC) -o $(LOCAL_CLIENT) $(LOCAL_CLIENT_SRC) $(LIB_SRC) $(CFLAGS)
	./$(LOCAL_CLIENT)

local_server:
	$(CC) -o $(LOCAL_SERVER) $(LOCAL_SERVER_SRC) $(LIB_SRC) $(CFLAGS)
	./$(LOCAL_SERVER)

# Bandwidth Tests (Compile & Run)
bandwidth_tcp_server:
	$(CC) -o $(BANDWIDTH_TEST) $(BANDWIDTH_SRC) $(LIB_SRC) -I$(LIB_HEADER) $(CFLAGS)
	./$(BANDWIDTH_TEST) -s -p 8080 -f received_file.txt

bandwidth_tcp_client:
	$(CC) -o $(BANDWIDTH_TEST) $(BANDWIDTH_SRC) $(LIB_SRC) -I$(LIB_HEADER) $(CFLAGS)
	./$(BANDWIDTH_TEST) -p 8080 -a 127.0.0.1 -f test_file.txt

bandwidth_microtcp_server:
	$(CC) -o $(BANDWIDTH_TEST) $(BANDWIDTH_SRC) $(LIB_SRC) -I$(LIB_HEADER) $(CFLAGS) -DDEBUG
	./$(BANDWIDTH_TEST) -s -m -p 8080 -f received_file.txt

bandwidth_microtcp_client:
	$(CC) -o $(BANDWIDTH_TEST) $(BANDWIDTH_SRC) $(LIB_SRC) -I$(LIB_HEADER) $(CFLAGS) -DDEBUG
	./$(BANDWIDTH_TEST) -m -p 8080 -a 127.0.0.1 -f test_file.txt

# Clean build files
clean:
	rm -f $(LOCAL_CLIENT) $(LOCAL_SERVER) $(BANDWIDTH_TEST)
