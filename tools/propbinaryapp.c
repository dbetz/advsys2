/*
    Or you could concatenate the compiled AdvSys2 code after the Spin binary,
    and then adjust the DBASE, DCURR and VBASE values in the Spin header. Each 
    of these three values would be increased by the size of the AdvSys2 code. 
    You would also need to adjust the checksum in the header.
*/

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "propbinary.h"

extern uint8_t advsys2_run_template_array[];
extern int advsys2_run_template_size;
extern uint8_t advsys2_step_template_array[];
extern int advsys2_step_template_size;
#ifdef WORDFIRE_SUPPORT
extern uint8_t wordfire_template_array[];
extern int wordfire_template_size;
#endif

static void Usage(void);
static uint8_t *ReadEntireFile(char *name, int *pSize);

/* main - the main program */
int main(int argc, char *argv[])
{
    uint8_t *template, *binary, *image;
    int templateSize, binarySize, imageSize, i;
    char outputFileBuf[100], *p;
    char *inputFile = NULL;
    char *outputFile = NULL;
    char *templateName = "run";
    int debugMode = 0;
    FILE *fp;
    
    /* get the arguments */
    for(i = 1; i < argc; ++i) {

        /* handle switches */
        if(argv[i][0] == '-') {
            switch(argv[i][1]) {
            case 'd':   // enable debug mode
                debugMode = 1;
                break;
            case 'o':
                if(argv[i][2])
                    outputFile = &argv[i][2];
                else if(++i < argc)
                    outputFile = argv[i];
                else
                    Usage();
                break;
            case 't':
                if(argv[i][2])
                    templateName = &argv[i][2];
                else if(++i < argc)
                    templateName = argv[i];
                else
                    Usage();
                break;
            default:
                Usage();
                break;
            }
        }

        /* handle the input filename */
        else {
            if (inputFile)
                Usage();
            inputFile = argv[i];
        }
    }
        
    if (!inputFile)
        Usage();
        
    if (!outputFile) {
        if (!(p = strrchr(inputFile, '.')))
            strcpy(outputFileBuf, inputFile);
        else {
            strncpy(outputFileBuf, inputFile, p - argv[1]);
            outputFileBuf[p - inputFile] = '\0';
        }
        strcat(outputFileBuf, ".binary");
        outputFile = outputFileBuf;
    }
    
    if (strcmp(templateName, "run") == 0) {
        template = advsys2_run_template_array;
        templateSize = advsys2_run_template_size;
    }
    else if (strcmp(templateName, "step") == 0) {
        template = advsys2_step_template_array;
        templateSize = advsys2_step_template_size;
    }
#ifdef WORDFIRE_SUPPORT
    else if (strcmp(templateName, "wordfire") == 0) {
        template = wordfire_template_array;
        templateSize = wordfire_template_size;
    }
#endif
    else {
        printf("error: unknown template name '%s'\n", templateName);
        return 1;
    }
    
    if (!(image = ReadEntireFile(inputFile, &imageSize))) {
        printf("error: reading image '%s'\n", inputFile);
        return 1;
    }
    
    if (!(binary = BuildBinary(template, templateSize, image, imageSize, &binarySize))) {
        printf("error: insufficient memory\n");
        return 1;
    }
    
    if (!(fp = fopen(outputFile, "wb"))) {
        printf("error: can't create '%s'\n", outputFile);
        return 1;
    }
    fwrite(binary, binarySize, 1, fp);
    fclose(fp);
        
    if (debugMode) {
        printf("\nTemplate:\n");
        DumpSpinBinary(template);
        printf("\nBinary:\n");
        DumpSpinBinary(binary);
    }
    
    return 0;
}

/* Usage - display a usage message and exit */
static void Usage(void)
{
#ifdef WORDFIRE_SUPPORT
    printf("\
usage: propbinary [ -d ] [ -o <output-file> ] [ -t <template-name> ] <input-file>\n\
       templates: run, step, wordfire\n");
#else
    printf("\
usage: propbinary [ -d ] [ -o <output-file> ] [ -t <template-name> ] <input-file>\n\
       templates: run, step\n");
#endif
    exit(1);
}

/* ReadEntireFile - read an entire file into an allocated buffer */
static uint8_t *ReadEntireFile(char *name, int *pSize)
{
    uint8_t *buf;
    int size;
    FILE *fp;

    /* open the file in binary mode */
    if (!(fp = fopen(name, "rb")))
        return NULL;

    /* get the file size */
    fseek(fp, 0L, SEEK_END);
    size = (int)ftell(fp);
    fseek(fp, 0L, SEEK_SET);
    
    /* allocate a buffer for the file contents */
    if (!(buf = (uint8_t *)malloc(size))) {
        fclose(fp);
        return NULL;
    }
    
    /* read the contents of the file into the buffer */
    if (fread(buf, 1, size, fp) != size) {
        free(buf);
        fclose(fp);
        return NULL;
    }
    
    /* close the file ad return the buffer containing the file contents */
    *pSize = size;
    fclose(fp);
    return buf;
}
