CXX := g++
CXXFLAGS := -Wall -Wextra -std=c++17 -O2 -pthread
DEBUG_FLAGS := -g -DDEBUG -fsanitize=address
SRC_DIR := src
BIN := see-plus-plus

# Find all .cpp files recursively in src directory
SOURCES := $(shell find $(SRC_DIR) -name "*.cpp")
OBJECTS := $(SOURCES:.cpp=.o)

.PHONY: all build run clean rebuild debug test

all: build

rebuild: clean build

build: $(BIN)

$(BIN): $(OBJECTS)
	$(CXX) $(CXXFLAGS) -o $@ $^

# Pattern rule for compiling .cpp files to .o files
%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

run: build
	./$(BIN)

clean:
	rm -f $(BIN) $(OBJECTS)

# Debug build with AddressSanitizer
debug: CXXFLAGS += $(DEBUG_FLAGS)
debug: clean $(BIN)

# Run with debug
debug-run: debug
	./$(BIN)

# Format code (requires clang-format)
format:
	find $(SRC_DIR) -name "*.cpp" -o -name "*.hpp" | xargs clang-format -i

# Show build info
info:
	@echo "Sources: $(SOURCES)"
	@echo "Objects: $(OBJECTS)"
	@echo "Compiler: $(CXX)"
	@echo "Flags: $(CXXFLAGS)"

# Simple load test (requires curl)
test: build
	@echo "Starting server in background..."
	@./$(BIN) &
	@SERVER_PID=$$!; \
	sleep 2; \
	echo "Running basic tests..."; \
	curl -s http://localhost:8080/ > /dev/null && echo "✅ Root endpoint works" || echo "❌ Root endpoint failed"; \
	curl -s http://localhost:8080/hello > /dev/null && echo "✅ Hello endpoint works" || echo "❌ Hello endpoint failed"; \
	curl -s http://localhost:8080/api/status > /dev/null && echo "✅ JSON endpoint works" || echo "❌ JSON endpoint failed"; \
	echo "Stopping server..."; \
	kill $$SERVER_PID