#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <libusb.h>
#include <linux/hid.h>

#define DEV_VID		0x1043
#define DEV_PID		0x3100
#define HID_IF		0x0
#define HID_AS		0x0
#define HID_WR_EP	0x02
#define HID_RD_EP	0x81

typedef struct {
	uint32_t	dCBWSignature;
	uint32_t	dCBWTag;
	uint32_t	dCBWDataTransferLength;
	uint8_t		bmCBWFlags;
	uint8_t		bCBWLUN;
	uint8_t		bCBWCBLength;
} __attribute__ ((packed)) cbw_t;

typedef struct {
	uint16_t	u1;
	uint32_t	blockSize;
	uint32_t	totalSize;
	uint8_t		index; //?
	uint8_t		u2;
	uint8_t		unused[4];
} __attribute__ ((packed)) cb_header_t;

typedef struct {
	uint16_t	u1;
	uint32_t	blockSize;
	uint32_t	totalSize;
	uint8_t		index; //?
	uint8_t		u2;
	uint8_t		unused[485];
} __attribute__ ((packed)) cb_footer_t;

typedef struct {
	uint8_t		u1;
	uint8_t		u2;
	uint16_t	u3;
	uint32_t	length;
	uint16_t	x, y, w, h;
	uint16_t	u4;
	uint16_t	u5;
	uint16_t	u6;
	uint16_t	u7;
	uint16_t	u8;
	uint16_t	u9;
	uint16_t	u10;
	uint16_t	u11;
} __attribute__ ((packed)) image_t;

int hw_init(libusb_device_handle *device) {
	if (libusb_reset_device(device) != 0) {
		libusb_release_interface(device, HID_IF);
		printf("Failed to reset the device\n");
		return 0;
	}

	if (libusb_kernel_driver_active(device, HID_IF) == 1 && libusb_detach_kernel_driver(device, HID_IF) != 0) {
		printf("Failed to activate and detatch\n");
		return 0;
	}

	if (libusb_set_configuration(device, 1) != 0) {
		libusb_release_interface(device, HID_IF);
		printf("Failed to set configuration\n");
		return 0;
	}

	if (libusb_claim_interface(device, HID_IF) != 0) {
		printf("Failed to claim interface\n");
		return 0;
	}

	if (libusb_set_interface_alt_setting(device, HID_IF, HID_AS) != 0) {
		libusb_release_interface(device, HID_IF);
		printf("Failed to set alt setting\n");
		return 0;
	}


	if (libusb_clear_halt(device, HID_WR_EP) != 0) {
		printf("Failed to clear WR halt");
		return 0;
	}


	if (libusb_clear_halt(device, HID_RD_EP) != 0) {
		libusb_release_interface(device, HID_IF);
		printf("Failed to clear RD halt\n");
		return 0;
	}

	return 1;
}

void hw_write(struct libusb_transfer *xfer) {
	/* free the data */
	free(xfer->buffer);
	libusb_free_transfer(xfer);
}

int dev_read(libusb_device_handle *device, uint8_t *data, unsigned int length) {
	int read = 0;
	if (libusb_bulk_transfer(device, HID_RD_EP, data, length, &read, 2000) != 0) {
		printf("error reading\n");
		return read;
	}
	return read;
}

void dump(uint8_t *data, int len) {
	printf("Dump: %0x\n", len);
	int i;
	for(i = 0; i < len; ++i) {
		printf("%02x ", data[i]);
		if ((i+1) % 16 == 0) printf("\n");
	}
	printf("\n");
}

uint32_t flip32(uint32_t v) {
	return ((v & 0xff000000) >> 24) | ((v & 0xff0000) >> 8) | ((v & 0xff00) << 8) | ((v & 0xff) << 24);
}


int dev_write(libusb_device_handle *device, uint8_t *data, unsigned int length) {
	int r, wrote;
	int pos = 0;
	uint8_t tmp[0xff];

	cbw_t		*cbw;
	cb_header_t	*header;
	cb_footer_t	*footer;

	uint8_t h[sizeof(cbw_t) + sizeof(cb_header_t)];
	uint8_t f[sizeof(cbw_t) + sizeof(cb_footer_t)];
	cbw	= (cbw_t      *)&h[0];
	header	= (cb_header_t*)&h[sizeof(cbw_t)];
	footer  = (cb_footer_t*)&f[sizeof(cbw_t)];
	
	cbw->dCBWSignature		= 0x43425355;
	cbw->dCBWTag			= 0x843d84a0;
	cbw->bmCBWFlags			= 0x00;
	cbw->bCBWLUN			= 0x00;
	cbw->bCBWCBLength		= 0x0c;
	header->u1			= 0x02e6;
	header->totalSize		= flip32(length);
	header->index			= 0;

	footer->u1			= 0x02e6;
	footer->totalSize		= header->totalSize;
	memset(header->unused, 0, sizeof(header->unused));
	memset(footer->unused, 0, sizeof(footer->unused));

	int block = 0;
	while(length > 0) {
		int copy = length > 0x10000 ? 0x10000 : length;

		cbw->dCBWDataTransferLength = copy;
		header->blockSize	= flip32(copy);
		header->index		= block;
		header->u2		= 0x0;

		libusb_bulk_transfer(device, HID_WR_EP, h, sizeof(h), &wrote, 100);
		if (wrote != sizeof(h)) {
			printf("Error writing CBW header\n");
			exit(-1);
		}

		r = libusb_bulk_transfer(device, HID_WR_EP, data, copy, &wrote, 0);
		data	+= wrote;
		pos	+= wrote;
		length	-= wrote;
		dev_read(device, tmp, 0xd);

		/* the footer cbw is the same as the header */
		memcpy(f, cbw, sizeof(cbw_t));
		footer->blockSize	= flip32(copy);
		footer->index		= block;
		footer->u2		= 0x0;

		libusb_bulk_transfer(device, HID_WR_EP, f, sizeof(f), &wrote, 100);
		if (wrote != sizeof(f)) {
			printf("Error writing CBW footer %d\n", wrote);
			exit(-1);
		}

		++block;
	}
	return pos;
}

int main(int argc, char *argv[]) {
	int			r;
	libusb_context		*ctx;
	libusb_device_handle	*device;

	if ((r = libusb_init(&ctx)) < 0)
		return r;

	device = libusb_open_device_with_vid_pid(ctx, DEV_VID, DEV_PID);
	if (!device) {
		printf("Failed to open the device\n");
		return -1;
	}

	if (!hw_init(device)) {
		libusb_close(device);
		libusb_exit(ctx);
		return -1;
	}

	uint8_t	image[sizeof(image_t) + (320 * 240 * 3)];
	memset(image, 0, sizeof(image));

	image_t *header = (image_t*)image;
	uint8_t	*data	= image + sizeof(image_t);

	header->u1	= 0x02;
	header->u2	= 0xf0;
	header->u3	= 0x20; /* bits per pixel? */
	header->x	= 0;
	header->y	= 0;
	header->w	= 320;
	header->h	= 240;
	header->length	= sizeof(image_t) + (header->w * header->h * 3);
	header->u5	= 0x01; /* no idea */

	memset(data, 0x00, sizeof(image) - sizeof(image_t));

	while(1) {
		for(r = 0; r < header->w * header->h * 3; r += 3) {
			data[r+0] = rand() % 255;
			data[r+1] = rand() % 255;
			data[r+2] = rand() % 255;
		}
		dev_write(device, image, sizeof(image));
	}

	libusb_attach_kernel_driver(device, HID_IF);
	libusb_close(device);
	libusb_exit(ctx);
	return 0;
}
