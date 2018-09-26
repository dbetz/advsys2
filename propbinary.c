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

/* target checksum for a binary file */
#define SPIN_TARGET_CHECKSUM    0x14

/* spin binary file header structure */
typedef struct {
    uint32_t    clkfreq;
    uint8_t     clkmode;
    uint8_t     chksum;
    uint16_t    pbase;
    uint16_t    vbase;
    uint16_t    dbase;
    uint16_t    pcurr;
    uint16_t    dcurr;
} SpinHdr;

/* spin object */
typedef struct {
    uint16_t next;
    uint8_t pubcnt;
    uint8_t objcnt;
    uint16_t pubstart;
    uint16_t numlocals;
} SpinObj;

/* DAT header in serial_helper.spin */
typedef struct {
    uint32_t baudrate;
    uint8_t rxpin;
    uint8_t txpin;
    uint16_t imagebase;
} DatHdr;

extern uint8_t advsys2_run_template_array[];
extern int advsys2_run_template_size;
extern uint8_t advsys2_step_template_array[];
extern int advsys2_step_template_size;

static void Usage(void);
static void UpdateChecksum(uint8_t *binary, int size);
static uint8_t *ReadEntireFile(char *name, int *pSize);
static void DumpSpinBinary(uint8_t *binary);

/* main - the main program */
int main(int argc, char *argv[])
{
    uint8_t *template, *binary, *image;
    int templateSize, binarySize, imageSize, paddedImageSize, i;
    char outputFileBuf[100], *p;
    char *inputFile = NULL;
    char *outputFile = NULL;
    int debugMode = 0;
    SpinHdr *hdr;
    SpinObj *obj;
    DatHdr *dat;
    
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
    
    template = advsys2_run_template_array;
    templateSize = advsys2_run_template_size;
    
    image = ReadEntireFile(inputFile, &imageSize);
    paddedImageSize = (imageSize + sizeof(uint32_t) - 1) & ~(sizeof(uint32_t) - 1); 
    
    binarySize = templateSize + paddedImageSize;
    if (!(binary = (uint8_t *)malloc(binarySize))) {
        printf("error: insufficient memory\n");
        return 1;
    }
    
    memset(binary, 0, binarySize);
    memcpy(binary, template, templateSize);
    
    hdr =  (SpinHdr *)binary;
    obj =  (SpinObj *)(binary + hdr->pbase);
    dat =  (DatHdr *)((uint8_t *)obj + (obj->pubcnt + obj->objcnt) * sizeof(uint32_t));
    dat->imagebase = hdr->vbase;
    memcpy(binary + hdr->vbase, image, imageSize);
    hdr->vbase += paddedImageSize;
    hdr->dbase += paddedImageSize;
    hdr->dcurr += paddedImageSize;
    UpdateChecksum(binary, binarySize);
    
    {
        FILE *fp = fopen(outputFile, "wb");
        fwrite(binary, binarySize, 1, fp);
        fclose(fp);
    }
        
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
    printf("usage: propbinary [ -d ] [ -o <output-file> ] <input-file>\n");
    exit(1);
}

/* UpdateChecksum - recompute the checksum */
static void UpdateChecksum(uint8_t *binary, int size)
{
    SpinHdr *hdr = (SpinHdr *)binary;
    int chksum = 0;
    hdr->chksum = 0;
    while (--size >= 0)
        chksum += *binary++;
    hdr->chksum = SPIN_TARGET_CHECKSUM - chksum;
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

/* DumpSpinBinary - dump a spin binary file */
static void DumpSpinBinary(uint8_t *binary)
{
    SpinHdr *hdr = (SpinHdr *)binary;
    printf("clkfreq: %d\n",   hdr->clkfreq);
    printf("clkmode: %02x\n", hdr->clkmode);
    printf("chksum:  %02x\n", hdr->chksum);
    printf("pbase:   %04x\n", hdr->pbase);
    printf("vbase:   %04x\n", hdr->vbase);
    printf("dbase:   %04x\n", hdr->dbase);
    printf("pcurr:   %04x\n", hdr->pcurr);
    printf("dcurr:   %04x\n", hdr->dcurr);
}
