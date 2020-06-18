#ifndef MULTIPART_HEADER_H
#define MULTIPART_HEADER_H
#include <stdbool.h>
#include <stdint.h>

// ============================================
// header file for nmfu parser multipart_header
// ============================================

// state object
struct multipart_header_state {
    struct {
        char name[32];
        char filename[32];
        int32_t content_length;
        bool ok;
        bool is_end;
    } c;
    uint8_t name_counter;
    uint8_t filename_counter;
    uint8_t state;
};
typedef struct multipart_header_state multipart_header_state_t;

enum multipart_header_result {
    MULTIPART_HEADER_OK,
    MULTIPART_HEADER_FAIL,
    MULTIPART_HEADER_DONE,
};
typedef enum multipart_header_result multipart_header_result_t;

multipart_header_result_t multipart_header_start(multipart_header_state_t *state);
multipart_header_result_t multipart_header_feed(uint8_t inval, bool is_end, multipart_header_state_t *state);
#endif
