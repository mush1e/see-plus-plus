CXX := g++
CXXFLAGS := -Wall -Wextra -std=c++17
SRC_DIR := server
BIN := see-plus-plus
SRC := $(SRC_DIR)/main.cpp

.PHONY: all build run clean

all: build

build:
	$(CXX) $(CXXFLAGS) -o $(BIN) $(SRC)

run: build
	./$(BIN)

clean:
	rm -f $(BIN)
