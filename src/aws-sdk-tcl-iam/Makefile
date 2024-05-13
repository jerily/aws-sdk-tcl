ifndef NAVISERVER
    NAVISERVER  = /usr/local/ns
endif

#
# Module name
#
MOD      =  awsiam.so

#
# Objects to build.
#
MODOBJS     = library.o ../common/common.o

MODLIBS  += -laws-cpp-sdk-core -laws-cpp-sdk-iam

CFLAGS += -DUSE_NAVISERVER
CXXFLAGS += $(CFLAGS)

include  $(NAVISERVER)/include/Makefile.module