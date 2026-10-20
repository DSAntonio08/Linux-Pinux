/*
 * mkboot0 - turns my SPL .bin into an image the Boot-ROM (BROM) actually accepts from the uSD card
 *
 * Runs on the Mac, NOT on the board, so here I have the full libc (printf, fopen, malloc...)
 *
 * What it does:
 *   1. read the whole .bin into memory
 *   2. check it's really an eGON image ("eGON.BT0" at offset 0x04), so I don't patch random files
 *   3. pad it with 0s up to the next multiple of 512 bytes (the BROM wants that)
 *   4. write the padded length into the header (offset 0x10)
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

enum { OFF_JMP = 0x00, OFF_MAGIC = 0x04, OFF_CHECKSUM = 0x0C, OFF_LEN = 0x10 };
#define BROM_STAMP 0x5F0A6C39u

int getBinary(const char *file, uint8_t **out_buf, long *out_len);
int chkImgType(const uint8_t *buf, long len);
int padImg(uint8_t **toPad, long len, long *out_padded);
void putLe32(uint8_t *p, uint32_t v);
uint32_t getLe32(const uint8_t *p);
int writeBinary(const char *dest, const uint8_t *buf, long len);

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "usage: %s <input.bin> <output.bin>\n", argv[0]);
        return 1;
    }

    if (strcmp(argv[1], argv[2]) == 0) {
        fprintf(stderr, "error: input and output must be different files\n");
        return 1;
    }

    uint8_t *buf = NULL;
    long len = 0;
    long paddedFilelen = 0;

    // &buf means adress of
    if (getBinary(argv[1], &buf, &len) != 0) {
        return 1;
    }
    if (chkImgType(buf, len) != 0) {
        free(buf);
        return 1;
    }
    if (padImg(&buf, len, &paddedFilelen) != 0) {
        free(buf);
        return 1;
    }

    putLe32(buf + OFF_LEN, (uint32_t)paddedFilelen);

    putLe32(buf + OFF_CHECKSUM, BROM_STAMP);
    uint32_t checkSum = 0;
    uint32_t wordcount = paddedFilelen / 4;
    for (uint32_t i = 0; i < wordcount; i++) {
        checkSum += getLe32(buf + 4 * i);
    }

    putLe32(buf + OFF_CHECKSUM, checkSum);

    if (writeBinary(argv[2], buf, paddedFilelen) != 0) {
        free(buf);
        return 1;
    }

    // test for writeBinary
    printf("%s: %ld -> %ld bytes, checksum 0x%08x\n", argv[2], len, paddedFilelen, checkSum);

    free(buf); // memory management bc of no garbage collector
    return 0;
}

/* getBinary: reads the whole file into a new zero-initialised buffer.
   Returns 0 on success: the caller owns *out_buf and must free it, *out_len is the file size.
   Returns 1 on error, with everything already cleaned up. */
int getBinary(const char *file, uint8_t **out_buf, long *out_len) {

    FILE *binaryFile = fopen(file, "rb"); // r - read; b - binary
    if (!binaryFile) {
        perror(file);
        return 1;
    }

    // get file size
    if (fseek(binaryFile, 0, SEEK_END) != 0) {
        perror("fseek");
        fclose(binaryFile);
        return 1;
    }
    long fileLen = ftell(binaryFile);
    if (fileLen < 0) {
        perror("ftell");
        fclose(binaryFile);
        return 1;
    }
    if (fileLen == 0) {
        fprintf(stderr, "error: '%s' is empty\n", file);
        fclose(binaryFile);
        return 1;
    }
    if (fseek(binaryFile, 0, SEEK_SET) != 0) {
        perror("fseek");
        fclose(binaryFile);
        return 1;
    }

    uint8_t *buffer = calloc(fileLen, 1); // allocate Memory for the buffer
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
            fprintf(stderr, "fread: short read (%zu of %li bytes)\n", numberOfItemsRead, fileLen);
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

/* chkImgType: checks that the image is 48 bytes to 32 KB and has "eGON.BT0" at offset 0x04.
   Returns 0 if valid, 1 otherwise. Does not modify the buffer. */
int chkImgType(const uint8_t *buf, long len) {
    if (len < 48) {
        fprintf(stderr, "error: file too small (%li bytes, eGON header needs 48)\n", len);
        return 1;
    }

    if (len > 32 * 1024) {
        fprintf(stderr, "error: image too big for SRAM A1\n");
        return 1;
    }

    if (memcmp(buf + OFF_MAGIC, "eGON.BT0", 8) != 0) {
        fprintf(stderr, "error: not an eGON image (magic \"eGON.BT0\" not found at offset 0x04)\n");
        return 1;
    }

    return 0;
}

/* padImg: grows *toPad with zeros to the next multiple of 512 and stores the new size in *out_padded.
   The buffer may move, so the caller must use the updated *toPad afterwards.
   Returns 1 on failure; the old buffer is then untouched and still owned by the caller. */
int padImg(uint8_t **toPad, long len, long *out_padded) {
    long padded = (len + 511) / 512 * 512;
    uint8_t *tmp = realloc(*toPad, padded);
    if (tmp == NULL) {
        fprintf(stderr, "error: could not allocate %li bytes for padded image\n", padded);
        return 1;
    }
    *toPad = tmp;
    memset(*toPad + len, 0, padded - len);

    *out_padded = padded;
    return 0;
}

/* putLe32: store v at p[0..3], lowest byte first (little endian), as the BROM header expects.
   Example: 0x00000400 -> 00 04 00 00. The caller guarantees 4 writable bytes at p. */
void putLe32(uint8_t *p, uint32_t v) {
    for (int i = 0; i < 4; i++) {
        p[i] = (v >> (i * 8)) & 0xFF;
    }
}

/* getLe32: read p[0..3] as one 32-bit number, p[0] being the lowest byte (little endian).
   Inverse of putLe32. The caller guarantees 4 readable bytes at p. */
uint32_t getLe32(const uint8_t *p) {
    uint32_t result = 0;
    for (int i = 0; i < 4; i++) {
        result |= ((uint32_t)p[i] << (i * 8));
    }
    return result;
}

/* writeBinary: writes len bytes of buf to a new file at dest (an existing file is overwritten).
   Returns 0 on success. Returns 1 on error and removes the partial file. Does not free buf. */
int writeBinary(const char *dest, const uint8_t *buf, long len) {
    FILE *destFile = fopen(dest, "wb");
    if (!destFile) {
        perror(dest);
        return 1;
    }

    size_t written = fwrite(buf, 1, (size_t)len, destFile);
    if (written != (size_t)len) {

        if (ferror(destFile)) {
            perror("fwrite");
        } else {
            fprintf(stderr, "fwrite: short write (%zu of %li bytes)\n", written, len);
        }
        fclose(destFile);
        remove(dest); // remove the half-written file
        return 1;
    }

    if (fclose(destFile) != 0) {
        perror("fclose");
        remove(dest);
        return 1;
    }
    return 0;
}
