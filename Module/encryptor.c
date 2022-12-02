/**************************************************************
* Class:  CSC-415-02 Fall 2022
* Name: Mark Kim
* Student ID: 918204214
* GitHub ID: mkim797
* Project: Assignment 6 – Device Driver
*
* File: encryptor.c
*
* Description: This is an encryption character device driver.  It takes
* character input and will apply a caesar cipher to it.
*
**************************************************************/

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/vmalloc.h>
#include <linux/sched.h>

#define MY_MAJOR 415
#define MY_MINOR 0
#define DEVICE_NAME "encryptor"
#define BUFFER_SIZE 512
#define KEY 5  // default key

#define ENCRYPT _IO('e', 0)
#define DECRYPT _IO('e', 1)
#define SETKEY _IO('e', 2)

static ssize_t myRead(struct file * fs, char __user * buf, size_t hsize, loff_t * off);
static ssize_t myWrite(struct file * fs, const char __user * buf, size_t hsize, loff_t * off);
static int myOpen(struct inode * inode, struct file * fs);
static int myClose(struct inode * inode, struct file * fs);
static int encrypt(int key);
static int decrypt(int key);
static long myIoCtl (struct file * fs, unsigned int command, unsigned long data);

char *kernel_buffer;

struct cdev my_cdev;

MODULE_AUTHOR("Mark Kim");
MODULE_DESCRIPTION("A simple encryption/decryption program");
MODULE_LICENSE("GPL");

// file operations data structure
struct file_operations fops = {
    .open = myOpen,
    .release = myClose,
    .write = myWrite,
    .read = myRead,
    .unlocked_ioctl = myIoCtl,
    .owner = THIS_MODULE
};

// data structure used for holding the encryption key and a flag that indicates
// whether the data is encrypted or decrypted
typedef struct encds {
    int key;
    int flag; // 0 = unencrypted, 1 = encrypted
} encds;

// this write function takes in the buffer from user space and brings it into
// kernel space.  then it encrypts it in the kernel buffer.
// returns how many bytes were passed in.
static ssize_t myWrite (struct file * fs, const char __user * buf, size_t hsize, loff_t * off) {
    int err;
    struct encds * ds;

    // printk(KERN_INFO "********************* inside myWrite *********************");

    ds = (struct encds *) fs->private_data;

    err = copy_from_user(kernel_buffer + *off, buf, hsize);

    printk(KERN_SOH "WRITE: Offset: %lld", *off);

    if (err != 0) {
        printk(KERN_ERR "encrypt: copy_from_user failed: %d bytes failed to copy\n", err);
        return -1;
    }

    encrypt(ds->key);

    ds->flag = 1;

    return hsize;
}

// this read function decrypts the data in the kernel buffer then copies it back
// to the user space.
static ssize_t myRead (struct file * fs, char __user * buf, size_t hsize, loff_t * off) {
    int err, bufLen;
    struct encds * ds;

    // printk(KERN_INFO "************* inside myRead *************");

    ds = (struct encds *) fs->private_data;

    bufLen = strlen(kernel_buffer);

    printk(KERN_SOH "READ: Buffer Length: %d    Offset: %lld", bufLen, *off);

    if (bufLen < hsize) {
        hsize = bufLen;
    }

    // decrypt(ds->key);

    // ds->flag = 0;
    
    err = copy_to_user(buf, kernel_buffer + *off, hsize);
    
    if (err != 0) {
        printk(KERN_ERR "decrypt: copy_to_user failed: %d bytes failed to copy\n", err);
        return -1;
    }

    return 0;
}

// this open function initialized the encds data structure and saves it in the
// private data of the file system descriptor
static int myOpen(struct inode * inode, struct file * fs) {
    struct encds * ds;

    ds = vmalloc(sizeof(struct encds));

    if (ds == 0) {
        printk(KERN_ERR "Cannot vmalloc, File not opened.\n");
        return -1;
    }

    ds->key = KEY;
    ds->flag = 0;
    fs->private_data = ds;
    return 0;
}

// this close function frees the file system private data memory
static int myClose(struct inode * inode, struct file * fs) {
    struct encds * ds;

    ds = (struct encds *) fs->private_data;
    vfree(ds);
    return 0;
}

// caesar cypher encryption function
static int encrypt(int key) {
    int i, bufLen;

    bufLen = strlen(kernel_buffer);

    for (i = 0; i < bufLen - 1; i++) {
        kernel_buffer[i] = ((kernel_buffer[i] - key) % 128) + 1;
    }

    printk(KERN_INFO "Encrypted Text:\n%s\n", kernel_buffer);

    return 0;
}

// decrypt function
static int decrypt(int key) {
    int i, bufLen;

    bufLen = strlen(kernel_buffer);

    for (i = 0; i < bufLen - 1; i++) {
        kernel_buffer[i] = (kernel_buffer[i] + key - 1) % 128;
    }

    printk(KERN_INFO "Decrypted Text:\n%s\n", kernel_buffer);

    return 0;
}

// this is a way to deal with device files where the data has already been
// encrypted/decrypted; it also allows for the key to be reset
static long myIoCtl (struct file * fs, unsigned int command, unsigned long data) {
    struct encds * ds;

    printk(KERN_INFO "inside myIoCtl");

    ds = (struct encds *) fs->private_data;
    switch (command) {
        case ENCRYPT:
            encrypt(ds->key);
            ds->flag = 1;
            break;
        case DECRYPT:
            decrypt(ds->key);
            ds->flag = 0;
            break;
        case SETKEY:
            ds->key = (int) data;
            break;
        default:
            printk(KERN_ERR "myIoCtl: invalid command entered");
    }
    return 0;
}

// creates a device node in /dev, returns error if not made
int init_module(void) {
    int registers, result;
    dev_t devno;

    devno = MKDEV(MY_MAJOR, MY_MINOR);

    registers = register_chrdev_region(devno, 1, DEVICE_NAME);
    printk(KERN_INFO "Register chardev succeeded 1: %d\n", registers);
    cdev_init(&my_cdev, &fops);

    kernel_buffer = vmalloc(BUFFER_SIZE);
    if (kernel_buffer == NULL) {
        printk(KERN_ERR "Failed to vmalloc kernel_buffer.\n");
        return -1;
    }

    result = cdev_add(&my_cdev, devno, 1);

    return result;
}

// removes and destroys module
void cleanup_module(void) {
    dev_t devno;

    devno = MKDEV(MY_MAJOR, MY_MINOR);
    unregister_chrdev_region(devno, 1);
    cdev_del(&my_cdev);

    vfree(kernel_buffer);
    kernel_buffer = NULL;

    printk(KERN_INFO "Goodbye from encryptor driver.\n");
}