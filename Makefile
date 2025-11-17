# Simple Makefile for a small C++ project

CXX := g++
CXXFLAGS := -std=c++17 -O2 -Wall -Wextra

SRCDIR := src
BINDIR := bin
TARGET := $(BINDIR)/main

SRCS := $(wildcard $(SRCDIR)/*.cpp)
OBJS := $(patsubst $(SRCDIR)/%.cpp,$(BINDIR)/%.o,$(SRCS))

.PHONY: all build clean run
all: build

build: $(TARGET)

$(BINDIR):
	@mkdir -p $(BINDIR)

$(BINDIR)/%.o: $(SRCDIR)/%.cpp | $(BINDIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(TARGET): $(OBJS) | $(BINDIR)
	$(CXX) $(CXXFLAGS) $^ -o $@

run: build
	$(TARGET)

clean:
	rm -rf $(BINDIR)/*
