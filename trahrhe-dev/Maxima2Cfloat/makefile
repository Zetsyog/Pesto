prefixe=expression
bin=Maxima2Cfloat

all: $(bin)

$(bin): y.tab.c lex.yy.c
	gcc y.tab.c lex.yy.c -ly -lfl -lm -o $@

y.tab.c: $(prefixe).y
	yacc -d $(prefixe).y

lex.yy.c: $(prefixe).l
	lex $(prefixe).l

clean:
	rm -f *.o y.tab.c y.tab.h lex.yy.c $(bin)
