DEBUG=#-g -ggdb -DDEBUG
OPTS=-O2
CFLAGS=${OPTS} ${DEBUG} -Wall -std=gnu99
LIBS=-lpthread

extsort: extsort.o qsort.o
	gcc -o extsort extsort.o qsort.o ${CFLAGS} ${LIBS}
extsort.o: extsort.c extsort.h
	gcc -c -o extsort.o extsort.c ${CFLAGS} ${LIBS}
qsort.o: qsort.c qsort.h
	gcc -c -o qsort.o qsort.c ${CFLAGS} ${LIBS}
clean:
	rm -f *.o
	rm -f extsort
	rm bar*.dat
	rm foo*.dat
