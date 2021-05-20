all: System

System: code.o 
	gcc -o program code.o  -lm -lpthread -Wall -Wextra
code.o: code.c
	gcc -c  code.c -Wall -Wextra

run1 : 
	valgrind --leak-check=yes ./program homeworkFile.txt studentsFile.txt 1000
		

clean:
	rm *.o program
