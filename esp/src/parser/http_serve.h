#ifndef HTTP_SERVE_H
#define HTTP_SERVE_H
#include <stdbool.h>
#include <stdint.h>

// ======================================
// header file for nmfu parser http_serve
// ======================================

// enum for output content_type
enum http_serve_out_content_type {
    HTTP_SERVE_CONTENT_TYPE_JSON,
    HTTP_SERVE_CONTENT_TYPE_PLAIN,
    HTTP_SERVE_CONTENT_TYPE_MULTIPART_FILE,
    HTTP_SERVE_CONTENT_TYPE_OTHER,
};
typedef enum http_serve_out_content_type http_serve_out_content_type_t;

// enum for output method
enum http_serve_out_method {
    HTTP_SERVE_METHOD_GET,
    HTTP_SERVE_METHOD_POST,
    HTTP_SERVE_METHOD_PUT,
    HTTP_SERVE_METHOD_DELETE,
};
typedef enum http_serve_out_method http_serve_out_method_t;

// enum for output error_code
enum http_serve_out_error_code {
    HTTP_SERVE_ERROR_CODE_OK,
    HTTP_SERVE_ERROR_CODE_BAD_METHOD,
    HTTP_SERVE_ERROR_CODE_URL_TOO_LONG,
    HTTP_SERVE_ERROR_CODE_AUTH_TOO_LONG,
    HTTP_SERVE_ERROR_CODE_UNSUPPORTED_VERSION,
    HTTP_SERVE_ERROR_CODE_BAD_REQUEST,
    HTTP_SERVE_ERROR_CODE_UNSUPPORTED_ENCODING,
};
typedef enum http_serve_out_error_code http_serve_out_error_code_t;

// enum for output auth_type
enum http_serve_out_auth_type {
    HTTP_SERVE_AUTH_TYPE_NO_AUTH,
    HTTP_SERVE_AUTH_TYPE_INVALID_TYPE,
    HTTP_SERVE_AUTH_TYPE_OK,
};
typedef enum http_serve_out_auth_type http_serve_out_auth_type_t;

// state object
struct http_serve_state {
    struct {
        char url[40];
        char etag[16];
        bool is_conditional;
        bool is_gzipable;
        bool is_chunked;
        http_serve_out_content_type_t content_type;
        http_serve_out_method_t method;
        http_serve_out_error_code_t error_code;
        http_serve_out_auth_type_t auth_type;
        char auth_string[52];
        char multipart_boundary[71];
        int32_t content_length;
    } c;
    uint8_t url_counter;
    uint8_t etag_counter;
    uint8_t auth_string_counter;
    uint8_t multipart_boundary_counter;
    uint16_t state;
};
typedef struct http_serve_state http_serve_state_t;

enum http_serve_result {
    HTTP_SERVE_OK,
    HTTP_SERVE_FAIL,
    HTTP_SERVE_DONE,
};
typedef enum http_serve_result http_serve_result_t;

http_serve_result_t http_serve_start(http_serve_state_t *state);
http_serve_result_t http_serve_feed(uint8_t inval, bool is_end, http_serve_state_t *state);
#endif
