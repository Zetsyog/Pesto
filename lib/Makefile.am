SUBDIRS=

lib_LTLIBRARIES = libpesto.la



if EXTERNAL_PLUTO
ISL_INCLUDE+=@ISL_INCLUDE@
OSL_INCLUDE+=@OSL_INCLUDE@
PLUTO_INCLUDE+=@PLUTO_INCLUDE@
else
ISL_INCLUDE+=-I$(top_srcdir)/pluto/isl/include/
ISL_LIBADD+=$(top_builddir)/pluto/isl/libisl.la

OSL_INCLUDE+=-I$(top_srcdir)/pluto/openscop/include/
OSL_LIBADD+=$(top_builddir)/pluto/openscop/libosl.la

CLOOG_INCLUDE+=-I$(top_srcdir)/pluto/cloog-isl/include/
CLOOG_LIBADD+=$(top_builddir)/pluto/cloog-isl/libcloog-isl.la

PLUTO_INCLUDE+=-I$(top_srcdir)/pluto/include/
PLUTO_LIBADD+=-lpluto
PLUTO_LDFLAGS+=-L$(top_builddir)/pluto/lib
endif



libpesto_la_SOURCES = \
   ClastCodegen.cpp \
   ClastNest.cpp \
   ClastOptimizer.cpp \
   ClastProgram.cpp \
   isl_support.c \
   OpenScopOptimizer.cpp \
   osl_support.c \
   PlutoPasses.cpp \
   PolyhedralPass.cpp \
   PolyPasses.cpp \
   PolyProgram.cpp \
   ScopNest.cpp 

libpesto_la_CPPFLAGS = $(OPT_FLAGS) $(DEBUG_FLAGS)\
   -I../include -I$(top_srcdir)/include \
   $(OSL_INCLUDE) \
   $(ISL_INCLUDE) \
   $(CLOOG_INCLUDE) \
   $(PLUTO_INCLUDE)

libpesto_la_LDFLAGS = $(PLUTO_LDFLAGS)

libpesto_la_LIBADD = \
   $(ISL_LIBADD) \
   $(OSL_LIBADD) \
   $(CLOOG_LIBADD) \
   $(PLUTO_LIBADD) \
   -lm -lgomp