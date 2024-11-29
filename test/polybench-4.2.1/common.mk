CC=gcc
CFLAGS=-march=native -O3 -fopenmp
LDLIBS=-lm -DPOLYBENCH_TIME

PESTO?=pesto
PESTO_FLAGS?=--indent

PESTO_CONFIG?=$(BASEDIR)/config/ptile.json

PLUTO_FLAGS=--tile --parallel --prevector --nounroll

GEN_METRICS_BIN=$(BASEDIR)/../tools/gen_metrics_per_thread.sh

CLEANFILES=\
	$(SRC) \
	$(SRC)_pluto \
	$(SRC)_pluto_metrics \
	$(SRC)_pesto \
	$(SRC)_pesto_metrics \
	$(SRC).pluto.c \
	$(SRC).pluto.metrics.c \
	$(SRC).pesto.c \
	$(SRC).pesto.metrics.c \
	header.*.h \
	trahrhe_header.*.h


all: $(SRC)_pesto

$(SRC): $(SRC).c
	$(CC) $(CFLAGS) -o $@ -I $(BASEDIR)/include/ $(BASEDIR)/polybench.c $< $(LDLIBS) $(EXTRA_FLAGS)


$(SRC).pluto.c: $(SRC).c
	$(POLYCC) $(SRC).c $(PLUTO_FLAGS) -o $@ 

$(SRC).pesto.c: $(SRC).c
	$(PESTO) $(SRC).c --config "$(PESTO_CONFIG)" -o $@ $(PESTO_FLAGS)

$(SRC).pluto.metrics.c: $(SRC).pluto.c
	@$(GEN_METRICS_BIN) $< $@

$(SRC).pesto.metrics.c: $(SRC).pesto.c
	@$(GEN_METRICS_BIN) $< $@

$(SRC)_pesto: $(SRC).pesto.c
	$(CC) $(CFLAGS) -o $@ -I $(BASEDIR)/include/ $(BASEDIR)/polybench.c $< $(LDLIBS) $(EXTRA_FLAGS)

$(SRC)_pluto: $(SRC).pluto.c
	$(CC) $(CFLAGS) -o $@ -I $(BASEDIR)/include/ $(BASEDIR)/polybench.c $< $(LDLIBS) $(EXTRA_FLAGS)

$(SRC)_pesto_metrics: $(SRC).pesto.metrics.c
	$(CC) $(CFLAGS) -o $@ -I $(BASEDIR)/include/ $(BASEDIR)/polybench.c -I $(BASEDIR)/../include/ $< $(LDLIBS) $(EXTRA_FLAGS)
	
$(SRC)_pluto_metrics: $(SRC).pluto.metrics.c
	$(CC) $(CFLAGS) -o $@ -I $(BASEDIR)/include/ $(BASEDIR)/polybench.c -I $(BASEDIR)/../include/ $< $(LDLIBS) $(EXTRA_FLAGS)

check:
	@$(BASEDIR)/tools/pb_chksum $(SRC).c "-DLARGE_DATASET"
	@$(BASEDIR)/tools/pb_chksum $(SRC).pesto.c "-DLARGE_DATASET"

rand:
	$(BASEDIR)/tools/rand_ptiles 5 32 $(SRC)_scheduled "-DMEDIUM_DATASET"

clean:
	rm -f $(CLEANFILES) *.scop
