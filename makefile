all:
	gcc main.c --std=gnu99 -O3 -o smallsh

run:
	gcc main.c --std=gnu99 -O3 -o smallsh && smallsh

valgrind:
	gcc main.c --std=gnu99 -O3 -o smallsh && valgrind --leak-check=full -s smallsh

clean:
	rm smallsh