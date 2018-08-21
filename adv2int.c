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

static void Usage(void);

int main(int argc, char *argv[])
{
    char *infile = NULL;
    int debug = VMFALSE;
    int imageSize;
    ImageHdr *image;
    FILE *fp;
    int i;
    
    /* get the arguments */
    for(i = 1; i < argc; ++i) {

        /* handle switches */
        if(argv[i][0] == '-') {
            switch(argv[i][1]) {
            case 'd':   // enable debug mode
                debug = VMTRUE;
                break;
            default:
                Usage();
                break;
            }
        }

        /* handle the input filename */
        else {
            if (infile)
                Usage();
            infile = argv[i];
        }
    }
    
    if (!infile)
        Usage();
        
    if (!(fp = fopen(infile, "rb"))) {
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
    
    Execute(image, debug);
    
    free(image);
    
    return 0;
}
  
static void Usage(void)
{
    printf("usage: adv2int [ -d ] <file>\n");
    exit(1);
}
