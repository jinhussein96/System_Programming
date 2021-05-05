all: System

System: 151044079.o 
	gcc -o program 151044079.o  -lm -pthread -Wall -Wextra -lrt
151044079.o: 151044079.c
	gcc -c  151044079.c -Wall -Wextra

run1 : 
	valgrind --leak-check=yes ./program 



clean:
	rm *.o program