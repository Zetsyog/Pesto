CC?=gcc
CFLAGS?=-march=native -O3 -fopenmp
LDLIBS=-lm -DPOLYBENCH_TIME

PESTO?=pesto
PESTO_FLAGS?=--indent

PESTO_CONFIG?=$(BASEDIR)/config/ptile.json
PESTO_TPZ_CONFIG=$(BASEDIR)/config/tpz.json

PLUTO_CONFIG?=$(BASEDIR)/config/pluto.json

CLEANFILES=\
	$(SRC) \
	$(SRC)_pluto \
	$(SRC)_pesto \
	$(SRC).pluto.c \
	$(SRC).pesto.c \
	header.*.h \
	trahrhe_header.*.h \
	$(SRC).trahrhe.* \
	*.cloog


all: $(SRC)_pesto

$(SRC): $(SRC).c
	$(CC) $(CFLAGS) -o $@ -I $(BASEDIR)/utilities/ $(BASEDIR)/utilities/polybench.c $< $(LDLIBS) $(EXTRA_FLAGS)

$(SRC).pluto.c: $(SRC).c
	$(PESTO) $(SRC).c --config "$(PLUTO_CONFIG)" -o $@ $(PESTO_FLAGS)

$(SRC).pesto.c: $(SRC).c
	$(PESTO) $(SRC).c --config "$(PESTO_CONFIG)" -o $@ $(PESTO_FLAGS)

$(SRC)_pesto: $(SRC).pesto.c
	$(CC) $(CFLAGS) -o $@ -I $(BASEDIR)/utilities/ $(BASEDIR)/utilities/polybench.c $< $(LDLIBS) $(EXTRA_FLAGS)

$(SRC)_pluto: $(SRC).pluto.c
	$(CC) $(CFLAGS) -o $@ -I $(BASEDIR)/utilities/ $(BASEDIR)/utilities/polybench.c $< $(LDLIBS) $(EXTRA_FLAGS)

check: $(SRC).pesto.c
	@$(BASEDIR)/tools/pb_chksum $(SRC).c "-DLARGE_DATASET"
	@$(BASEDIR)/tools/pb_chksum $(SRC).pesto.c "-DLARGE_DATASET"

rand:
	@$(BASEDIR)/tools/rand_ptiles 5 32 $(SRC)_pesto "-MEDIUM_DATASET"

clean:
	rm -f $(CLEANFILES) *.scop
