CC?=gcc
CFLAGS?=-march=native -O3 -fopenmp
LDLIBS=-lm -DPOLYBENCH_TIME

PESTO?=pesto
PESTO_FLAGS?=--indent

PESTO_TPZ_CONFIG?=$(BASEDIR)/config/tpz.json
PESTO_ALGEBRAIC_CONFIG?=$(BASEDIR)/config/atiling.json
PESTO_HYBRID_CONFIG?=$(BASEDIR)/config/hybrid.json

PLUTO_CONFIG?=$(BASEDIR)/config/pluto.json

CLEANFILES=\
	original \
	atiled \
	hybrid \
	trapez \
	$(SRC).pluto.c \
	$(SRC).atiled.c \
	$(SRC)_atiled \
	$(SRC).hybrid.c \
	$(SRC)_hybrid \
	$(SRC).pesto.tpz.c \
	$(SRC)_tpz \
	header.*.h \
	trahrhe_header.*.h \
	$(SRC).trahrhe.* \
	$(SRC).pesto.trahrhe.* \
	$(SRC).pesto.tpz.trahrhe.* \
	$(SRC).hybrid.trahrhe.* \
	$(SRC).atiled.trahrhe.* \
	*.cloog


all: $(SRC)_pesto

original: $(SRC).c
	$(CC) $(CFLAGS) -o $@ -I $(BASEDIR)/utilities/ $(BASEDIR)/utilities/polybench.c $< $(LDLIBS) $(EXTRA_FLAGS)

$(SRC).pluto.c: $(SRC).c
	$(PESTO) $(SRC).c --config "$(PLUTO_CONFIG)" -o $@ $(PESTO_FLAGS)

pluto: $(SRC).pluto.c
	$(CC) $(CFLAGS) -o $@ -I $(BASEDIR)/utilities/ $(BASEDIR)/utilities/polybench.c $< $(LDLIBS) $(EXTRA_FLAGS)

$(SRC).atiled.c: $(SRC).c
	$(PESTO) $(SRC).c --config "$(PESTO_ALGEBRAIC_CONFIG)" -o $@ $(PESTO_FLAGS)

atiled: $(SRC).atiled.c
	$(CC) $(CFLAGS) -o $@ -I $(BASEDIR)/utilities/ $(BASEDIR)/utilities/polybench.c $< $(LDLIBS) $(EXTRA_FLAGS)

$(SRC).hybrid.c: $(SRC).c
	$(PESTO) $(SRC).c --config "$(PESTO_HYBRID_CONFIG)" -o $@ $(PESTO_FLAGS)

hybrid: $(SRC).hybrid.c
	$(CC) $(CFLAGS) -o $@ -I $(BASEDIR)/utilities/ $(BASEDIR)/utilities/polybench.c $< $(LDLIBS) $(EXTRA_FLAGS)

check: $(SRC).atiled.c
	@$(BASEDIR)/tools/pb_chksum $(SRC).c "-DLARGE_DATASET"
	@$(BASEDIR)/tools/pb_chksum $(SRC).atiled.c "-DLARGE_DATASET"

check-atiled-rand:
	@$(BASEDIR)/tools/rand_ptiles 5 32 atiled "-DMEDIUM_DATASET"

clean:
	rm -f $(CLEANFILES) *.scop
