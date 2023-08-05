ifndef NAVISERVER
    NAVISERVER  = /usr/local/ns
endif

#
# Module name
#
MOD      =  awss3.so

#
# Objects to build.
#
MODOBJS     = library.o

MODLIBS  += -laws-cpp-sdk-core -laws-cpp-sdk-s3

CFLAGS += -DUSE_NAVISERVER
CXXFLAGS += $(CFLAGS)

include  $(NAVISERVER)/include/Makefile.module