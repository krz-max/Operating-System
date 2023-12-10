// kfetch_mod.c
#include <linux/atomic.h> 
#include <linux/cdev.h> 
#include <linux/delay.h> 
#include <linux/device.h> 
#include <linux/fs.h> 
#include <linux/init.h> 
#include <linux/kernel.h> /* for sprintf() */ 
#include <linux/module.h> 
#include <linux/printk.h> 
#include <linux/types.h> 
#include <linux/uaccess.h> /* for get_user and put_user */ 
#include <linux/version.h> 
#include <linux/utsname.h> // for struct new_utsname
#include <linux/sched.h>
#include <linux/mm.h>
#include <linux/jiffies.h>

#include <asm/errno.h> 

MODULE_LICENSE("GPL"); 
MODULE_AUTHOR("LKMPG"); 
MODULE_DESCRIPTION("A sample driver"); 

#define DEVICE_NAME "kfetch"
#define SUCCESS 0
#define BUF_SIZE 1024

#define KFETCH_NUM_INFO 6

#define KFETCH_RELEASE   (1 << 0)
#define KFETCH_NUM_CPUS  (1 << 1)
#define KFETCH_CPU_MODEL (1 << 2)
#define KFETCH_MEM       (1 << 3)
#define KFETCH_UPTIME    (1 << 4)
#define KFETCH_NUM_PROCS (1 << 5)

#define KFETCH_FULL_INFO ((1 << KFETCH_NUM_INFO) - 1);

static int major_number;
static struct class *cls;
static int mask_info = KFETCH_FULL_INFO;
static char kfetch_buf[BUF_SIZE + 1]; /* The msg the device will give when asked */ 
static const char logo[7][32] = {"        .-.        ",
                                 "       (.. |       ",
                                 "       <>  |       ",
                                 "      / --- \\      ",
                                 "     ( |   | |     ",
                                 "   |\\\\_)___/\\)/\\   ",
                                 "  <__)------(__/   "};

enum { 
    CDEV_NOT_USED = 0, 
    CDEV_EXCLUSIVE_OPEN = 1, 
}; 
/* Is device open? Used to prevent multiple access to device */ 
static atomic_t already_open = ATOMIC_INIT(CDEV_NOT_USED); 

static int kfetch_open(struct inode *, struct file *); 
static int kfetch_release(struct inode *, struct file *); 
static ssize_t kfetch_read(struct file *, char __user *, size_t, loff_t *); 
static ssize_t kfetch_write(struct file *, const char __user *, size_t, loff_t *); 
static const struct file_operations kfetch_ops = {
    .owner   = THIS_MODULE,
    .read    = kfetch_read,
    .write   = kfetch_write,
    .open    = kfetch_open,
    .release = kfetch_release,
};
// Function to get system information based on the mask
static void get_system_info(unsigned int info_mask, char *output_buffer)
{
    struct new_utsname *utsname_info;

    // Get the utsname structure
    utsname_info = utsname();

    char hostname[64], releaseInfo[64] = "";
    int hostname_length = snprintf(hostname, 64, utsname_info->nodename);
    int current_logo_row = 0;
    
    snprintf(output_buffer, BUF_SIZE, "                   %s\n%s------\n", utsname_info->nodename, logo[current_logo_row++]);

    if (mask_info & KFETCH_RELEASE) {
        pr_info("Release information\n");
        int release_length = snprintf(releaseInfo, 64, "Kernel:   %s\n", utsname_info->release);
        strcat(output_buffer, logo[current_logo_row++]);
        strcat(output_buffer, releaseInfo);
    }
    char CPUModel[64] = "";
    if (mask_info & KFETCH_CPU_MODEL) {
        pr_info("CPU Model information\n");
        snprintf(CPUModel, 64, "CPU:      Intel(R) Pentium(R) Gold G5400 CPU @ 3.70GHz\n");
        strcat(output_buffer, logo[current_logo_row++]);
        strcat(output_buffer, CPUModel);
    }
    int online_cpu = num_online_cpus();
    int possible_cpus = num_possible_cpus();
    char CPUInfo[64] = "";
    if (mask_info & KFETCH_NUM_CPUS) {
        pr_info("Number of CPUs information\n");
        snprintf(CPUInfo, 64, "CPUs:     %d / %d\n", num_online_cpus(), num_possible_cpus());
        strcat(output_buffer, logo[current_logo_row++]);
        strcat(output_buffer, CPUInfo);    
    }
    struct sysinfo si;
    si_meminfo(&si);
    // Calculate free and total memory in MB
    unsigned long free_mem_mb = (si.freeram * si.mem_unit) >> 20;
    unsigned long total_mem_mb = (si.totalram * si.mem_unit) >> 20;    
    char MemInfo[64] = "";
    if (mask_info & KFETCH_MEM) {
        pr_info("Memory information\n");
        snprintf(MemInfo, 64, "Mem:      %lu MB / %lu MB\n", free_mem_mb, total_mem_mb);
        strcat(output_buffer, logo[current_logo_row++]);
        strcat(output_buffer, MemInfo);        
    }
    struct task_struct *task;
    int num_processes = 0;
    // Iterate through the list of processes
    for_each_process(task) {
        num_processes++;
    }
    char ProcInfo[64] = "";
    if (mask_info & KFETCH_NUM_PROCS) {
        pr_info("Number of Processes information\n");
        snprintf(ProcInfo, 64, "Procs:    %d\n", num_processes);
        strcat(output_buffer, logo[current_logo_row++]);
        strcat(output_buffer, ProcInfo);        
    }
    // jiffies represents the number of clock ticks since system boot
    unsigned long uptime_jiffies = jiffies;

    // HZ is the number of clock ticks per second
    unsigned long uptime_seconds = jiffies_to_msecs(uptime_jiffies) / 1000;
    unsigned long uptime_minutes = uptime_seconds / 60;
    char UptimeInfo[64] = "";
    if (mask_info & KFETCH_UPTIME) {
        pr_info("Uptime information\n");
        snprintf(UptimeInfo, 64, "Uptime:   %lu mins\n", uptime_minutes);
        strcat(output_buffer, logo[current_logo_row++]);
        strcat(output_buffer, UptimeInfo);        
    }
    while(current_logo_row < 7){
        strcat(output_buffer, logo[current_logo_row++]);
        strcat(output_buffer, "\n");
    }
}

