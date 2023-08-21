ifndef NAVISERVER
    NAVISERVER  = /usr/local/ns
endif

#
# Module name
#
MOD      =  awssqs.so

#
# Objects to build.
#
MODOBJS     = library.o

MODLIBS  += -laws-cpp-sdk-core -laws-cpp-sdk-sqs

CFLAGS += -DUSE_NAVISERVER
CXXFLAGS += $(CFLAGS)

include  $(NAVISERVER)/include/Makefile.module