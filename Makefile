all:
	gcc -O0 -g main.c -o duo \
                `pkg-config libusb-1.0 --cflags --libs`
