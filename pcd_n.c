#include<linux/module.h>
#include<linux/fs.h>
#include<linux/cdev.h>
#include<linux/kdev_t.h>
#include<linux/uaccess.h>

#define TAG "[PCD]"
#define PCD1_BUFF_SIZE 1024U
#define PCD2_BUFF_SIZE 512U
#define PCD3_BUFF_SIZE 1024U
#define PCD4_BUFF_SIZE 512U
#define PCD_MINOR_START 0
#define PCD_MAX_MINORS 4
#define DEV_NAME "pcd"
#define MOD_LOGI(format,...) pr_info(TAG format __VA_OPT__(,) __VA_ARGS__)
#define MOD_LOGE(format,...) pr_err(TAG format __VA_OPT__(,) __VA_ARGS__)
#define RDWR	0x11
#define WRONLY	0x10
#define RDONLY	0x01

/* device memory */
uint8_t pcd1_buff[PCD1_BUFF_SIZE];
uint8_t pcd2_buff[PCD2_BUFF_SIZE];
uint8_t pcd3_buff[PCD3_BUFF_SIZE];
uint8_t pcd4_buff[PCD4_BUFF_SIZE];

/* Device private data */
struct pcdev_private_data {

	char* buffer;
	unsigned size;
	const char* serial_number;
	int perm;
	struct cdev cdev;

};


/* Driver private data */
struct pcdrv_private_data {

	int total_dev;
	dev_t dev_num;
	struct class* class_pcd;
	struct device* device_pcd;
	struct pcdev_private_data pcdev_data[PCD_MAX_MINORS];
	
};

struct pcdrv_private_data pcdrv_data = 
{	
	.total_dev = PCD_MAX_MINORS,
	.pcdev_data = 
	{
		[0] = 
		{
			.buffer = pcd1_buff,
			.size  = PCD1_BUFF_SIZE,
			.serial_number = "PCDEV1",
			.perm = RDONLY
				
		},
		
		[1] = 
		{
			.buffer = pcd2_buff,
			.size  = PCD2_BUFF_SIZE,
			.serial_number = "PCDEV2",
			.perm = RDWR
				
		},
		
		[2] = 
		{
			.buffer = pcd3_buff,
			.size  = PCD3_BUFF_SIZE,
			.serial_number = "PCDEV3",
			.perm = WRONLY
				
		},
		
		[3] = 
		{
			.buffer = pcd4_buff,
			.size  = PCD4_BUFF_SIZE,
			.serial_number = "PCDEV4",  
			.perm = RDWR
				
		}
	}

};

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

int check_permission(int dev_perm, int acc_mode){

	if(dev_perm == RDWR) {
		return 0;
	}
	/* Check read only access */
	if(dev_perm == RDONLY && (acc_mode & FMODE_READ) && !(acc_mode & FMODE_WRITE)) {
		return 0;
	}
	/* Check write only access */
	if(dev_perm == WRONLY && (acc_mode & FMODE_WRITE) && !(acc_mode & FMODE_READ)) {
		return 0;
	}
	
	/* Check read write access */
	if(dev_perm == RDWR && (acc_mode & FMODE_WRITE) && (acc_mode & FMODE_READ)) {
		return 0;
	}
	
	return -EPERM;
}
static int pcd_open(struct inode* inode, struct file* file) {

	MOD_LOGI("open");
	int ret;
	/* get information about the minor that was opened */
	int minor = MINOR(inode->i_rdev);
	MOD_LOGI("minor accessed %d",minor);
	/* 
		container_of macro returns pointer to the structure containing member 
		container_of(ptr,type,member)
	*/
	struct pcdev_private_data* pcdev_data = container_of(inode->i_cdev,struct pcdev_private_data,cdev);
	
	/* Supply device private data to other methods of driver */
	file->private_data = pcdev_data;
	
	/* check permissions */
	ret = check_permission(pcdev_data->perm,file->f_mode);
	
	if(!ret)MOD_LOGI("open was successfull");
	else MOD_LOGE("open was unsuccessfull");
	
	return ret;
}

static int pcd_release(struct inode* inode, struct file* file) {

	MOD_LOGI("release");
	return 0;
}


