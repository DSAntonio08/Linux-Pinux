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

static int get_binary(const char *file, uint8_t **out_buf, long *out_len);
static int chk_img_type(const uint8_t *buf, long len);
static int pad_img(uint8_t **to_pad, long len, long *out_padded);
static void put_le32(uint8_t *p, uint32_t v);
static uint32_t get_le32(const uint8_t *p);
static int write_binary(const char *dest, const uint8_t *buf, long len);

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
    long padded_file_len = 0;

    // &buf means adress of
    if (get_binary(argv[1], &buf, &len) != 0) {
        return 1;
    }
    if (chk_img_type(buf, len) != 0) {
        free(buf);
        return 1;
    }
    if (pad_img(&buf, len, &padded_file_len) != 0) {
        free(buf);
        return 1;
    }

    put_le32(buf + OFF_LEN, (uint32_t)padded_file_len);

    put_le32(buf + OFF_CHECKSUM, BROM_STAMP);
    uint32_t check_sum = 0;
    uint32_t wordcount = padded_file_len / 4;
    for (uint32_t i = 0; i < wordcount; i++) {
        check_sum += get_le32(buf + 4 * i);
    }

    put_le32(buf + OFF_CHECKSUM, check_sum);

    if (write_binary(argv[2], buf, padded_file_len) != 0) {
        free(buf);
        return 1;
    }

    // test for write_binary
    printf("%s: %ld -> %ld bytes, checksum 0x%08x\n", argv[2], len, padded_file_len, check_sum);

    free(buf); // memory management bc of no garbage collector
    return 0;
}

/* get_binary: reads the whole file into a new zero-initialised buffer.
   Returns 0 on success: the caller owns *out_buf and must free it, *out_len is the file size.
   Returns 1 on error, with everything already cleaned up. */
static int get_binary(const char *file, uint8_t **out_buf, long *out_len) {

    FILE *binary_file = fopen(file, "rb"); // r - read; b - binary
    if (!binary_file) {
        perror(file);
        return 1;
    }

    // get file size
    if (fseek(binary_file, 0, SEEK_END) != 0) {
        perror("fseek");
        fclose(binary_file);
        return 1;
    }
    long file_len = ftell(binary_file);
    if (file_len < 0) {
        perror("ftell");
        fclose(binary_file);
        return 1;
    }
    if (file_len == 0) {
        fprintf(stderr, "error: '%s' is empty\n", file);
        fclose(binary_file);
        return 1;
    }
    if (fseek(binary_file, 0, SEEK_SET) != 0) {
        perror("fseek");
        fclose(binary_file);
        return 1;
    }

    uint8_t *buffer = calloc(file_len, 1); // allocate Memory for the buffer
    if (!buffer) {
        fclose(binary_file);
        fprintf(stderr, "error: could not allocate memory\n");
        return 1;
    }

    size_t number_of_items_read = fread(buffer, 1, file_len, binary_file); // 1 - item_size
    if (number_of_items_read != (size_t)file_len) {
        if (ferror(binary_file)) {
            perror("fread");
        } else {
            fprintf(stderr, "fread: short read (%zu of %li bytes)\n", number_of_items_read, file_len);
        }
        free(buffer);
        fclose(binary_file);
        return 1;
    }

    fclose(binary_file);
    *out_buf = buffer;
    *out_len = file_len;
    return 0;
}

/* chk_img_type: checks that the image is 48 bytes to 32 KB and has "eGON.BT0" at offset 0x04.
   Returns 0 if valid, 1 otherwise. Does not modify the buffer. */
static int chk_img_type(const uint8_t *buf, long len) {
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

/* pad_img: grows *to_pad with zeros to the next multiple of 512 and stores the new size in *out_padded.
   The buffer may move, so the caller must use the updated *to_pad afterwards.
   Returns 1 on failure; the old buffer is then untouched and still owned by the caller. */
static int pad_img(uint8_t **to_pad, long len, long *out_padded) {
    long padded = (len + 511) / 512 * 512;
    uint8_t *tmp = realloc(*to_pad, padded);
    if (tmp == NULL) {
        fprintf(stderr, "error: could not allocate %li bytes for padded image\n", padded);
        return 1;
    }
    *to_pad = tmp;
    memset(*to_pad + len, 0, padded - len);

    *out_padded = padded;
    return 0;
}

/* put_le32: store v at p[0..3], lowest byte first (little endian), as the BROM header expects.
   Example: 0x00000400 -> 00 04 00 00. The caller guarantees 4 writable bytes at p. */
static void put_le32(uint8_t *p, uint32_t v) {
    for (int i = 0; i < 4; i++) {
        p[i] = (v >> (i * 8)) & 0xFF;
    }
}

/* get_le32: read p[0..3] as one 32-bit number, p[0] being the lowest byte (little endian).
   Inverse of put_le32. The caller guarantees 4 readable bytes at p. */
static uint32_t get_le32(const uint8_t *p) {
    uint32_t result = 0;
    for (int i = 0; i < 4; i++) {
        result |= ((uint32_t)p[i] << (i * 8));
    }
    return result;
}

/* write_binary: writes len bytes of buf to a new file at dest (an existing file is overwritten).
   Returns 0 on success. Returns 1 on error and removes the partial file. Does not free buf. */
static int write_binary(const char *dest, const uint8_t *buf, long len) {
    FILE *dest_file = fopen(dest, "wb");
    if (!dest_file) {
        perror(dest);
        return 1;
    }

    size_t written = fwrite(buf, 1, (size_t)len, dest_file);
    if (written != (size_t)len) {

        if (ferror(dest_file)) {
            perror("fwrite");
        } else {
            fprintf(stderr, "fwrite: short write (%zu of %li bytes)\n", written, len);
        }
        fclose(dest_file);
        remove(dest); // remove the half-written file
        return 1;
    }

    if (fclose(dest_file) != 0) {
        perror("fclose");
        remove(dest);
        return 1;
    }
    return 0;
}
