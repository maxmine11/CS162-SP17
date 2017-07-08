all: main wc map
main: main.o
	cc -o main main.o
main.o: main.c
	cc -c main.c
wc: wc.o
	cc -o wc wc.o
wc.o: wc.c
	cc -c wc.c
map: map.o 
	cc -o map map.o
map.o: map.c       
	 cc -c map.c

clean:
	rm -f *.o core

