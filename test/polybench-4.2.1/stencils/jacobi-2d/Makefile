SRC=jacobi-2d
BASEDIR=../../

include $(BASEDIR)/common.mk

$(SRC).pesto.tpz.c: $(SRC).c
	$(PESTO) $(SRC).c --config "$(PESTO_TPZ_CONFIG)" -o $@ $(PESTO_FLAGS)

${SRC}_tpz: $(SRC).pesto.tpz.c
	$(CC) $(CFLAGS) -o $@ -I $(BASEDIR)/utilities/ $(BASEDIR)/utilities/polybench.c $< $(LDLIBS) $(EXTRA_FLAGS)