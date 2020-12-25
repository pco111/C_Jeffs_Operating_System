test: a3_test.o sma.o
	gcc -g -o test a3_test.o sma.o

a3_test.o: a3_test.c
	gcc -g -c a3_test.c

sma.o: sma.c
	gcc -g -c sma.c

clean: 
	rm test sma.o a3_test.o