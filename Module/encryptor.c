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

#define ENCRYPT _IO('e', 0)
#define DECRYPT _IO('e', 1)
#define MY_MAJOR 415
#define MY_MINOR 0
#define DEVICE_NAME "encryptor"
#define BUFFER_SIZE 512

static ssize_t myRead(struct file * fs, char __user * buf, size_t hsize, loff_t * off);
static ssize_t myWrite(struct file * fs, const char __user * buf, size_t hsize, loff_t * off);
static int myOpen(struct inode * inode, struct file * fs);
static int myClose(struct inode * inode, struct file * fs);
static long myIoCtl (struct file * fs, unsigned int command, unsigned long data);
int encrypt(char *dest, const char *src, const int key);
int decrypt(char *dest, const char *src, const int key);

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
    char flag[1];  // 'e' is encrypted, 'd' is decrypted
} encds;

// this write function increments the data structure's count every time it is called.
// NOTE - data is not physically written anywhere.
// returns how many bytes were passed in.
// 0 is in, 1 is out, 2 is error, 3 is the first file handle
static ssize_t myWrite (struct file * fs, const char __user * buf, size_t hsize, loff_t * off) {
    int err, i;
    struct encds * ds;
    ds = (struct encds *) fs->private_data;

    err = copy_from_user(kernel_buffer, buf, hsize);

    if (err != 0) {
        printk(KERN_ERR "encrypt: copy_from_user failed: %d bytes failed to copy\n", err);
        return -1;
    }

    printk(KERN_INFO "myWrite: copy_from_user\n%s\n", buf);

    char *temp = vmalloc(strlen(kernel_buffer) + 1);

    if (encrypt(temp, kernel_buffer, ds->key) < 0) {
        return -1;
    }

    strcpy(kernel_buffer, temp);

    vfree(temp);
    temp = NULL;

    return hsize;
}

static ssize_t myRead (struct file * fs, char __user * buf, size_t hsize, loff_t * off) {
    int err, i;
    struct encds * ds;
    ds = (struct encds *) fs->private_data;
    
    err = copy_to_user(buf, kernel_buffer, hsize);

    printk(KERN_INFO "myRead: copy_to_user\n%s\n", kernel_buffer);
    
    if (err != 0) {
        printk(KERN_ERR "decrypt: copy_to_user failed: %d bytes failed to copy\n", err);
        return -1;
    }

    return hsize;
}

static int myOpen(struct inode * inode, struct file * fs) {
    struct encds * ds;
    ds = vmalloc(sizeof(struct encds));

    if (ds == 0) {
        printk(KERN_ERR "Cannot vmalloc, File not opened.\n");
        return -1;
    }

    ds->key = 0;
    fs->private_data = ds;
    return 0;
}

static int myClose(struct inode * inode, struct file * fs) {
    struct encds * ds;
    ds = (struct encds *) fs->private_data;
    vfree(ds);
    return 0;
}

int encrypt(char *dest, const char *src, const int key) {
    int i;

    if (src == NULL || dest == NULL) {
        printk(KERN_ERR "encrypt: encryption failed: source or destination NULL\n");
        return -1;
    }

    int src_len = strlen(src);
    
    if (src_len < 1) {
        printk(KERN_ERR "encrypt: encryption failed: source length invalid (< 1)\n");
        return -1;
    }

    for (i = 0; i < src_len; i++) {
        dest[i] = (src[i] + key) % 128;
    }

    return src_len;
}

int decrypt(char *dest, const char *src, const int key) {
    int i;

    if (src == NULL || dest == NULL) {
        printk(KERN_ERR "decrypt: decryption failed: source or destination NULL\n");
        return -1;
    }

    int src_len = strlen(src);
    
    if (src_len < 1) {
        printk(KERN_ERR "decrypt: decryption failed: source length invalid (< 1)\n");
        return -1;
    }

    for (i = 0; i < src_len; i++) {
        dest[i] = (src[i] - key) % 128;
    }

    return src_len;
}

// this is a way to deal with device files where there may not be read/write
// basically counts how many times "write" was called
static long myIoCtl (struct file * fs, unsigned int command, unsigned long data) {
    struct encds * ds;
    ds = (struct encds *) fs->private_data;

    char *temp = vmalloc(strlen(kernel_buffer) + 1);

    switch (command) {
        case ENCRYPT:
            printk(KERN_INFO "Encrypting...\n");
            encrypt(temp, kernel_buffer, ds->key);
            break;
        case DECRYPT:
            printk(KERN_INFO "Decrypting...\n");
            decrypt(temp, kernel_buffer, ds->key);
            break;
        default:
            printk(KERN_ERR "myIoCtl: invalid command %d\n", command);
    }

    strcpy(kernel_buffer, temp);

    vfree(temp);
    temp = NULL;

    return 0;
}

// creates a device node in /dev, returns error if not made
int init_module(void) {
    int result, registers;
    dev_t devno;

    devno = MKDEV(MY_MAJOR, MY_MINOR);

    registers = register_chrdev_region(devno, 1, DEVICE_NAME);
    printk(KERN_INFO "Register chardev succeeded 1: %d\n", registers);
    cdev_init(&my_cdev, &fops);

    result = cdev_add(&my_cdev, devno, 1);

    if (result < 0) {
        printk(KERN_ERR "init_module: add char device failed\n");
    }

    return result;
}

void cleanup_module(void) {
    dev_t devno;
    devno = MKDEV(MY_MAJOR, MY_MINOR);
    unregister_chrdev_region(devno, 1);
    cdev_del(&my_cdev);

    // unregister_chrdev(MY_MAJOR, DEVICE_NAME);

    vfree(kernel_buffer);
    kernel_buffer = NULL;

    printk(KERN_INFO "Goodbye from encryptor driver.\n");
}