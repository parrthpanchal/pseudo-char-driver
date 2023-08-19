#include<linux/module.h>
#include<linux/platform_device.h>
#include<linux/device.h>
#include<linux/fs.h>
#include<linux/cdev.h>
#include<linux/kdev_t.h>
#include<linux/uaccess.h>
#include<linux/slab.h>
#include<linux/mod_devicetable.h>
#include "pcd_platform.h"


#define TAG "[PCD]"
#define BUFF_SIZE 1024U
#define PCD_MINOR_START 0
#define PCD_MAX_MINORS 10
#define DEV_NAME "pcd"
#define MOD_LOGI(format,...) pr_info(TAG format __VA_OPT__(,) __VA_ARGS__)
#define MOD_LOGE(format,...) pr_err(TAG format __VA_OPT__(,) __VA_ARGS__)

/* Device specific private data */
struct pcdev_prv_data {

	struct pcdev_platform_data pdev;
	dev_t dev_num;
	char* buffer;
	struct cdev cdev;

};

/* Driver specific private data */
struct pcdrv_prv_data {

	int dev_count;
	dev_t dev_num_base;
	struct class* class;
	struct device* device;

};

struct pcdrv_prv_data pcdrv_data;

static int pcd_open(struct inode* inode, struct file* file);
static int pcd_release(struct inode* inode, struct file* file);
static ssize_t pcd_read (struct file * file, char __user * buff, size_t size, loff_t * offset);
static ssize_t pcd_write(struct file * file, const char __user * buff, size_t size, loff_t * offset);
static loff_t pcd_llseek(struct file* file, loff_t off, int whence);
static int pcd_platform_drv_probe(struct platform_device *);
static int pcd_platform_drv_remove(struct platform_device *);

/* File operations for pcd */
struct file_operations pcd_fops = {
	.owner = THIS_MODULE,
	.open  = pcd_open,
	.release = pcd_release,
	.read  = pcd_read,
	.write = pcd_write,
	.llseek = pcd_llseek
};

/* 
	Support multiple versions of pcdev
	TODO : Look at platform_match function in platform.c
	to get more details about how the binding process work

 */
const struct platform_device_id pcdevs_id[] = {

	[0] = {.name = "pcdev-A1x"},
	[1] = {.name = "pcdev-A2x"},
	[2] = {.name = "pcdev-A3x"},
	[3] = {.name = "pcdev-A4x"},
	{ }
};

struct platform_driver pcd_platform_drv = {

	.probe = pcd_platform_drv_probe,
	.remove = pcd_platform_drv_remove,
	.id_table = pcdevs_id,
	.driver = {
			.name = "pseudo-char-device"
	}

};

/* Called when matching platform device is found */
static int pcd_platform_drv_probe(struct platform_device * pdev) {

	int ret;
	struct pcdev_prv_data* pcdev_data;

	/* Get the platform data */
	struct pcdev_platform_data* dev_data = (struct pcdev_platform_data*)pdev->dev.platform_data;

	/* If the platform data is not available return error */
	if(!dev_data) {
		MOD_LOGE("Platform data not available");
		ret = -EINVAL;
		goto out;
	}

	/*
		void* kzalloc(size_t size, gfp_t flags)
		--> Allocate memory in kernel space initialized with zero 
		--> memory obtained by kmalloc is physically contiguos

		You can also use kernel resource managed APIs 
		devm_kzalloz
		Memory allocated using kernel resource manage APIs will be freed automatically by 
		kernel when the device is removed.

	*/

	pcdev_data = kzalloc(sizeof(*pcdev_data),GFP_KERNEL);
	if(!pcdev_data) {
		MOD_LOGE("memory allocation failed");
		ret = -ENOMEM;
		goto out;
	}

	/* save the device private data pointer in platform device structure */
	dev_set_drvdata(&pdev->dev,pcdev_data);

	memcpy(&pcdev_data->pdev,dev_data,sizeof(*dev_data));
	MOD_LOGI("Serial number %s : Size %d : perm : %d",pcdev_data->pdev.serial_number,
													  pcdev_data->pdev.size,
													  pcdev_data->pdev.perm);

	/* 
		Dynamically allocate memory for device buffer using size data 
		available in the platform data
	*/
		pcdev_data->buffer = kzalloc(pcdev_data->pdev.size,GFP_KERNEL);
		if(!pcdev_data->buffer){
			MOD_LOGE("memory allocation failed");
			ret = -ENOMEM;
			goto dev_data_free;
		}

	/*
		Get the device number
	*/
		pcdev_data->dev_num = pcdrv_data.dev_num_base + pdev->id;

	/*
		initialize and add cdev 
	*/
		cdev_init(&pcdev_data->cdev,&pcd_fops);
		pcdev_data->cdev.owner = THIS_MODULE;
		ret = cdev_add(&pcdev_data->cdev,pcdev_data->dev_num,1);
		if(ret < 0) {
			MOD_LOGE("cdev_add failed");
			goto buffer_free;
		}

	/*
		Create device file for the detected device
	*/
		pcdrv_data.device = device_create(pcdrv_data.class,NULL,pcdev_data->dev_num,NULL,DEV_NAME "-%d",pdev->id);
		if(IS_ERR(pcdrv_data.device)) {
			MOD_LOGE("Device creation failed");
			ret = PTR_ERR(pcdrv_data.device);
			goto cdev_del;
		}

	pcdrv_data.dev_count++;
	MOD_LOGI("probing successfull dev_count: %d",pcdrv_data.dev_count);
	return 0;

cdev_del :
	cdev_del(&pcdev_data->cdev);
buffer_free :
	kfree(pcdev_data->buffer);
dev_data_free :
	kfree(pcdev_data);
out :
	MOD_LOGI("Device probing failed");
	return ret;
}

