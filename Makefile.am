AUTOMAKE_OPTIONS = subdir-objects

AM_CXXFLAGS=-Wall -std=c++17

ISL_LA = $(top_builddir)/isl/libisl.la
OSL_LA = $(top_builddir)/osl/libosl.la
CLAN_LA = $(top_builddir)/clan/libclan.la
CLOOG_LA = $(top_builddir)/cloog/libcloog.la


SUBDIRS = pluto lib tool

ACLOCAL_AMFLAGS = -I m4

bin_SCRIPTS = pesto trahrhe_wrapper
CLEANFILES = $(bin_SCRIPTS) 
EXTRA_DIST = pesto.sh.in trahrhe_wrapper.sh.in

trahrhe_wrapper: trahrhe_wrapper.sh
	cat trahrhe_wrapper.sh > trahrhe_wrapper
	chmod ugo+x trahrhe_wrapper

pesto: pesto.sh
	cat pesto.sh > pesto
	chmod ugo+x pesto
