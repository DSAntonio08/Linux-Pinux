/*
 * mkboot0 - turns my SPL .bin into an image the Boot-ROM (BROM) actually accepts from the uSD card
 *
 * Runs on the Mac, NOT on the board, so here I have the full libc (printf, fopen, malloc...)
 *
 * What it does:
 *   1. read the whole .bin into memory
 *   2. check it's really an eGON image ("eGON.BT0" at offset 0x04), so I don't patch random files
 *   3. pad it with 0s up to the next multiple of 512 bytes (the BROM wants that) 4. write the padded length into the header (offset 0x10)
 *   5. checksum: put 0x5F0A6C39 at 0x0C, add up ALL 32-bit words (little-endian, overflow is fine),
 *      then write that sum back into 0x0C
 *   6. write everything into a NEW file, the input stays untouched
 *
 * Usage: mkboot0 <input.bin> <output.bin>
 *
 */

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int getBinary(const char *file, uint8_t **out_buf, long *out_len);
int chkImgType(const uint8_t *buf, long len);
int padImg(uint8_t **pad, long len, long *out_padded);

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "usage: %s <input.bin> <output.bin>\n", argv[0]);
        return 1;
    }

    uint8_t *buf = NULL;
    long len = 0;

    if (getBinary(argv[1], &buf, &len) != 0) {
        return 1;
    }
    if (chkImgType(buf, len) != 0) {
        free(buf);
        return 1;
    }

    free(buf); // memory management bc of no garbage collector
    return 0;
}

int getBinary(const char *file, uint8_t **out_buf, long *out_len) {

    FILE *binaryFile = fopen(file, "rb"); // r - read; b - binary
    if (!binaryFile) {
        fprintf(stderr, "error: cannot open input file '%s'\n", file);
        return 1;
    }
    // get file size
    fseek(binaryFile, 0, SEEK_END);
    long fileLen = ftell(binaryFile);
    if (fileLen < 0) {
        perror("ftell");
        fclose(binaryFile);
        return 1;
    }
    fseek(binaryFile, 0, SEEK_SET); // Move back to the beginning of the file

    uint8_t *buffer = calloc(fileLen + 1, 1); // allocate Memory for the buffer
    if (!buffer) {
        fclose(binaryFile);
        fprintf(stderr, "error: could not allocate memory\n");
        return 1;
    }

    size_t numberOfItemsRead = fread(buffer, 1, fileLen, binaryFile); // 1 - itemSize
    if (numberOfItemsRead != (size_t)fileLen) {
        if (ferror(binaryFile)) {
            perror("fread");
        } else {
            fprintf(stderr, "fread: short read (%zu of %ld bytes)\n", numberOfItemsRead, fileLen);
        }
        free(buffer);
        fclose(binaryFile);
        return 1;
    }

    fclose(binaryFile);
    *out_buf = buffer;
    *out_len = fileLen;
    return 0;
}

int chkImgType(const uint8_t *buf, long len) {
    if (len < 48) {
        fprintf(stderr, "error: file too small (%ld bytes, eGON header needs 48)\n", len);
        return 1;
    }

    if (len > 32 * 1024) {
        fprintf(stderr, "error: image too big for SRAM A1\n");
        return 1;
    }

    if (memcmp(buf + 4, "eGON.BT0", 8) != 0) {
        fprintf(stderr, "error: not an eGON image (magic \"eGON.BT0\" not found at offset 0x04)\n");
        return 1;
    }

    return 0;
}

int padImg(uint8_t **pad, long len, long *out_padded){
    
    return 0;
}