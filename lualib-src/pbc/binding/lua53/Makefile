CC = gcc
CFLAGS = -O2 -fPIC -Wall -shared
MAC_FLAGS = -undefined dynamic_lookup
LUADIR = ../../../../skynet/3rd/lua
TARGET = protobuf.so

.PHONY : all linux macosx clean

all : linux

linux : pbc-lua53.c
	$(CC) $(CFLAGS) -o $(TARGET) -I../.. -I$(LUADIR) -L../../build $^ -lpbc

macosx : pbc-lua53.c
	$(CC) $(CFLAGS) $(MAC_FLAGS) -o $(TARGET) -I../.. -I$(LUADIR) -L../../build $^ -lpbc

clean :
	rm -f $(TARGET)
