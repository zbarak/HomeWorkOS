#makefile:
# 1. Variables - Match your folder names exactly (Case Sensitive!)
CC = gcc
CFLAGS = -Wall -Wextra -IHeader
SRC_DIR = Src
SERVER_TARGET = hw3server
CLIENT_TARGET = hw3client

# 2. Default target (Builds both as requested)
all: $(SERVER_TARGET) $(CLIENT_TARGET)

# 3. Build the Server
# Assumes server.c contains the main() for the server
$(SERVER_TARGET): $(SRC_DIR)/server.c
	$(CC) $(CFLAGS) $(SRC_DIR)/server.c -o $(SERVER_TARGET)

# 4. Build the Client
# Assumes client.c contains the main() for the client
$(CLIENT_TARGET): $(SRC_DIR)/client.c
	$(CC) $(CFLAGS) $(SRC_DIR)/client.c -o $(CLIENT_TARGET)

# 5. Clean rule (Corrected: No dependencies needed)
clean:
	rm -f $(SERVER_TARGET) $(CLIENT_TARGET)