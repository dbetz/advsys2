#include <stdlib.h>
#include <string.h>
#include "propbinary.h"

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
    uint16_t imagebase;
    uint32_t baudrate;
    uint8_t rxpin;
    uint8_t txpin;
} DatHdr;

/* target checksum for a binary file */
#define SPIN_TARGET_CHECKSUM    0x14

static void UpdateChecksum(uint8_t *binary, int size);

/* BuildBinary - build a .binary file from a template and a VM image */
uint8_t *BuildBinary(uint8_t *template, int templateSize, uint8_t *image, int imageSize, int *pBinarySize)
{
    int binarySize, paddedImageSize;
    uint8_t *binary;
    SpinHdr *hdr;
    SpinObj *obj;
    DatHdr *dat;
    
    /* pad the image to a long boundary */
    paddedImageSize = (imageSize + sizeof(uint32_t) - 1) & ~(sizeof(uint32_t) - 1); 
    
    /* compute the size of the binary file including the image */
    binarySize = templateSize + paddedImageSize;
    
    /* allocate space for the binary file */
    if (!(binary = (uint8_t *)malloc(binarySize)))
        return NULL;
    memset(binary, 0, binarySize);
    
    /* copy the template to the start of the file */
    memcpy(binary, template, templateSize);
    
    /* update the binary file header to include the VM image */
    hdr =  (SpinHdr *)binary;
    obj =  (SpinObj *)(binary + hdr->pbase);
    dat =  (DatHdr *)((uint8_t *)obj + (obj->pubcnt + obj->objcnt) * sizeof(uint32_t));
    dat->imagebase = hdr->vbase;
    memcpy(binary + hdr->vbase, image, imageSize);
    hdr->vbase += paddedImageSize;
    hdr->dbase += paddedImageSize;
    hdr->dcurr += paddedImageSize;
    UpdateChecksum(binary, binarySize);
    
    /* return the binary */
    *pBinarySize = binarySize;
    return binary;
}

/* DumpSpinBinary - dump a spin binary file */
void DumpSpinBinary(uint8_t *binary)
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