/* Called when a process, which already opened the dev file, attempts to 
 * read from it. 
 */ 
static ssize_t kfetch_read(struct file *file, char __user *user_buffer, size_t length, loff_t *offset) {
    /* Number of bytes actually written to the buffer */ 

    pr_info("kfetch_read with the mask info: %d\n", mask_info);


    // Get system information based on the mask
    get_system_info(mask_info, kfetch_buf);
    
    // Copy the information to user space
    if (copy_to_user(user_buffer, kfetch_buf, strlen(kfetch_buf)) != 0) {
        printk(KERN_ERR "Failed to copy data to user space\n");
        return -EFAULT;
    }

    // Move the file offset and return the number of bytes read
    *offset += length;

    return strlen(kfetch_buf);
}


/* Methods */ 
 
/* Called when a process tries to open the device file, like 
 * "sudo cat /dev/chardev" 
 */ 
static int kfetch_open(struct inode *inode, struct file *file) 
{ 

    if (atomic_cmpxchg(&already_open, CDEV_NOT_USED, CDEV_EXCLUSIVE_OPEN)) 
        return -EBUSY; 
 
    try_module_get(THIS_MODULE); 
 
    return SUCCESS; 
} 
 
/* Called when a process closes the device file. */ 
static int kfetch_release(struct inode *inode, struct file *file) 
{ 
    /* We're now ready for our next caller */ 
    atomic_set(&already_open, CDEV_NOT_USED); 
 
    /* Decrement the usage count, or else once you opened the file, you will 
     * never get rid of the module. 
     */ 
    module_put(THIS_MODULE); 
 
    return SUCCESS; 
} 
/* Called when a process writes to dev file: echo "hi" > /dev/hello */ 
static ssize_t kfetch_write(struct file *file, const char __user *user_buffer, size_t length, loff_t *offset) {

    if (copy_from_user(&mask_info, user_buffer, length)) {
        pr_alert("Failed to copy data from user");
        return -EFAULT;  // Error copying data from user space
    }

    pr_info("kfetch_write captures the mask info: %d\n", mask_info);

    return sizeof(int);
}


static int __init kfetch_mod_init(void) {
    // Register the character device
    major_number = register_chrdev(0, DEVICE_NAME, &kfetch_ops);
    if (major_number < 0) {
        pr_alert("Failed to register a major number\n");
        return major_number;
    }

    pr_info("kfetch_mod module loaded with major number %d\n", major_number);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 4, 0) 
    cls = class_create(DEVICE_NAME); 
#else 
    cls = class_create(THIS_MODULE, DEVICE_NAME); 
#endif 
    device_create(cls, NULL, MKDEV(major_number, 0), NULL, DEVICE_NAME); 
 
    pr_info("Device created on /dev/%s\n", DEVICE_NAME); 
 
    return SUCCESS; 
}

static void __exit kfetch_mod_exit(void) {
    device_destroy(cls, MKDEV(major_number, 0)); 
    class_destroy(cls); 

    // Unregister the character device
    unregister_chrdev(major_number, DEVICE_NAME);

    pr_info("kfetch_mod module unloaded\n");
}
module_init(kfetch_mod_init);
module_exit(kfetch_mod_exit);
