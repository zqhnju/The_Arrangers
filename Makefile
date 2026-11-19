CC = gcc
CFLAGS ?= -std=c11 -O2 -Wall -Wextra
LDLIBS += -lm

ifeq ($(OS),Windows_NT)
TARGET = scheduler.exe
else
TARGET = scheduler
endif

SOURCES = main.c io.c cost.c anneal.c
HEADERS = io.h scheduler.h

$(TARGET): $(SOURCES) $(HEADERS)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ $(SOURCES) $(LDLIBS)
