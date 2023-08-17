#include<linux/module.h>
#include<linux/platform_device.h>
#include<linux/device.h>
#include "pcd_platform.h"


void pcdev_release(struct device* dev) {

	pr_info("device release");
}

/* platform device data */
struct pcdev_platform_data pcdev_data[4] = 
{
	[0] = {

		.size  = 512,
		.perm = RDWR,
		.serial_number = "PCDEV1ABC"
	},

	[1] = {

		.size  = 512,
		.perm = RDWR,
		.serial_number = "PCDEV2ABC"

	},

	[2] = {

		.size  = 512,
		.perm = RDWR,
		.serial_number = "PCDEV3ABC"

	},

	[3] = {

		.size  = 512,
		.perm = RDWR,
		.serial_number = "PCDEV4ABC"

	}
};

struct  platform_device platform_pcdev_1 = {

	.name = "pseudo-char-device",
	.id = 0,
	.dev = {

			/* platform device specific data */
			.platform_data = &pcdev_data[0],
			.release = pcdev_release
	}
};

struct  platform_device platform_pcdev_2 = {

	.name = "pseudo-char-device",
	.id = 1,
	.dev = {

			/* platform device specific data */
			.platform_data = &pcdev_data[1],
			.release = pcdev_release
	}
};

struct  platform_device platform_pcdev_3 = {

	.name = "pseudo-char-device",
	.id = 2,
	.dev = {

			/* platform device specific data */
			.platform_data = &pcdev_data[2],
			.release = pcdev_release
	}
};

struct  platform_device platform_pcdev_4 = {

	.name = "pseudo-char-device",
	.id = 3,
	.dev = {

			/* platform device specific data */
			.platform_data = &pcdev_data[3],
			.release = pcdev_release
	}
};

static int __init pcd_platform_dev_init(void) {

	pr_info("module inserted");

	/* register the  platform device */

	platform_device_register(&platform_pcdev_1);
	platform_device_register(&platform_pcdev_2);
	platform_device_register(&platform_pcdev_3);
	platform_device_register(&platform_pcdev_4);
	/*
	
	You can also use the below function to add multiple 
	platform devices.
	
	platform_add_device(struct platform_device**,int num)

	*/
	return 0;
}


static void __exit pcd_platform_dev_exit(void) {

	pr_info("module removed");

	/* unregister the  platform device */

	platform_device_unregister(&platform_pcdev_1);
	platform_device_unregister(&platform_pcdev_2);
	platform_device_unregister(&platform_pcdev_3);
	platform_device_unregister(&platform_pcdev_4);

}


module_init(pcd_platform_dev_init);
module_exit(pcd_platform_dev_exit);


MODULE_AUTHOR("Parth Panchal");
MODULE_DESCRIPTION("platform device reg");
MODULE_LICENSE("Dual MIT/GPL");