SRC=symm
BASEDIR=../../../

include $(BASEDIR)/common.mk

$(SRC).pesto.c: $(SRC).c
	$(PESTO) $(SRC).c --config "$(BASEDIR)/config/ptile.json" -o $@ $(PESTO_FLAGS)
