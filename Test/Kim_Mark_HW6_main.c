/**************************************************************
* Class:  CSC-415-02 Fall 2022
* Name: Mark Kim
* Student ID: 918204214
* GitHub ID: mkim797
* Project: Assignment 6 – Device Driver
*
* File: encTest.c
*
* Description: This is the test application that will test the encryptor driver.
*
**************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <string.h>

#define ENCRYPT _IO('e', 0)
#define DECRYPT _IO('e', 1)
#define SETKEY _IO('e', 2)

#define MAX_CHARS 512

int main(int argc, char *argv[]) {
  int key, fd;
  char *text = malloc(MAX_CHARS);
  char *res = malloc(MAX_CHARS);
  char again[1];

  while (1) {
    printf("Opening the encryption device...\n");

    fd = open("/dev/encryptor", O_RDWR);
    if (fd < 0) {
      perror("open: failed to open device");
      return fd;
    }

    printf("Enter a key for encryption (0-50): ");
    scanf("%d", &key);
    getc(stdin);

    // ioctl to set the key for the device driver
    ioctl(fd, SETKEY, key);

    printf("Enter some text to be encrypted (maximum 512 characters):\n");
    scanf("%[^\n]s", text);
    getc(stdin);

    printf("Length: %ld\n", strlen(text));

    printf("This is the text you are encrypting:\n%s\n", text);

    write(fd, text, strlen(text));
    read(fd, res, strlen(text));

    printf("\nHere is your encrypted text:\n%s\n\n", res);
    printf("Decrypting the text...\n\n");

    // ioctl to decrypt the data on the driver (the kernel buffer)
    ioctl(fd, DECRYPT);

    read(fd, res, strlen(text));

    printf("Here is the decrypted text again: \n%s\n\n", res);

    printf("Re-encrypting the text for security...\n\n");

    // re-encrypt the data on the driver before closing
    ioctl(fd, ENCRYPT);

    printf("Closing the encryption device...\n");

    close(fd);

    printf("Would you like to try again (y/n)? ");
    scanf("%1s", again);

    if (again[0] != 'y') {
      break;
    }

    for (int i = 0; i < MAX_CHARS; i++) {
      text[i] = '\0';
      res[i] = '\0';
    }
  }

  printf("\nThank you for testing my character device. Bye!\n");

}