CC=gcc
CFLAGS=-march=native -fopenmp -O3 
LDLIBS=-lm -DPOLYBENCH_TIME

PESTO=$(BASEDIR)/../../pesto
PESTO_FLAGS=--indent

PLUTO_CONFIG=$(BASEDIR)/config/pluto.json
PESTO_CONFIG=$(BASEDIR)/config/ptile.json

CLEANFILES=$(SRC) $(SRC)_pluto $(SRC)_pesto $(SRC).pluto.c $(SRC).pesto.c header.*.h 


all: $(SRC)_pesto

$(SRC): $(SRC).c
	$(CC) $(CFLAGS) -o $@ -I $(BASEDIR)/include/ $(BASEDIR)/polybench.c $< $(LDLIBS) $(EXTRA_FLAGS)

$(SRC)_pesto: $(SRC).pesto.c
	$(CC) $(CFLAGS) -o $@ -I $(BASEDIR)/include/ $(BASEDIR)/polybench.c $< $(LDLIBS) $(EXTRA_FLAGS)

$(SRC)_pluto: $(SRC).pluto.c
	$(CC) $(CFLAGS) -o $@ -I $(BASEDIR)/include/ $(BASEDIR)/polybench.c $< $(LDLIBS) $(EXTRA_FLAGS)

$(SRC).pluto.c: $(SRC).c
	$(PESTO) $(SRC).c --config "$(PLUTO_CONFIG)" -o $@ $(PESTO_FLAGS)

$(SRC).pesto.c: $(SRC).c
	$(PESTO) $(SRC).c --config "$(PESTO_CONFIG)" -o $@ $(PESTO_FLAGS)

check:
	@$(BASEDIR)/tools/pb_chksum $(SRC).c "-DLARGE_DATASET"
	@$(BASEDIR)/tools/pb_chksum $(SRC).pesto.c "-DLARGE_DATASET"

rand:
	$(BASEDIR)/tools/rand_ptiles 5 32 $(SRC)_scheduled "-DMEDIUM_DATASET"

clean:
	rm -f $(CLEANFILES) *.scop
