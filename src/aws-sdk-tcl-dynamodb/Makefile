ifndef NAVISERVER
    NAVISERVER  = /usr/local/ns
endif

#
# Module name
#
MOD      =  awsdynamodb.so

#
# Objects to build.
#
MODOBJS     = library.o

MODLIBS  += -laws-cpp-sdk-core -laws-cpp-sdk-dynamodb

CFLAGS += -DUSE_NAVISERVER
CXXFLAGS += $(CFLAGS)

include  $(NAVISERVER)/include/Makefile.module