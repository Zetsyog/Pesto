CC=gcc
CFLAGS=-march=native -O3 -fopenmp
LDLIBS=-lm -DPOLYBENCH_TIME

PESTO?=pesto
PESTO_FLAGS?=--indent

PESTO_CONFIG?=$(BASEDIR)/config/ptile.json

PLUTO_FLAGS=--tile --parallel --prevector --nounroll

CLEANFILES=\
	$(SRC) \
	$(SRC)_pluto \
	$(SRC)_pesto \
	$(SRC).pluto.c \
	$(SRC).pesto.c \
	header.*.h \
	trahrhe_header.*.h \
	*.cloog


all: $(SRC)_pesto

$(SRC): $(SRC).c
	$(CC) $(CFLAGS) -o $@ -I $(BASEDIR)/include/ $(BASEDIR)/utilities/polybench.c $< $(LDLIBS) $(EXTRA_FLAGS)

$(SRC).pluto.c: $(SRC).c
	$(POLYCC) $(SRC).c $(PLUTO_FLAGS) -o $@ 

$(SRC).pesto.c: $(SRC).c
	$(PESTO) $(SRC).c --config "$(PESTO_CONFIG)" -o $@ $(PESTO_FLAGS)

$(SRC)_pesto: $(SRC).pesto.c
	$(CC) $(CFLAGS) -o $@ -I $(BASEDIR)/utilities/ $(BASEDIR)/utilities/polybench.c $< $(LDLIBS) $(EXTRA_FLAGS)

$(SRC)_pluto: $(SRC).pluto.c
	$(CC) $(CFLAGS) -o $@ -I $(BASEDIR)/utilities/ $(BASEDIR)/utilities/polybench.c $< $(LDLIBS) $(EXTRA_FLAGS)

check:
	@$(BASEDIR)/tools/pb_chksum $(SRC).c "-DLARGE_DATASET"
	@$(BASEDIR)/tools/pb_chksum $(SRC).pesto.c "-DLARGE_DATASET"

rand:
	$(BASEDIR)/tools/rand_ptiles 5 32 $(SRC)_pesto "-DMEDIUM_DATASET"

clean:
	rm -f $(CLEANFILES) *.scop
