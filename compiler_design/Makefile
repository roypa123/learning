# Makefile for Linux / macOS / MSYS2 users.
#   make                 build every chapter into bin/
#   make ch07_lexer      build one chapter
#   make test            build and run the test suite
CXX      ?= g++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra -Wno-unused-parameter -Iinclude

SOURCES  := $(wildcard chapters/*.cpp)
PROGRAMS := $(patsubst chapters/%.cpp,bin/%,$(SOURCES))
HEADERS  := $(wildcard include/pebble/*.h)

all: $(PROGRAMS)

bin/%: chapters/%.cpp $(HEADERS)
	@mkdir -p bin
	$(CXX) $(CXXFLAGS) $< -o $@

%: bin/%
	@true

test: bin/run_tests
	./bin/run_tests

bin/run_tests: tests/run_tests.cpp $(HEADERS)
	@mkdir -p bin
	$(CXX) $(CXXFLAGS) $< -o $@

clean:
	rm -rf bin

.PHONY: all clean test
