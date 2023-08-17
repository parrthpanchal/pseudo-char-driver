#ifndef __PCD_PLATFORM_H__
#define __PCD_PLATFORM_H__


#define RDONLY 	0x01
#define RDWR	0x11
#define RWONLY	0x10

struct pcdev_platform_data {

	int size;
	int perm;
	const char* serial_number;
};

#endif