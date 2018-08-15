/* adv2int.c - bytecode interpreter for a simple virtual machine
 *
 * Copyright (c) 2018 by David Michael Betz.  All rights reserved.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <setjmp.h>
#include <string.h>
#include <ctype.h>
#include "adv2vm.h"

int main(int argc, char *argv[])
{
    int imageSize;
    ImageHdr *image;
    FILE *fp;
    
    if (argc != 2) {
        printf("usage: adv2int <file>\n");
        return 1;
    }
    
    if (!(fp = fopen(argv[1], "rb"))) {
        printf("error: can't open '%s'\n", argv[1]);
        return 1;
    }
        
    fseek(fp, 0, SEEK_END);
    imageSize = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    
    if (!(image = (ImageHdr *)malloc(imageSize))) {
        printf("error: insufficient memory\n");
        return 1;
    }
    
    if (fread(image, 1, imageSize, fp) != imageSize) {
        printf("error: error reading image\n");
        return 1;
    }
    
    fclose(fp);
    
    Execute(image, VMFALSE);
    
    free(image);
    
    return 0;
}

