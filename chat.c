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
#include <linux/mutex.h>

#include <linux/slab.h>  // to use kmalloc and kfree
#include <linux/errno.h>  // to use errors define

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

#define BUFFER_SIZE 21 //Number of bytes to copy in copy_to_user call.

static char  *ptr_buffer_in=NULL;
static char  *ptr_buffer_aux=NULL;
static char  *ptr_buffer_out=NULL;
static atomic_t nro_user=ATOMIC_INIT(-1);

struct mutex mutex_buffer_in;
struct mutex mutex_buffer_out;
struct mutex mutex_read_sync;

static loff_t index_writer=0;
static loff_t index_last_writer=0;
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
  unsigned int diff_aux=0;
  
  diff_aux=index_writer-index_last_writer;
  printk("diff=%lu\n",diff_aux);
  if ( diff_aux > 0 ){
	spin_lock(&lock_buffer);
	memcpy(ptr_buffer_aux, ptr_buffer_in+index_last_writer,diff_aux);
	spin_unlock(&lock_buffer);

  	spin_lock(&lock_buffer_aux);
 	memcpy(ptr_buffer_out, ptr_buffer_aux+index_last_writer,diff_aux);
 	spin_unlock(&lock_buffer_aux);
  	index_last_writer+=diff_aux;
	mutex_unlock(&mutex_read_sync);
  }
  ret = mod_timer( &timer_cpy_buffers, jiffies + msecs_to_jiffies(800) );
  if (ret) printk("Error in mod_timer\n");
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
    if (!ptr_buffer_in){
        printk("Error call mallok, %s, %i\n",__FUNCTION__,__LINE__);
    }else{
        printk("Memory ok.\n");
    }

    // Only if the user wanna write overflow
    if( *f_pos+count > BUFFER_SIZE - 2){
	count=BUFFER_SIZE - *f_pos - 1 ; // this less 1 is \0 
    }else{// error en el tamanio del buffer
}

//    spin_lock(&lock_buffer); // Este no libera hasta tomar el semaforo, con lo cual no conmuta el scheduler(Usar solo en caso de inrettupciones de hardware). Por ello usar mutex(todo lo que sea necesario interactuar con el usuario). Si esta bloqueado pone a dormir el proceso hasta que sea libreado

	//mutex
    mutex_lock(&mutex_buffer_in);
    retval=copy_from_user(ptr_buffer_in+*f_pos,buf,count);
    mutex_unlock(&mutex_buffer_in);

    if( retval < 0 ){
        printk("Error copy from user, %s, %i\n",__FUNCTION__,__LINE__);
	retval=-EFAULT;
    }else{
        retval=count;
	*f_pos+=count;
	index_writer=count;
    }
    // This two is for leave \0 to end of each chat
    if ( *f_pos >= BUFFER_SIZE -1 ){
	printk("Buffer complet. Writer exit");
	return 0;
    }

   // printk("Recibido de buffer user: %s\n",ptr_buf);    
    printk("Valores offset  %lu, %s\n",i,ptr_buffer_in);
	
    return retval;  // returned a single character. Ok
}

// Read function //
ssize_t dev_read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos) {
   long int retval=-1;
   loff_t diff_aux=0;
   char *buffer_reader; 
   /////Some info printed in /var/log/messages ///////
   printk(KERN_INFO "Entering dev_read function\n");
   printk(KERN_INFO "Count Parameter from user space %lu.\n", count);
   printk(KERN_INFO "Offset in buffer %lu.\n",(long unsigned int)*f_pos);

   // Check if the offset is out of de buffer
   if(*f_pos >= BUFFER_SIZE-1){
	return 0;
   }

   printk("Blockeo de sync.\n");
   mutex_lock(&mutex_read_sync);
   printk("Salgo del blockeo de sync.\n");
   /* Diferencia entre lo leido y si hay datos por leer del buffer */
   diff_aux=index_last_writer-*f_pos;

   //printk("Buffer=%s, Posision del puntero:%lu\n",ptr_buffer_aux);

   if ( (buffer_reader = kzalloc(sizeof(char)*diff_aux, GFP_KERNEL)) ){
	printk("Error calling kzalloc %s, %i .\n",__FUNCTION__,__LINE__);
   }

   mutex_lock(&mutex_buffer_out);
   memcpy(buffer_reader,ptr_buffer_out+*f_pos,diff_aux);
   mutex_unlock(&mutex_buffer_out);

   mutex_unlock(&mutex_read_sync);

   retval = copy_to_user((char *)buf,buffer_reader,diff_aux); 

   kfree(buffer_reader);

   if ( 0 == retval ){
	retval=diff_aux;
	*f_pos+=retval;
   }else if( retval >0 ){
	retval=diff_aux-retval;
	*f_pos+=retval;
   	printk("retval mayor q 0=%lu\n",retval);
   }else{
	retval=-1;
   }
   
   printk("retval=%lu\n",retval);
   return retval;  // returned a single character. Ok
}

int open(struct inode * no,struct file *fd){
	int retval;	
    //printk("Ingreo Open Function.\n  Nro user: %i\n",atomic_read(&nro_user));
    spin_lock_init(&lock_buffer);
    spin_lock_init(&lock_buffer_aux);

    mutex_init(&mutex_buffer_in);
    mutex_init(&mutex_buffer_out);
    mutex_init(&mutex_read_sync);
    mutex_lock(&mutex_read_sync);
	

// necesito que sea todo atomico por lo tanto la comparacion decrementar
    if(atomic_inc_and_test(&nro_user)){ // 
        ptr_buffer_in = kzalloc(sizeof(char)*BUFFER_SIZE, GFP_KERNEL);
        ptr_buffer_aux = kzalloc(sizeof(char)*BUFFER_SIZE, GFP_KERNEL);
        ptr_buffer_out = kzalloc(sizeof(char)*BUFFER_SIZE, GFP_KERNEL);
        if ( !ptr_buffer_in || !ptr_buffer_aux ||  !ptr_buffer_out ){
            printk("Error call mallok, %s, %i\n",__FUNCTION__,__LINE__);
	    retval=-1;
        }else{
            printk("Ingreso el escritor del grupo.Mem OK\n");
            // Init timer to copy buffer from kernel buffer to the kernel buffer 
            init_module_timer( );// ver error
	    index_writer=0;
	    index_last_writer=0;
	    retval=0;
        }    	
    }else{
            printk("Ingreso del lector nro: %i\n",atomic_read(&nro_user));
	    retval=0;
    }
    return retval; // ver q retorna
}



int chat_close(struct inode *inode, struct file *flip){
    if(!atomic_dec_and_test(&nro_user)){
        printk("Un lector a dejado el chat, quedan %i\n",atomic_read(&nro_user));
       
    }else{
        printk("El escrito dejo el chat.\n");

        if (ptr_buffer_in){
            cleanup_module_timmer();
            kfree(ptr_buffer_in);
            kfree(ptr_buffer_aux);
            kfree(ptr_buffer_out);
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
