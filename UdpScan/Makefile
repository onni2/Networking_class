# Makefile for UDP Scanner Project
# Compiler and flags
CXX = g++
CXXFLAGS = -std=c++20 -Wall -Wextra -O2
LDFLAGS = 

# Target executables
TARGETS = scanner puzzlesolver

# Source files
SCANNER_SRC = scanner.cpp
PUZZLESOLVER_SRC = puzzlesolver.cpp

# Object files
SCANNER_OBJ = $(SCANNER_SRC:.cpp=.o)
PUZZLESOLVER_OBJ = $(PUZZLESOLVER_SRC:.cpp=.o)

# Default target
all: $(TARGETS)

# Build scanner executable
scanner: $(SCANNER_OBJ)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

# Build puzzlesolver executable
puzzlesolver: $(PUZZLESOLVER_OBJ)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

# Compile source files to object files
%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Clean build files
clean:
	rm -f $(SCANNER_OBJ) $(PUZZLESOLVER_OBJ) $(TARGETS)

# Clean and rebuild everything
rebuild: clean all

# Install targets (optional - adjust paths as needed)
install: $(TARGETS)
	cp scanner /usr/local/bin/
	cp puzzlesolver /usr/local/bin/

# Uninstall targets
uninstall:
	rm -f /usr/local/bin/scanner
	rm -f /usr/local/bin/puzzlesolver

# Debug builds with debugging symbols
debug: CXXFLAGS += -g -DDEBUG
debug: $(TARGETS)

# Release builds with optimizations
release: CXXFLAGS += -O3 -DNDEBUG
release: $(TARGETS)

# Run scanner with example parameters
run-scanner: scanner
	./scanner 127.0.0.1 8001 8002 8003 8004 1

# Run puzzlesolver with example parameters
run-puzzlesolver: puzzlesolver
	./puzzlesolver

# Help target
help:
	@echo "Available targets:"
	@echo "  all          - Build all executables (default)"
	@echo "  scanner      - Build scanner executable"
	@echo "  puzzlesolver - Build puzzlesolver executable"
	@echo "  clean        - Remove build files"
	@echo "  rebuild      - Clean and build all"
	@echo "  debug        - Build with debug symbols"
	@echo "  release      - Build optimized release version"
	@echo "  install      - Install executables to /usr/local/bin"
	@echo "  uninstall    - Remove installed executables"
	@echo "  run-scanner  - Build and run scanner with example args"
	@echo "  run-puzzlesolver - Build and run puzzlesolver"
	@echo "  help         - Show this help message"

# Phony targets
.PHONY: all clean rebuild install uninstall debug release run-scanner run-puzzlesolver help
