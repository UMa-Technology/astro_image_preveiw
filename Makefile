MAKEFLAGS += -j4

CXX = g++
CC = gcc
CXXFLAGS = -std=c++17 -Wall -Wextra -Werror -pedantic -O2
CFLAGS = -Wall -Wextra -Werror -pedantic -O2
INCLUDES = -I. \
           -I./lib \
           -I/usr/local/include \
           -I/usr/include \
           -I/usr/include/tiff \
           -I/tmp/lib

LDFLAGS = -L/usr/local/lib -L/tmp/lib
LIBS = -lraw -lz -ljpeg -llz4 -lstdc++fs -pthread -ltiff -lauth -lsodium
BUILD_DIR = build
TARGET = $(BUILD_DIR)/viewer

CXX_SRCS = main.cpp preview.cpp stretch.cpp
C_SRCS = fits.c xisf.c tiff.c raw.c lib/cJSON.c lib/xml.c

CXX_OBJS = $(CXX_SRCS:%.cpp=$(BUILD_DIR)/%.o)
C_OBJS = $(C_SRCS:%.c=$(BUILD_DIR)/%.o)
OBJS = $(CXX_OBJS) $(C_OBJS)

all: $(TARGET)

$(shell mkdir -p $(BUILD_DIR)/lib $(BUILD_DIR))

$(TARGET): $(OBJS)
	@echo "Linking viewer..."
	$(CXX) $(OBJS) -o $@ $(LDFLAGS) $(LIBS)
	@echo "Build completed: $(TARGET)"

$(BUILD_DIR)/%.o: %.cpp
	@mkdir -p $(dir $@)
	@echo "Compiling $<..."
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

$(BUILD_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	@echo "Compiling $<..."
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

$(BUILD_DIR)/lib/%.o: lib/%.c
	@mkdir -p $(dir $@)
	@echo "Compiling $< (third-party)..."
	$(CC) -Wall -g -O2 -Wno-type-limits -Wno-unused-parameter -Wno-pointer-sign $(INCLUDES) -c $< -o $@

clean:
	@echo "Cleaning build directory..."
	rm -rf $(BUILD_DIR)

check_deps:
	@echo "Checking system dependencies..."
	@if ! pkg-config --exists libraw; then \
		echo "Error: libraw not found. Please install libraw development package."; \
		echo "  Ubuntu/Debian: sudo apt-get install libraw-dev"; \
		exit 1; \
	fi
	@if ! pkg-config --exists libjpeg; then \
		echo "Error: libjpeg not found. Please install libjpeg development package."; \
		echo "  Ubuntu/Debian: sudo apt-get install libjpeg-dev"; \
		exit 1; \
	fi
	@if ! pkg-config --exists liblz4; then \
		echo "Error: liblz4 not found. Please install lz4 development package."; \
		echo "  Ubuntu/Debian: sudo apt-get install liblz4-dev"; \
		echo "  CentOS/RHEL: sudo yum install lz4-devel"; \
		exit 1; \
	fi
	@if ! pkg-config --exists libtiff-4; then \
		echo "Error: libtiff not found. Please install libtiff development package."; \
		echo "  Ubuntu/Debian: sudo apt-get install libtiff-dev"; \
		exit 1; \
	fi
	@echo "All dependencies found."

install_deps_ubuntu:
	@echo "Installing dependencies for Ubuntu/Debian..."
	sudo apt-get update
	sudo apt-get install -y libraw-dev libjpeg-dev liblz4-dev libtiff-dev build-essential

.PHONY: all clean check_deps install_deps_ubuntu