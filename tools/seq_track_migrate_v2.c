#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core/seq/seq_project.h"

#define PATTERN_BLOB_MAGIC 0x42504154U

typedef struct __attribute__((packed)) {
    uint32_t magic;
    uint16_t version;
    uint8_t track_count;
    uint8_t reserved;
} pattern_blob_header_t;

typedef struct __attribute__((packed)) {
    uint32_t cart_id;
    uint32_t payload_size;
    uint8_t slot_id;
    uint8_t flags;
    uint16_t capabilities;
} track_payload_header_t;

static int migrate(const uint8_t *input, size_t input_len, FILE *output) {
    if (input_len < sizeof(pattern_blob_header_t)) {
        fprintf(stderr, "error: input too small (%zu)\n", input_len);
        return -1;
    }

    pattern_blob_header_t header;
    memcpy(&header, input, sizeof(header));
    if (header.magic != PATTERN_BLOB_MAGIC) {
        fprintf(stderr, "error: invalid pattern magic\n");
        return -1;
    }
    if ((header.version != 1U) && (header.version != 2U)) {
        fprintf(stderr, "error: unsupported source version %u\n", header.version);
        return -1;
    }

    const uint8_t *cursor = input + sizeof(header);
    size_t remaining = input_len - sizeof(header);

    pattern_blob_header_t new_header = header;
    new_header.version = SEQ_PROJECT_PATTERN_VERSION;

    if (fwrite(&new_header, sizeof(new_header), 1U, output) != 1U) {
        perror("fwrite");
        return -1;
    }

    for (uint8_t track = 0U; track < header.track_count; ++track) {
        if (remaining < sizeof(track_payload_header_t)) {
            fprintf(stderr, "error: truncated track header\n");
            return -1;
        }
        track_payload_header_t track_header;
        memcpy(&track_header, cursor, sizeof(track_header));
        cursor += sizeof(track_header);
        remaining -= sizeof(track_header);

        if (track_header.payload_size > remaining) {
            fprintf(stderr, "error: truncated payload for track %u\n", track);
            return -1;
        }

        const uint8_t *payload = cursor;
        const size_t payload_size = track_header.payload_size;
        cursor += payload_size;
        remaining -= payload_size;

        seq_model_track_t track;
        if (!seq_project_track_steps_decode(&track, payload, payload_size, header.version,
                                              SEQ_PROJECT_TRACK_DECODE_FULL)) {
            fprintf(stderr, "error: decode failed for track %u\n", track);
            return -1;
        }

        uint8_t encoded[SEQ_PROJECT_PATTERN_STORAGE_MAX];
        size_t written = 0U;
        if (!seq_project_track_steps_encode(&track, encoded, sizeof(encoded), &written)) {
            fprintf(stderr, "error: encode failed for track %u\n", track);
            return -1;
        }

        track_header.payload_size = (uint32_t)written;
        if (fwrite(&track_header, sizeof(track_header), 1U, output) != 1U) {
            perror("fwrite");
            return -1;
        }
        if (fwrite(encoded, written, 1U, output) != 1U) {
            perror("fwrite");
            return -1;
        }
    }

    return 0;
}

int main(int argc, char **argv) {
    if (argc != 3) {
        fprintf(stderr, "usage: %s <input_blob> <output_blob>\n", argv[0]);
        return 1;
    }

    const char *input_path = argv[1];
    const char *output_path = argv[2];

    FILE *input = fopen(input_path, "rb");
    if (input == NULL) {
        perror("fopen(input)");
        return 1;
    }
    if (fseek(input, 0, SEEK_END) != 0) {
        perror("fseek");
        fclose(input);
        return 1;
    }
    long size = ftell(input);
    if (size < 0) {
        perror("ftell");
        fclose(input);
        return 1;
    }
    rewind(input);

    uint8_t *buffer = (uint8_t *)malloc((size_t)size);
    if (buffer == NULL) {
        perror("malloc");
        fclose(input);
        return 1;
    }

    if (fread(buffer, (size_t)size, 1U, input) != 1U) {
        perror("fread");
        free(buffer);
        fclose(input);
        return 1;
    }
    fclose(input);

    FILE *output = fopen(output_path, "wb");
    if (output == NULL) {
        perror("fopen(output)");
        free(buffer);
        return 1;
    }

    int rc = migrate(buffer, (size_t)size, output);
    free(buffer);
    fclose(output);
    return (rc == 0) ? 0 : 1;
}
