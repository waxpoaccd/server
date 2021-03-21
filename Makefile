PLAT ?= none
PLATS = linux macosx

.PHONY: none $(PLATS) all clean

CC = gcc
PLAT ?= linux
CFLAGS = -g -O2 -Wall
#SHARED := -fPIC --shared
# for MAC OS X below
#SHARED := -fPIC -bundle -undefined dynamic_lookup

LUADIR = skynet/3rd/lua
LUA_CLIB_PATH ?= luaclib
LUA_CLIB = log protobuf

default :
	$(MAKE) $(PLAT)

none :
	@echo "Please do 'make PLATFORM' where PLATFORM is one of these:"
	@echo "   $(PLATS)"

linux : PLAT = linux
linux : SHARED := -fPIC --shared
linux : PROTOSH := ./protogenpb.sh

macosx : PLAT = macosx
macosx : SHARED := -fPIC -bundle -undefined dynamic_lookup
macosx : PROTOSH := ./protogenpb-mac.sh

linux macosx :
	@echo $(SHARED)
	$(MAKE) all SHARED="$(SHARED)" PLAT="$(PLAT)" PROTOSH="$(PROTOSH)"

all : \
  $(foreach v, $(LUA_CLIB), $(LUA_CLIB_PATH)/$(v).so) \
	proto


proto :

	#cd ./protocol; sh $(PROTOSH); cd ../;

$(LUA_CLIB_PATH) :
	mkdir $(LUA_CLIB_PATH)

$(LUA_CLIB_PATH)/log.so : lualib-src/lua-log.c | $(LUA_CLIB_PATH)
	$(CC) $(CFLAGS) $(SHARED) $^ -o $@ -I$(LUADIR)

$(LUA_CLIB_PATH)/protobuf.so : | $(LUA_CLIB_PATH)
	cd lualib-src/pbc && $(MAKE) lib && cd binding/lua53 && $(MAKE) $(PLAT) && cd ../../../.. && cp lualib-src/pbc/binding/lua53/protobuf.so $@


clean :
	rm -rf luaclib
	cd lualib-src/pbc && $(MAKE) clean
	cd lualib-src/pbc/binding/lua53 && $(MAKE) clean
