#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/types.h>
#include <linux/cdev.h>
#include <linux/kdev_t.h>
#include <linux/uaccess.h>
#include <linux/errno.h>
#include <linux/device.h>
#include <linux/wait.h>
#include <linux/semaphore.h>

#define BUFF_SIZE 100

MODULE_LICENSE("Dual BSD/GPL");

dev_t my_dev_id;
static struct class *my_class;
static struct device *my_device;
static struct cdev *my_cdev;

DECLARE_WAIT_QUEUE_HEAD(readQ);
DECLARE_WAIT_QUEUE_HEAD(writeQ);


struct semaphore sem;
char stred[BUFF_SIZE];
int pos = 0;
int end_read = 0;

int stred_open(struct inode *pinode, struct file *pfile);
int stred_close(struct inode *pinode, struct file *pfile);
ssize_t stred_read(struct file *pfile, char __user *buffer, size_t length, loff_t *offset);
ssize_t stred_write(struct file *pfile, const char __user *buffer, size_t length, loff_t *offset);

struct file_operations my_fops =
{
	.owner = THIS_MODULE,
	.open = stred_open,
	.read = stred_read,
	.write = stred_write,
	.release = stred_close,
};


int stred_open(struct inode *pinode, struct file *pfile) 
{
		printk(KERN_INFO "Succesfully opened stred\n");
		return 0;
}

int stred_close(struct inode *pinode, struct file *pfile) 
{
		printk(KERN_INFO "Succesfully closed stred\n");
		return 0;
}

ssize_t stred_read(struct file *pfile, char __user *buffer, size_t length, loff_t *offset) 
{
	int ret;
	char buff[BUFF_SIZE];
	long int len;
	if (end_read){
		end_read = 0;
		printk(KERN_INFO "Succesfully read from stred\n");
		return 0;
	}
	if(pos > 0)
	{
		len = scnprintf(buff, BUFF_SIZE, "%s\n", stred);
		ret = copy_to_user(buffer, buff, len);
		if(ret)
			return -EFAULT;
		end_read = 1;
	}
	else
	{
		printk(KERN_INFO "Stred is empty!\n");
		return 0;
	}

	return len;
}

ssize_t stred_write(struct file *pfile, const char __user *buffer, size_t length, loff_t *offset) 
{
	char buff[150];
	char data[BUFF_SIZE];
	char command[8];
	int ret;
	int data_length;

	ret = copy_from_user(buff, buffer, length);
	if(ret)
		return -EFAULT;
	buff[length-1] = '\0';

	ret = sscanf(buff,"%10[^= ]=%99[^\t\n=]", command, data);

	printk(KERN_INFO "Command:%s\n", command);

	if(ret == 2)
	{
		printk(KERN_INFO "Data: %s\n", data);
	}
	else if(ret == 1)
	{
		printk(KERN_WARNING "No data\n");
	}


//STRING
	if(!strcmp(command,"string"))
	{
		pos = 0;
		data_length = strlen(data);
		if(data_length < 100)
		{
			strncpy(stred,data,data_length);
			printk(KERN_INFO "Data is successfully stored in stred\n");
			stred[data_length] = '\0';
			pos = data_length;
		}
		else
		{
			printk(KERN_WARNING "Data is to big\n");
		}
		printk(KERN_INFO "Command "string" was executed\n");
	}


//CLEAR
	if(!strcmp(command,"clear"))
	{
		pos = 0;
		stred[0] = '\0'; 
		printk(KERN_INFO "Command "clear" was executed\n");
		
	} 


//SHRINK
	if(!strcmp(command,"shrink"))
	{
		int i;
		if(stred[pos-1] == ' ')  
		{
			stred[pos-1] = '\0';
		}
		
		if(stred[0] == ' ')
		{	
			for(i = 1; i < pos; i++){
				stred[i-1] = stred[i];
			}
		}
		printk(KERN_INFO "Command "shrink" was executed\n");
	}


//APPEND
	if(!strcmp(command,"append"))
	{
		int i;	
		data_length = strlen(data);
		if((pos + data_length) < 100)
		{
			for(i = 0; i < data_length; i++)
			{
				stred[pos+i] = data[i];
			}
			pos+=data_length;
			stred[pos] = '\0';
		}
		else
		{
			printk(KERN_WARNING "Data cannot be added, there are not enough free spaces in the buffer\n");
		}
		printk(KERN_INFO "Command "append" was executed\n");
	}


//TRUNCATE
	if(!strcmp(command,"truncat"))
	{
		int i;
		char *l;
		i = simple_strtol(data,&l,10);

		printk(KERN_INFO "Number of characters for the truncut: %d\n",i);
		printk(KERN_INFO "Srting  is: %s\n",l);

		if((pos - i) < 0)
		{
			pos = 0;
			stred[0] = '\0';
			printk(KERN_WARNING "The number of characters given for truncut is too large, the command cannot be executed\n");	
		}
		else
		{
			pos-=i;
			stred[pos] = '\0';
			printk(KERN_INFO "command "truncat" was executed\n");
		}	
	}


//REMOVE
	if(!strcmp(command,"remove"))
	{
		int i, j, k, l, n;
		int remove_pos = 0;
		data_length = strlen(data);

		for(i = 0; i < pos; i++)
		{
			if(data[0] == stred[i])
			{	
				l = i+1;
				for(j = 1; j < data_length; j++)
				{
					if(data[j] == stred[l])
						remove_pos++;
					l++;
				}

				
				if(remove_pos == data_length-1)
				{	
					n = l;

					for(k = 0; k < (pos - l + 1); k++)
					{
						stred[n - data_length] = stred[n];
						n++;
					}
					pos-=data_length;

					printk(KERN_INFO "command "remove" was executed\n");
				}
				remove_pos = 0;
			}			
		}
	}

	
	return length;
}

static int __init stred_init(void)
{
   int ret = 0;
	
   sema_init(&sem,1);

   ret = alloc_chrdev_region(&my_dev_id, 0, 1, "stred");
   if (ret){
      printk(KERN_ERR "failed to register char device\n");
      return ret;
   }
   printk(KERN_INFO "char device region allocated\n");

   my_class = class_create(THIS_MODULE, "stred_class");
   if (my_class == NULL){
      printk(KERN_ERR "failed to create class\n");
      goto fail_0;
   }
   printk(KERN_INFO "class created\n");
   
   my_device = device_create(my_class, NULL, my_dev_id, NULL, "stred");
   if (my_device == NULL){
      printk(KERN_ERR "failed to create device\n");
      goto fail_1;
   }
   printk(KERN_INFO "device created\n");

	my_cdev = cdev_alloc();	
	my_cdev->ops = &my_fops;
	my_cdev->owner = THIS_MODULE;
	ret = cdev_add(my_cdev, my_dev_id, 1);
	if (ret)
	{
      printk(KERN_ERR "failed to add cdev\n");
		goto fail_2;
	}
   printk(KERN_INFO "cdev added\n");
   printk(KERN_INFO "Hello world\n");

   return 0;

   fail_2:
      device_destroy(my_class, my_dev_id);
   fail_1:
      class_destroy(my_class);
   fail_0:
      unregister_chrdev_region(my_dev_id, 1);
   return -1;
}

static void __exit stred_exit(void)
{
   cdev_del(my_cdev);
   device_destroy(my_class, my_dev_id);
   class_destroy(my_class);
   unregister_chrdev_region(my_dev_id,1);
   printk(KERN_INFO "Goodbye, cruel world\n");
}


module_init(stred_init);
module_exit(stred_exit);