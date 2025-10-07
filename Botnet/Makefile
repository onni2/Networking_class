# Compiler and flags
CXX = g++
CXXFLAGS = -std=c++11 -Wall -Wextra -pthread -g
LDFLAGS = -pthread

# Your group number (CHANGE THIS!)
GROUP_NUM = 1

# Target executables
SERVER = tsamgroup$(GROUP_NUM)
CLIENT = client

# Source files
SERVER_SRC = server.cpp protocol.cpp
CLIENT_SRC = client.cpp protocol.cpp

# Object files
SERVER_OBJ = $(SERVER_SRC:.cpp=.o)
CLIENT_OBJ = $(CLIENT_SRC:.cpp=.o)

# Header files
HEADERS = protocol.h

# Default target
all: $(SERVER) $(CLIENT)

# Build server
$(SERVER): $(SERVER_OBJ)
	$(CXX) $(LDFLAGS) -o $(SERVER) $(SERVER_OBJ)
	@echo "Server built successfully: $(SERVER)"

# Build client
$(CLIENT): $(CLIENT_OBJ)
	$(CXX) $(LDFLAGS) -o $(CLIENT) $(CLIENT_OBJ)
	@echo "Client built successfully: $(CLIENT)"

# Compile source files to object files
%.o: %.cpp $(HEADERS)
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Clean build artifacts
clean:
	rm -f $(SERVER) $(CLIENT) *.o *.log core
	@echo "Cleaned build artifacts"

# Clean and rebuild
rebuild: clean all

# Run server (example - adjust port as needed)
run-server: $(SERVER)
	./$(SERVER) 4044

# Run client (example - adjust IP and port as needed)
run-client: $(CLIENT)
	./$(CLIENT) 127.0.0.1 4044

# Help target
help:
	@echo "Available targets:"
	@echo "  all          - Build both server and client (default)"
	@echo "  $(SERVER)    - Build server only"
	@echo "  $(CLIENT)    - Build client only"
	@echo "  clean        - Remove built files"
	@echo "  rebuild      - Clean and build everything"
	@echo "  run-server   - Build and run server on port 4044"
	@echo "  run-client   - Build and run client connecting to localhost:4044"
	@echo "  help         - Show this help message"

# Phony targets (not actual files)
.PHONY: all clean rebuild run-server run-client help
