ifndef NAVISERVER
    NAVISERVER  = /usr/local/ns
endif

#
# Module name
#
MOD      =  awsssm.so

#
# Objects to build.
#
MODOBJS     = library.o

MODLIBS  += -laws-cpp-sdk-core -laws-cpp-sdk-ssm

CFLAGS += -DUSE_NAVISERVER
CXXFLAGS += $(CFLAGS)

include  $(NAVISERVER)/include/Makefile.module