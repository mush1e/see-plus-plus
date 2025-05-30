CXX := g++
CXXFLAGS := -Wall -Wextra -std=c++17
SRC_DIR := server
BIN := see-plus-plus

# Automatically find all .cpp files in the server directory
SOURCES := $(wildcard $(SRC_DIR)/*.cpp)
OBJECTS := $(SOURCES:.cpp=.o)

.PHONY: all build run clean

all: build

build: $(BIN)

$(BIN): $(OBJECTS)
	$(CXX) $(CXXFLAGS) -o $@ $^

$(SRC_DIR)/%.o: $(SRC_DIR)/%.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

run: build
	./$(BIN)

clean:
	rm -f $(BIN) $(OBJECTS)

debug:
	@echo "Sources: $(SOURCES)"
	@echo "Objects: $(OBJECTS)"