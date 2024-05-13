ifndef NAVISERVER
    NAVISERVER  = /usr/local/ns
endif

#
# Module name
#
MOD      =  awslambda.so

#
# Objects to build.
#
MODOBJS     = library.o ../common/common.o

MODLIBS  += -laws-cpp-sdk-core -laws-cpp-sdk-lambda

CFLAGS += -DUSE_NAVISERVER
CXXFLAGS += $(CFLAGS)

include  $(NAVISERVER)/include/Makefile.module