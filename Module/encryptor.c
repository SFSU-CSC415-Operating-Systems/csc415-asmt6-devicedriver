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

char *kernel_buffer;

struct cdev my_cdev;

MODULE_AUTHOR("Mark Kim");
MODULE_DESCRIPTION("A simple encryption/decryption program");
MODULE_LICENSE("GPL");

// data structure used for keeping count of how may times data is written
struct myds {
    int count;
} myds;

// this write function increments the data structure's count every time it is called.
// NOTE - data is not physically written anywhere.
// returns how many bytes were passed in.
// 0 is in, 1 is out, 2 is error, 3 is the first file handle
static ssize_t myWrite (struct file * fs, const char __user * buf, size_t hsize, loff_t * off) {
    struct myds * ds;
    ds = (struct myds *) fs->private_data;
    ds->count = ds->count + 1;
    printk(KERN_INFO "We wrote : %lu on write number %d\n", hsize, ds->count);
    return hsize;
}

static ssize_t myRead (struct file * fs, char __user * buf, size_t hsize, loff_t * off) {
    struct myds * ds;
    ds = (struct myds *) fs->private_data;
    ds->count = ds->count + 1;
    printk(KERN_INFO "We read : %lu on write number %d\n", hsize, ds->count);
    return 0;
}

static int myOpen(struct inode * inode, struct file * fs) {
    struct myds * ds;
    ds = vmalloc(sizeof(struct myds));

    if (ds == 0) {
        printk(KERN_ERR "Cannot vmalloc, File not opened.\n");
        return -1;
    }

    ds->count = 0;
    fs->private_data = ds;
    return 0;
}

static int myClose(struct inode * inode, struct file * fs) {
    struct myds * ds;
    ds = (struct myds *) fs->private_data;
    vfree(ds);
    return 0;
}

// this is a way to deal with device files where there may not be read/write
// basically counts how many times "write" was called
static long myIoCtl (struct file * fs, unsigned int command, unsigned long data) {
    int * count;
    struct myds * ds;
    ds = (struct myds *) fs->private_data;
    if (command != 3) {
        printk(KERN_ERR "failed in myioctl.\n");
        return -1;
    }
    count = (int *) data;
    *count = ds->count;
    return 0;
}

// another data structure
struct file_operations fops = {
    .open = myOpen,
    .release = myClose,
    .write = myWrite,
    .read = myRead,
    .unlocked_ioctl = myIoCtl,
    .owner = THIS_MODULE,
}

// creates a device node in /dev, returns error if not made
int init_module(void) {
    int result, registers;
    dev_t devno;

    devno = MKDEV(MY_MAJOR, MY_MINOR);

    registers = register_chrdev_region(devno, 1, DEVICE_NAME);
    printk(KERN_INFO "Register chardev succeeded 1: %d\n", registers);
    cdev_init(&my_cdev, &fops);

}