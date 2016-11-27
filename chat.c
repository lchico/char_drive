/* Copyright (C) 2015, Santiago F. Maudet
 * This file is part of char01 module.
 *
 * char01 module is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * char01 module is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/* !\brief This is a basic example of a char device.
 *         Basics of LKM are described.
 *         This module has a initialized buffer in kernel memory that you
 *         can read from user space.
 *         Usage:
 *         1) COMPILE = make
 *         2) Insert module into kernel (root): insmod char_01.ko
 *         3) Create /dev/char_01 node: mknod /dev/char_01 c [MAYOR_NUMBER] [MINOR_NUMBER]
 *         4) cat /dev/char_01 or use open and read from a C program.
 *
*/
#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>

#include <linux/types.h>
#include <linux/kdev_t.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>

#include <linux/version.h>
#include <linux/device.h>
#include <linux/atomic.h>
#include <asm/atomic.h>
#include <linux/spinlock.h>
#include <linux/smp.h>

#include <linux/slab.h>  // to use kmalloc and kfree
#include <linux/errno.h>  // to use errors define
//#include <linux/errno-base.h>  // to use errors define

#include <linux/hrtimer.h>
#include <linux/pagemap.h>

#include <linux/version.h>

#include <linux/uaccess.h>

#define TEST_CLASSS "chat1"
#define test_class "chat_class"
#define MODULE_NAME "chat"


#define CHAR_01_MINOR 0     //Start MINOR..
#define CHAR_01_N_DEVS 1    //Number o devices to register.
#define NUM_BYTES_TO_COPY 1 //Number of bytes to copy in copy_to_user call.

#define BUFFER_SIZE 20 //Number of bytes to copy in copy_to_user call.

static char  *ptr_buffer=NULL;
static char  *ptr_buffer_aux=NULL;
static char  *ptr_buffer_r=NULL;
static atomic_t nro_user=ATOMIC_INIT(0);


static struct timer_list timer_cpy_buffers;

static dev_t device;        //This variable stores the minor and major mumber of the device
struct cdev device_cdev;    //Kernel structure for char devices.
static int result;  // Aux Variable.
spinlock_t lock_buffer;
spinlock_t lock_buffer_aux;


// Timer to copy de buffer kernel to buffer_aux that the user need.
void cp_buffers( unsigned long data )
{
  int ret;
  

  //printk("Buffer=%s\n",ptr_buffer);
  spin_lock(&lock_buffer);
  memcpy(ptr_buffer_aux, ptr_buffer,BUFFER_SIZE);
  spin_unlock(&lock_buffer);


  spin_lock(&lock_buffer_aux);
  memcpy(ptr_buffer_r, ptr_buffer_aux,BUFFER_SIZE);
  spin_unlock(&lock_buffer_aux);
 
  //printk("Buffer=%s\n",ptr_buffer_aux);
 
  ret = mod_timer( &timer_cpy_buffers, jiffies + msecs_to_jiffies(800) );
  if (ret) printk("Error in mod_timer\n");
//  printk("Timer update\n");

}


int init_module_timer( void )
{
  int ret;

  printk("Timer module installing\n");

  // my_timer.function, my_timer.data
  setup_timer( &timer_cpy_buffers, cp_buffers, 0 );

  printk( "Starting timer to fire in 800ms (%ld)\n", jiffies );
  ret = mod_timer( &timer_cpy_buffers, jiffies + msecs_to_jiffies(800) );
  if (ret) printk("Error in mod_timer\n");

  return 0;
}

void cleanup_module_timmer( void )
{
  int ret;

  ret = del_timer( &timer_cpy_buffers );
  if (ret) printk("The timer is still in use...\n");

  printk("Timer module uninstalling\n");

  return;
}

// Write function //
ssize_t dev_write(struct file *filp, const char __user *buf, size_t count, loff_t *f_pos) {
    ssize_t retval=-ENOMEM;
    unsigned long i=0;
    /////Some info printed in /var/log/messages ///////
    printk(KERN_INFO "Entering dev_write function\n");
    printk(KERN_INFO "Count Parameter from user space %lu.\n", count);
    printk(KERN_INFO "Offset in buffer %lu.\n",(long unsigned int)*f_pos);
    ///////////////////////////////////////////////////
    if (!ptr_buffer){
        printk("Error call mallok, %s, %i\n",__FUNCTION__,__LINE__);
    }else{
        printk("Memory ok.\n");
    }

    if( *f_pos+count > BUFFER_SIZE ){
	printk("Buffer kernel complete.");
	return -EFAULT;
    }
    spin_lock(&lock_buffer);
    //if(copy_from_user(ptr_buffer,buf,fpos)){
    retval=strncpy_from_user(ptr_buffer,buf,count);
    spin_unlock(&lock_buffer);
    if( retval < 0 ){
        printk("Error copy from user, %s, %i\n",__FUNCTION__,__LINE__);
	retval=-EFAULT;
    }else{
        retval=count;
	*f_pos=count;
    }

   // printk("Recibido de buffer user: %s\n",ptr_buf);    
    printk("Valores offset  %lu, %s\n",i,ptr_buffer);
	
    return retval;  // returned a single character. Ok
}

