#include<linux/module.h>
#include<linux/fs.h>
#include<linux/cdev.h>
#include<linux/kdev_t.h>
#include<linux/uaccess.h>

#define TAG "[PCD]"
#define BUFF_SIZE 1024U
#define PCD_MINOR_START 0
#define PCD_MAX_MINORS 1
#define DEV_NAME "pcd"
#define MOD_LOGI(format,...) pr_info(TAG format __VA_OPT__(,) __VA_ARGS__)
#define MOD_LOGE(format,...) pr_err(TAG format __VA_OPT__(,) __VA_ARGS__)

/* device memory */
uint8_t pcd_buff[BUFF_SIZE];
/* Holds the device number */
dev_t device_number;
struct cdev pcd_dev;
struct class* class_pcd;
struct device* device_pcd;

static int pcd_open(struct inode* inode, struct file* file);
static int pcd_release(struct inode* inode, struct file* file);
static ssize_t pcd_read (struct file * file, char __user * buff, size_t size, loff_t * offset);
static ssize_t pcd_write(struct file * file, const char __user * buff, size_t size, loff_t * offset);
static loff_t pcd_llseek(struct file* file, loff_t off, int whence);


/* File operations for pcd */
struct file_operations pcd_fops = {
	.owner = THIS_MODULE,
	.open  = pcd_open,
	.release = pcd_release,
	.read  = pcd_read,
	.write = pcd_write,
	.llseek = pcd_llseek
};


static int pcd_open(struct inode* inode, struct file* file) {

	MOD_LOGI("open");
	return 0;
}

static int pcd_release(struct inode* inode, struct file* file) {

	MOD_LOGI("release");
	return 0;
}


static ssize_t pcd_read(struct file * file, char __user * buff, size_t size, loff_t * offset) {
	
	MOD_LOGI("read req %zu bytes",size);
	MOD_LOGI("Current file position %lld\n",*offset);
	
	/* Adjust the amount of data to be read */
	if(size > BUFF_SIZE - *offset) size = BUFF_SIZE - *offset;
	
	/* Check if offset has reached end of file */
	if(size <= 0) return 0;
	
	/* Copy data to user space buffer */
	if(copy_to_user(buff,pcd_buff + *offset,size)) {
		return -EFAULT;
	}
	
	/* Update the offset pointer */
	*offset += size;
	
	MOD_LOGI("# of bytes successfully read %d",size);
	MOD_LOGI("Updated file position %lld\n",*offset);
	
	/* return number of bytes successfully read */
	return size;
}

static ssize_t pcd_write(struct file * file, const char __user * buff, size_t size, loff_t * offset) {

	MOD_LOGI("write req %zu bytes",size);
	MOD_LOGI("Current file position %lld\n",*offset);
	
	/* Adjust the amount of data to be written */
	if(size > BUFF_SIZE - *offset) size = BUFF_SIZE - *offset;
	
	/* if pcd_buff is full no more data can be written */
	if(size <= 0) {
		MOD_LOGI("No more memory to write data\n",);
		return -ENOMEM;
	}
	/* Copy data from user space buffer to kernel using kernel data copy utility (copy_from_user()) */
	if(copy_from_user(pcd_buff + *offset,buff,size)) {
		return -EFAULT;
	}
	
	/* Update the offset pointer */
	*offset += size;
	
	MOD_LOGI("# of bytes successfully written %d",size);
	MOD_LOGI("Updated file position %lld\n",*offset);
	
	/* return number of bytes successfully written */
	return size;
}

static loff_t pcd_llseek(struct file* file, loff_t off, int whence) {
	
	MOD_LOGI("lseek requested");
	MOD_LOGI("Current file position %lld\n",file->f_pos);
	
	switch(whence) {
	
		case SEEK_SET:
			if((off > BUFF_SIZE) || (off < 0))
				return -EINVAL;
			file->f_pos = off;
		break;
		case SEEK_END:
			if(((BUFF_SIZE + off) > BUFF_SIZE) || (off < 0))
				return -EINVAL;
			file->f_pos = BUFF_SIZE;
		break;
		case SEEK_CUR:
			if(((file->f_pos + off) > BUFF_SIZE) || (off < 0))
				return -EINVAL;
			file->f_pos += off;
		break;
		default:
			return -EINVAL;
		break;
		
	}
	
	MOD_LOGI("Updated file position %lld\n",file->f_pos);
	return file->f_pos;
}

static int __init pcd_init(void) {
	
	int ret;
	/* Allocate device number dynamically */
	ret = alloc_chrdev_region(&device_number,PCD_MINOR_START,PCD_MAX_MINORS,DEV_NAME);
	if(ret < 0) {
		MOD_LOGE("alloc_chrdev_region failed");
		goto out;
	}
	
	MOD_LOGI("Major : %d | Minor : %d\n",MAJOR(device_number),MINOR(device_number));
	 
	/* Initialize cdev structure */
	cdev_init(&pcd_dev,&pcd_fops);
	pcd_dev.owner = THIS_MODULE;
		
	/* Add char dev to kernel VFS */
	ret = cdev_add(&pcd_dev,device_number,PCD_MAX_MINORS);
	if(ret < 0) {
		MOD_LOGE("cdev_add failed");
		goto unreg_chr_dev;
	}
	
	/* Create device class under /sys/class */
	class_pcd = class_create(THIS_MODULE,"pcd_class");
	
	/* 
	   if class creation fails class_create method returns ERROR POINTER which 
	   can be converted to error using PTR_ERR() macro
	   
	   linux/err.h
	   IS_ERR() --> Checks whether pointer returned by method is an error pointer
	   PTR_ERR() --> Converts error pointer to error 
	   ERR_PTR() --> Converts error to pointer type
	   
	*/
	if(IS_ERR(class_pcd)) {
		MOD_LOGE("Class creation failed");
		ret = PTR_ERR(class_pcd);
		goto cdev_delete;
	}
	 
	/* create device file under /sys/class/pcd_class */
	device_pcd = device_create(class_pcd,NULL,device_number,NULL,DEV_NAME);
	if(IS_ERR(device_pcd)) {
		MOD_LOGE("Device creation failed");
		ret = PTR_ERR(device_pcd);
		goto delete_class;
	}
	
	return 0;

delete_class :
	class_destroy(class_pcd);
cdev_delete :
	cdev_del(&pcd_dev);
unreg_chr_dev:
	unregister_chrdev_region(device_number,PCD_MAX_MINORS);
out:
	MOD_LOGE("Module insertion failed");
	return ret;

}

static void __exit pcd_exit(void) {

	/* remove the device and device class from sysfs */
	device_destroy(class_pcd,device_number);
	class_destroy(class_pcd);

	cdev_del(&pcd_dev);
	unregister_chrdev_region(device_number,PCD_MAX_MINORS);
	MOD_LOGI("module exit");
}


module_init(pcd_init);
module_exit(pcd_exit);

MODULE_AUTHOR("Parth Panchal");
MODULE_DESCRIPTION("PCD");
MODULE_LICENSE("Dual MIT/GPL");
