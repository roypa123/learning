# Makefile for Linux / macOS / MSYS2 users.
#   make            build every chapter into bin/
#   make ch14_sphere
CXX      ?= g++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Iinclude -pthread

SOURCES  := $(wildcard chapters/*.cpp)
PROGRAMS := $(patsubst chapters/%.cpp,bin/%,$(SOURCES))
HEADERS  := $(wildcard include/pixel/*.h)

all: $(PROGRAMS)

bin/%: chapters/%.cpp $(HEADERS)
	@mkdir -p bin
	$(CXX) $(CXXFLAGS) $< -o $@

%: bin/%
	@true

clean:
	rm -rf bin

.PHONY: all clean