// Read function //
ssize_t dev_read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos) {
   long unsigned int retval=0;
   /////Some info printed in /var/log/messages ///////
   printk(KERN_INFO "Entering dev_read function\n");
   printk(KERN_INFO "Count Parameter from user space %lu.\n", count);
   printk(KERN_INFO "Offset in buffer %lu.\n",(long unsigned int)*f_pos);

   printk("Buffer=%s\n",ptr_buffer_aux);
   spin_lock(&lock_buffer_aux);
   retval = copy_to_user((void *)buf,(const void *)ptr_buffer_aux,(unsigned long)f_pos);
   spin_unlock(&lock_buffer_aux);

   if (retval == 0){
	*f_pos=count;
   }else{
	*f_pos=retval;
   }
   if(retval >= BUFFER_SIZE-1){
	retval=0;
   }

   return retval;  // returned a single character. Ok
}

int open(struct inode * no,struct file *fd){
	
    //printk("Ingreo Open Function.\n  Nro user: %i\n",atomic_read(&nro_user));
    spin_lock_init(&lock_buffer);
    spin_lock_init(&lock_buffer_aux);

    if(atomic_read(&nro_user) == 0){
        ptr_buffer = kmalloc(sizeof(char)*BUFFER_SIZE, GFP_KERNEL);
        ptr_buffer_aux = kmalloc(sizeof(char)*BUFFER_SIZE, GFP_KERNEL);
        ptr_buffer_r = kmalloc(sizeof(char)*BUFFER_SIZE, GFP_KERNEL);
        if ( !ptr_buffer && !ptr_buffer_aux &&  !ptr_buffer_r ){
            printk("Error call mallok, %s, %i\n",__FUNCTION__,__LINE__);
        }else{
            printk("Ingreso el escritor del grupo.Mem OK\n");
	    memset(ptr_buffer,0,BUFFER_SIZE);
	    memset(ptr_buffer_aux,0,BUFFER_SIZE);
	    memset(ptr_buffer_r,0,BUFFER_SIZE);
            // Init timer to copy buffer from kernel buffer to the kernel buffer 
            init_module_timer( );
        }    	
    }else{
            printk("Ingreso del lector nro: %i\n",atomic_read(&nro_user));
    }
    atomic_inc(&nro_user);
    return 0;
}



int chat_close(struct inode *inode, struct file *flip){
    if(!atomic_dec_and_test(&nro_user)){
        printk("Un lector a dejado el chat, quedan %i\n",atomic_read(&nro_user));
       
    }else{
        printk("El escrito dejo el chat.\n");

        if (ptr_buffer){
            cleanup_module_timmer();
            kfree(ptr_buffer);
        }
    }
    return 0;
}


struct file_operations dev_fops = { //Struct File Operations, this module only supports read...
	.owner = THIS_MODULE,           // Tells who is owner of struct file_operations
	.open = open,
	.write = dev_write,
	.read = dev_read,               // Function pointer init with dev_read function.
	.release = chat_close,
};



static struct class *my_first_class;

// Init Function //
static int __init dev_init(void)
{
    printk(KERN_INFO "Loading module char_01\n"); //Kernel Info

    // Dynamic Allocation of MAJOR Number for char_01 Module
    result = alloc_chrdev_region(&device,CHAR_01_MINOR,CHAR_01_N_DEVS, "char_01");

    //Can't get MAJOR Number, return...
    if(result < 0){
      printk(KERN_WARNING "char_01: can't get major\n");
      return result;
    }
		
	my_first_class=class_create(THIS_MODULE, "TEST_CLASSS");
	device_create(my_first_class,NULL,device,NULL,"test_class");

    // Initialize struct cdev "device_cdev" with struct file_operations "dev_fops"
    cdev_init(&device_cdev, &dev_fops);

    device_cdev.owner = THIS_MODULE;  //Tells who is owner of struct cdev

    // Add device to kernel.
    result = cdev_add(&device_cdev, device,CHAR_01_N_DEVS);
    if(result < 0){
      printk(KERN_WARNING "char_01 can't be registered in kenel\n");
      return result;
    }
    printk(KERN_INFO "Correct Registration of device char_01...\n");
    return 0;
}

// Exit Function //
static void __exit dev_exit(void)
{
    printk(KERN_INFO "Unloading Module CHAR_01\n");
    cdev_del(&device_cdev); //Remove device form kernel.
		printk("%s , line=%d\n",__FUNCTION__,__LINE__);
    device_destroy(my_first_class,device);
		printk("%s , line=%d\n",__FUNCTION__,__LINE__);
		class_destroy(my_first_class);
		printk("%s , line=%d\n",__FUNCTION__,__LINE__);
    unregister_chrdev_region(device,CHAR_01_N_DEVS);//Release MAJOR and MINIOR Numbers.

}

module_init(dev_init); //Init Macro loaded with init function.
module_exit(dev_exit); //Exit Macro loaded with exit function.

MODULE_AUTHOR("Santiago F. Maudet, DPLab @ UTN.BA http://www.electron.frba.utn.edu.ar/dplab");
MODULE_LICENSE("GPL");
