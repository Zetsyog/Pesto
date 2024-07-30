SUBDIRS=

ISL_INCLUDE+=-I$(top_srcdir)/pluto/isl/include/

OSL_INCLUDE+=-I$(top_srcdir)/pluto/openscop/include/

CLOOG_INCLUDE+=-I$(top_srcdir)/pluto/cloog-isl/include/

CLAN_INCLUDE+=-I$(top_srcdir)/pluto/clan/include/
CLAN_LIBADD+=$(top_srcdir)/pluto/clan/libclan.la

PLUTO_INCLUDE+=-I$(top_srcdir)/pluto/include/
PLUTO_LIBADD+=-lpluto
PLUTO_LDFLAGS+=-L$(top_builddir)/pluto/lib

bin_PROGRAMS=pesto

EXTRA_pesto_DEPENDENCIES = ../lib/libpesto.la

pesto_SOURCES= \
    Options.cpp \
    main.cpp

pesto_CPPFLAGS=$(OPT_FLAGS) $(DEBUG_FLAGS) \
    $(CLOOG_INCLUDE) \
    $(ISL_INCLUDE) \
    $(OSL_INCLUDE) \
    $(CLAN_INCLUDE) \
    $(PLUTO_INCLUDE) \
    -I../include -I$(top_srcdir)/include 

pesto_LDFLAGS=-static $(PLUTO_LDFLAGS)

pesto_LDADD = \
   ../lib/libpesto.la \
    $(PLUTO_LIBADD) \
    $(CLAN_LIBADD) \
   -lm -lgomp