/* Called when platform device is removed from the system */
static int pcd_platform_drv_remove(struct platform_device * pdev) {

	MOD_LOGI("pcd_platform_drv_remove");
	struct pcdev_prv_data* pcdev_data = dev_get_drvdata(&pdev->dev);

	/*
		remote the device from /sys/class/pcd
	*/
	device_destroy(pcdrv_data.class,pcdev_data->dev_num);

	/*
		remove cdev entry from system
	*/
	cdev_del(&pcdev_data->cdev);

	/*
		free the memory held by device
	*/

	kfree(pcdev_data->buffer);
	kfree(pcdev_data);
	pcdrv_data.dev_count++;

	return 0;
}

static int pcd_open(struct inode* inode, struct file* file) {

	MOD_LOGI("open");
	return -EPERM;
}

static int pcd_release(struct inode* inode, struct file* file) {

	MOD_LOGI("release");
	return 0;
}


static ssize_t pcd_read(struct file * file, char __user * buff, size_t size, loff_t * offset) {
	
	
	return 0;
}

static ssize_t pcd_write(struct file * file, const char __user * buff, size_t size, loff_t * offset) {

	return 0;
}

static loff_t pcd_llseek(struct file* file, loff_t off, int whence) {
	

	return 0;
}

static int __init pcd_init(void) {
	
	int ret;

	/* Dynamically allocate device numbers for MAX devices */
	ret = alloc_chrdev_region(&pcdrv_data.dev_num_base,0,PCD_MAX_MINORS,"pcdev");

	if(ret < 0) {
		MOD_LOGE("alloc_chrdev_region failed");
		return ret;
	}

	/* Create device class under /sys/class */
	pcdrv_data.class = class_create(THIS_MODULE,"pcdev");

	if(IS_ERR(pcdrv_data.class)) {
		MOD_LOGE("class_creat failed");
		ret = PTR_ERR(pcdrv_data.class);
		unregister_chrdev_region(pcdrv_data.dev_num_base,PCD_MAX_MINORS);
		return ret;
	}


	/* register the platform driver */
	platform_driver_register(&pcd_platform_drv);

	MOD_LOGI("Module loaded");

	return 0;

}

static void __exit pcd_exit(void) {

	/* unregister the platform driver */
	platform_driver_unregister(&pcd_platform_drv);

	/* remove the class */
	class_destroy(pcdrv_data.class);

	/* deallocate device numbers */
	unregister_chrdev_region(pcdrv_data.dev_num_base,PCD_MAX_MINORS);

	MOD_LOGI("Module unloaded");

}


module_init(pcd_init);
module_exit(pcd_exit);

MODULE_AUTHOR("Parth Panchal");
MODULE_DESCRIPTION("PCD");
MODULE_LICENSE("Dual MIT/GPL");