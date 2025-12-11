# Makefile for OS Time Management System

CXX = g++
CXXFLAGS = -std=c++11 -Wall
LIBS = -lsfml-graphics -lsfml-window -lsfml-system -pthread
SRC_DIR = src
BUILD_DIR = build
TARGET = $(BUILD_DIR)/time_app

# Source files
SOURCES = $(SRC_DIR)/main.cpp

# Default target
all: $(TARGET)

# Build the application
$(TARGET): $(SOURCES)
	@mkdir -p $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) $(SOURCES) -o $(TARGET) $(LIBS)
	@echo "Build complete! Run with: ./$(TARGET)"

# Clean build artifacts
clean:
	rm -rf $(BUILD_DIR)/*
	@echo "Clean complete!"

# Run the application
run: $(TARGET)
	./$(TARGET)

.PHONY: all clean run
