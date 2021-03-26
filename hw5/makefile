all: System

System: 151044079.o 
	gcc -o exe 151044079.o  -lm -lpthread -Wall -Wextra
151044079.o: 151044079.c
	gcc -c  151044079.c -Wall -Wextra

run1 : 
	valgrind --leak-check=yes ./exe -i data.dat



clean:
	rm *.o exe