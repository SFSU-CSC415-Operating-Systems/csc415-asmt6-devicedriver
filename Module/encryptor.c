/**************************************************************
* Class:  CSC-415-02 Fall 2022
* Name: Mark Kim
* Student ID: 918204214
* GitHub ID: mkim797
* Project: Assignment 6 – Device Driver
*
* File: encryptor.c
*
* Description:
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
#define KEY 5

static ssize_t myRead(struct file * fs, char __user * buf, size_t hsize, loff_t * off);
static ssize_t myWrite(struct file * fs, const char __user * buf, size_t hsize, loff_t * off);
static int myOpen(struct inode * inode, struct file * fs);
static int myClose(struct inode * inode, struct file * fs);
static long myIoCtl (struct file * fs, unsigned int command, unsigned long data);

char *kernel_buffer;

struct cdev my_cdev;

MODULE_AUTHOR("Mark Kim");
MODULE_DESCRIPTION("A simple encryption/decryption program");
MODULE_LICENSE("GPL");

// another data structure
struct file_operations fops = {
    .open = myOpen,
    .release = myClose,
    .write = myWrite,
    .read = myRead,
    .unlocked_ioctl = myIoCtl,
    .owner = THIS_MODULE
};

// data structure used for keeping count of how may times data is written
typedef struct encds {
    int key;
} encds;

// this write function increments the data structure's count every time it is called.
// NOTE - data is not physically written anywhere.
// returns how many bytes were passed in.
static ssize_t myWrite (struct file * fs, const char __user * buf, size_t hsize, loff_t * off) {
    int err, i, bufLen;
    struct encds * ds;

    printk(KERN_INFO "inside myWrite");

    ds = (struct encds *) fs->private_data;

    err = copy_from_user(kernel_buffer + *off, buf, hsize);
    *off += hsize;

    if (err != 0) {
        printk(KERN_ERR "encrypt: copy_from_user failed: %d bytes failed to copy\n", err);
        return -1;
    }

    bufLen = strlen(kernel_buffer);

    printk(KERN_INFO "Before Encrypt\nKernel Buffer Length: %d:\n%s\n", bufLen, kernel_buffer);

    for (i = 0; i < bufLen - 1; i++) {
        kernel_buffer[i] = (kernel_buffer[i] + ds->key) % 128;
    }

    printk(KERN_INFO "After Encrypt:\n%s\n", kernel_buffer);

    return hsize;
}

static ssize_t myRead (struct file * fs, char __user * buf, size_t hsize, loff_t * off) {
    int err, i, bufLen;
    struct encds * ds;

    printk(KERN_INFO "inside myRead");

    ds = (struct encds *) fs->private_data;

    bufLen = strlen(kernel_buffer);

    printk(KERN_INFO "Before Decrypt\nKernel Buffer Length: %d\n%s\n", bufLen, kernel_buffer);

    for (i = 0; i < bufLen - 1; i++) {
        kernel_buffer[i] = (kernel_buffer[i] - ds->key) % 128;
    }

    printk(KERN_INFO "After Decrypt:\n%s\n", kernel_buffer);
    
    err = copy_to_user(buf, kernel_buffer + *off, bufLen);
    *off += bufLen;
    
    if (err != 0) {
        printk(KERN_ERR "decrypt: copy_to_user failed: %d bytes failed to copy\n", err);
        return -1;
    }

    printk(KERN_INFO "Decrypt End");

    return bufLen;
}

static int myOpen(struct inode * inode, struct file * fs) {
    struct encds * ds;

    printk(KERN_INFO "inside myOpen");

    ds = vmalloc(sizeof(struct encds));

    if (ds == 0) {
        printk(KERN_ERR "Cannot vmalloc, File not opened.\n");
        return -1;
    }

    ds->key = KEY;
    fs->private_data = ds;
    return 0;
}

static int myClose(struct inode * inode, struct file * fs) {
    struct encds * ds;

    printk(KERN_INFO "inside myClose");

    ds = (struct encds *) fs->private_data;
    vfree(ds);
    return 0;
}

// this is a way to deal with device files where there may not be read/write
// basically counts how many times "write" was called
static long myIoCtl (struct file * fs, unsigned int command, unsigned long data) {
    struct encds * ds;

    printk(KERN_INFO "inside myIoCtl");

    ds = (struct encds *) fs->private_data;
    if (command < 0) {
        printk(KERN_ERR "failed in myioctl.\n");
        return -1;
    }
    // count = (int *) data;
    // *count = ds->count;
    return 0;
}

// creates a device node in /dev, returns error if not made
int init_module(void) {
    int registers, result;
    dev_t devno;

    printk(KERN_INFO "inside init_module");

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

    printk(KERN_INFO "after cdev_add");

    return result;
}

void cleanup_module(void) {
    dev_t devno;

    printk(KERN_INFO "inside cleanup_module");

    devno = MKDEV(MY_MAJOR, MY_MINOR);
    unregister_chrdev_region(devno, 1);
    cdev_del(&my_cdev);

    printk(KERN_INFO "Goodbye from encryptor driver.\n");
}