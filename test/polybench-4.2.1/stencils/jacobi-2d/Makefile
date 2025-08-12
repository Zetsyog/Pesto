SRC=jacobi-2d
BASEDIR=../../

include $(BASEDIR)/common.mk

$(SRC).pesto.tpz.c: $(SRC).c
	$(PESTO) $(SRC).c --config "$(PESTO_TPZ_CONFIG)" -o $@ $(PESTO_FLAGS)