static ssize_t pcd_read(struct file * file, char __user * buff, size_t size, loff_t * offset) {


	MOD_LOGI("read req %zu bytes",size);
	MOD_LOGI("Current file position %lld\n",*offset);
	
	/* get device's private data */
	struct pcdev_private_data* pcdev_data = (struct pcdev_private_data*)file->private_data;
	
	/* Adjust the amount of data to be read */
	if(size > pcdev_data->size - *offset) size = pcdev_data->size - *offset;
	
	/* Check if offset has reached end of file */
	if(size <= 0) return 0;
	
	/* Copy data to user space buffer */
	if(copy_to_user(buff,pcdev_data->buffer + *offset,size)) {
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
	
	/* get device's private data */
	struct pcdev_private_data* pcdev_data = (struct pcdev_private_data*)file->private_data;
	
	/* Adjust the amount of data to be read */
	if(size > pcdev_data->size - *offset) size = pcdev_data->size - *offset;
	
	/* if pcd_buff is full no more data can be written */
	if(size <= 0) {
		MOD_LOGI("No more memory to write data\n",);
		return -ENOMEM;
	}
	/* Copy data from user space buffer to kernel using kernel data copy utility (copy_from_user()) */
	if(copy_from_user(pcdev_data->buffer + *offset,buff,size)) {
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
	
	struct pcdev_private_data* pdev_data = file->private_data;
	int size  = pdev_data->size;
	
	switch(whence) {
	
		case SEEK_SET:
			if((off > size) || (off < 0))
				return -EINVAL;
			file->f_pos = off;
		break;
		case SEEK_END:
			if(((size + off) > size) || (off < 0))
				return -EINVAL;
			file->f_pos = size;
		break;
		case SEEK_CUR:
			if(((file->f_pos + off) > size) || (off < 0))
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
	
	int ret,i;
	/* Allocate device number dynamically */
	ret = alloc_chrdev_region(&pcdrv_data.dev_num,PCD_MINOR_START,PCD_MAX_MINORS,DEV_NAME);
	if(ret < 0) {
		MOD_LOGE("alloc_chrdev_region failed");
		goto out;
	}
	
	pcdrv_data.class_pcd = class_create(THIS_MODULE,"pcd_class");
	
	if(IS_ERR(pcdrv_data.class_pcd)) {
		MOD_LOGE("Class creation failed");
		ret = PTR_ERR(pcdrv_data.class_pcd);
		goto unreg_chr_dev;
	}
	
	for(i=0;i<PCD_MAX_MINORS;i++) {
	
		MOD_LOGI("Major : %d | Minor : %d\n",MAJOR(pcdrv_data.dev_num + i),MINOR(pcdrv_data.dev_num + i));
	 
		/* Initialize cdev structure */
		cdev_init(&pcdrv_data.pcdev_data[i].cdev,&pcd_fops);
		pcdrv_data.pcdev_data[i].cdev.owner = THIS_MODULE;
			
		/* Add char dev to kernel VFS */
		ret = cdev_add(&pcdrv_data.pcdev_data[i].cdev,pcdrv_data.dev_num + i,1);
		if(ret < 0) {
			MOD_LOGE("cdev_add failed");
			goto unreg_chr_dev;
		}
		
		/* create device file under /sys/class/pcd_class */
		pcdrv_data.device_pcd = device_create(pcdrv_data.class_pcd,NULL,pcdrv_data.dev_num + i,NULL,DEV_NAME "-%d",i);
		if(IS_ERR(pcdrv_data.device_pcd)) {
			MOD_LOGE("Device creation failed");
			ret = PTR_ERR(pcdrv_data.device_pcd);
			goto delete_class;
		}
	}
	
	MOD_LOGI("module init successfull");
	return 0;


cdev_delete :
	for(;i>=0;i--)
	{
		device_destroy(pcdrv_data.class_pcd,pcdrv_data.dev_num+i);
		cdev_del(&pcdrv_data.pcdev_data[i].cdev);

	}
delete_class :
	class_destroy(pcdrv_data.class_pcd);
unreg_chr_dev:
	unregister_chrdev_region(pcdrv_data.dev_num,PCD_MAX_MINORS);
out:
	MOD_LOGE("Module insertion failed");
	return ret;

}

static void __exit pcd_exit(void) {

	/* remove the device and device class from sysfs */
	for(int i=0;i<PCD_MAX_MINORS;i++)
	{
		device_destroy(pcdrv_data.class_pcd,pcdrv_data.dev_num+i);
		cdev_del(&pcdrv_data.pcdev_data[i].cdev);

	}
	class_destroy(pcdrv_data.class_pcd);

	unregister_chrdev_region(pcdrv_data.dev_num,PCD_MAX_MINORS);
	MOD_LOGI("module exit");
}


module_init(pcd_init);
module_exit(pcd_exit);

MODULE_AUTHOR("Parth Panchal");
MODULE_DESCRIPTION("Pseudo character driver which handles n nodes");
MODULE_LICENSE("Dual MIT/GPL");
