all:
	gcc -O0 -g main.c -o duo -I. -L. -lusb-1.0
