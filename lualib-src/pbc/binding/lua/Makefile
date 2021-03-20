CC = gcc
CFLAGS = -O2 -fPIC -Wall -shared
MAC_FLAGS = -undefined dynamic_lookup
LUADIR = /usr/local/include
TARGET = protobuf.so

.PHONY : all linux macosx clean

all : linux

linux : pbc-lua.c
	$(CC) $(CFLAGS) -o $(TARGET) -I../.. -I$(LUADIR) -L../../build $^ -lpbc

macosx : pbc-lua.c
	$(CC) $(CFLAGS) $(MAC_FLAGS) -o $(TARGET) -I../.. -I$(LUADIR) -L../../build $^ -lpbc

clean :
	rm -f $(TARGET)
