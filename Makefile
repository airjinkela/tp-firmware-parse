
BASEDIR=$(shell pwd)

BUILDDIR=$(BASEDIR)/build
SRCDIR=$(BASEDIR)/src

SRCS:=$(wildcard $(SRCDIR)/*.c)
OBJS:=$(patsubst $(SRCDIR)/%.c,$(BUILDDIR)/%.o,$(SRCS))

CFLAGS += -Wall
LDFLAGS += -static


all: main

main: $(OBJS)
	$(CC) $(OBJS) $(LDFLAGS) -o $@

$(BUILDDIR):
	mkdir -p $(BUILDDIR)

$(BUILDDIR)/%.o: $(SRCDIR)/%.c | $(BUILDDIR)
	$(CC) $(CFLAGS) -c $< -o $@

.PHONY: all
