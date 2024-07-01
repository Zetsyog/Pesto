SUBDIRS=


if EXTERNAL_PLUTO
ISL_INCLUDE+=@ISL_INCLUDE@
OSL_INCLUDE+=@OSL_INCLUDE@
PLUTO_INCLUDE+=@PLUTO_INCLUDE@
else
ISL_INCLUDE+=-I$(top_srcdir)/pluto/isl/include/

OSL_INCLUDE+=-I$(top_srcdir)/pluto/openscop/include/

CLOOG_INCLUDE+=-I$(top_srcdir)/pluto/cloog-isl/include/

PLUTO_INCLUDE+=-I$(top_srcdir)/pluto/include/
PLUTO_LIBADD+=-lpluto
PLUTO_LDFLAGS+=-L$(top_builddir)/pluto/lib
endif



bin_PROGRAMS=attila

attila_SOURCES=\
    attila.cpp

attila_CPPFLAGS=$(OPT_FLAGS) $(DEBUG_FLAGS) \
    $(OSL_INCLUDE) \
    $(ISL_INCLUDE) \
    $(CLOOG_INCLUDE) \
    $(PLUTO_INCLUDE) \
    -I../include -I$(top_srcdir)/include 

attila_LDFLAGS=-static $(PLUTO_LDFLAGS)

attila_LDADD = \
   ../lib/libattila.la \
    $(PLUTO_LIBADD) \
   -lm -lgomp