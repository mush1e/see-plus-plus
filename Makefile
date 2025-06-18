CXX := g++
CXXFLAGS := -Wall -Wextra -std=c++17 -lssl -lcrypto
SRC_DIR := src
BIN := see-plus-plus

# Find all .cpp files recursively in src directory
SOURCES := $(shell find $(SRC_DIR) -name "*.cpp")
OBJECTS := $(SOURCES:.cpp=.o)

.PHONY: all build run clean rebuild debug

all: build

rebuild: clean build run

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

debug:
	@echo "Sources: $(SOURCES)"
	@echo "Objects: $(OBJECTS)"