ifndef NAVISERVER
    NAVISERVER  = /usr/local/ns
endif

#
# Module name
#
MOD      =  libtaws.so

#
# Objects to build.
#
MODOBJS     = src/aws-sdk-tcl-s3/library.o

MODLIBS  += -laws-cpp-sdk-core -laws-cpp-sdk-s3

CFLAGS += -DUSE_NAVISERVER
CXXFLAGS += $(CFLAGS)

include  $(NAVISERVER)/include/Makefile.module