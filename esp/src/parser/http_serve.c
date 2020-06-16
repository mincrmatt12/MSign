// ======================================
// source file for nmfu parser http_serve
// ======================================
#include "http_serve.h"
#include <string.h>

http_serve_result_t http_serve_start(http_serve_state_t *state) {
    // initialize append counter for url
    state->url_counter = 0;
    // initialize append counter for etag
    state->etag_counter = 0;
    // initialize default for is_conditional
    state->c.is_conditional = false;
    // initialize default for is_gzipable
    state->c.is_gzipable = false;
    // initialize default for is_chunked
    state->c.is_chunked = false;
    // initialize append counter for auth_string
    state->auth_string_counter = 0;
    // initialize append counter for multipart_boundary
    state->multipart_boundary_counter = 0;
    // initialize default for content_length
    state->c.content_length = 0;
    // set starting state
    state->state = 0;
    // run start actions
    state->c.auth_type = HTTP_SERVE_AUTH_TYPE_NO_AUTH;
    return HTTP_SERVE_OK;
}
http_serve_result_t http_serve_feed(uint8_t inval, bool is_end, http_serve_state_t *state) {
    switch (state->state) {
    case 0:
        // transitions
        if ((inval == 68 /* 'D' */) && !is_end) {
            state->state = 1;
        }
        else if ((inval == 80 /* 'P' */) && !is_end) {
            state->state = 2;
        }
        else if ((inval == 71 /* 'G' */) && !is_end) {
            state->state = 3;
        }
        else {
            state->state = 16;
        }
        return HTTP_SERVE_OK;
    case 1:
        // transitions
        if ((inval == 69 /* 'E' */) && !is_end) {
            state->state = 4;
        }
        else {
            state->state = 16;
        }
        return HTTP_SERVE_OK;
    case 2:
        // transitions
        if ((inval == 85 /* 'U' */) && !is_end) {
            state->state = 5;
        }
        else if ((inval == 79 /* 'O' */) && !is_end) {
            state->state = 6;
        }
        else {
            state->state = 16;
        }
        return HTTP_SERVE_OK;
    case 3:
        // transitions
        if ((inval == 69 /* 'E' */) && !is_end) {
            state->state = 7;
        }
        else {
            state->state = 16;
        }
        return HTTP_SERVE_OK;
    case 4:
        // transitions
        if ((inval == 76 /* 'L' */) && !is_end) {
            state->state = 8;
        }
        else {
            state->state = 16;
        }
        return HTTP_SERVE_OK;
    case 5:
        // transitions
        if ((inval == 84 /* 'T' */) && !is_end) {
            state->state = 9;

            // action <nmfu.SetTo object at 0x7f63588b21d0> 
            state->c.method = HTTP_SERVE_METHOD_PUT;
        }
        else {
            state->state = 16;
        }
        return HTTP_SERVE_OK;
    case 6:
        // transitions
        if ((inval == 83 /* 'S' */) && !is_end) {
            state->state = 10;
        }
        else {
            state->state = 16;
        }
        return HTTP_SERVE_OK;
    case 7:
        // transitions
        if ((inval == 84 /* 'T' */) && !is_end) {
            state->state = 11;

            // action <nmfu.SetTo object at 0x7f63576b22b0> 
            state->c.method = HTTP_SERVE_METHOD_GET;
        }
        else {
            state->state = 16;
        }
        return HTTP_SERVE_OK;
    case 8:
        // transitions
        if ((inval == 69 /* 'E' */) && !is_end) {
            state->state = 12;
        }
        else {
            state->state = 16;
        }
        return HTTP_SERVE_OK;
    case 9:
        // transitions
        if ((inval == 32 /* ' ' */) && !is_end) {
            state->state = 271;
        }
        else {
            state->state = 272;
        }
        return HTTP_SERVE_OK;
    case 10:
        // transitions
        if ((inval == 84 /* 'T' */) && !is_end) {
            state->state = 13;

            // action <nmfu.SetTo object at 0x7f63576b28d0> 
            state->c.method = HTTP_SERVE_METHOD_POST;
        }
        else {
            state->state = 16;
        }
        return HTTP_SERVE_OK;
    case 11:
        // transitions
        if ((inval == 32 /* ' ' */) && !is_end) {
            state->state = 271;
        }
        else {
            state->state = 272;
        }
        return HTTP_SERVE_OK;
    case 12:
        // transitions
        if ((inval == 84 /* 'T' */) && !is_end) {
            state->state = 14;
        }
        else {
            state->state = 16;
        }
        return HTTP_SERVE_OK;
    case 13:
        // transitions
        if ((inval == 32 /* ' ' */) && !is_end) {
            state->state = 271;
        }
        else {
            state->state = 272;
        }
        return HTTP_SERVE_OK;
    case 14:
        // transitions
        if ((inval == 69 /* 'E' */) && !is_end) {
            state->state = 15;

            // action <nmfu.SetTo object at 0x7f635781a0b8> 
            state->c.method = HTTP_SERVE_METHOD_DELETE;
        }
        else {
            state->state = 16;
        }
        return HTTP_SERVE_OK;
    case 15:
        // transitions
        if ((inval == 32 /* ' ' */) && !is_end) {
            state->state = 271;
        }
        else {
            state->state = 272;
        }
        return HTTP_SERVE_OK;
    case 16:
        // transitions
        if ((inval == 13 /* '\r' */) && !is_end) {
            state->state = 17;
        }
        else {
            state->state = 16;
        }
        return HTTP_SERVE_OK;
    case 17:
        // transitions
        if ((inval == 10 /* '\n' */) && !is_end) {
            state->state = 18;
        }
        else {
            state->state = 16;
        }
        return HTTP_SERVE_OK;
    case 18:
        // transitions
        if ((inval == 13 /* '\r' */) && !is_end) {
            state->state = 19;
        }
        else {
            state->state = 16;
        }
        return HTTP_SERVE_OK;
    case 19:
        // transitions
        if ((inval == 10 /* '\n' */) && !is_end) {
            // terminating state

            // action <nmfu.SetTo object at 0x7f6357812ba8> 
            state->c.error_code = HTTP_SERVE_ERROR_CODE_BAD_METHOD;

            // action <nmfu.FinishAction object at 0x7f6357812320> 
            return HTTP_SERVE_DONE;
        }
        else {
            state->state = 16;
        }
        return HTTP_SERVE_OK;
    case 20:
        // transitions
        if (((45 <= inval && inval <= 57 /* '-' - '9' */) || (65 <= inval && inval <= 90 /* 'A' - 'Z' */) || (97 <= inval && inval <= 122 /* 'a' - 'z' */) || inval == 35 /* '#' */ || inval == 38 /* '&' */ || inval == 63 /* '?' */ || inval == 95 /* '_' */) && !is_end) {
            state->state = 20;

            // action <nmfu.AppendTo object at 0x7f635761b828> 
            if (state->url_counter == 39) state->state = 21;
            else {
                state->c.url[state->url_counter++] = (char)(inval);
                state->c.url[state->url_counter] = 0;
            }
        }
        else if ((inval == 32 /* ' ' */) && !is_end) {
            state->state = 269;
        }
        else {
            state->state = 272;
        }
        return HTTP_SERVE_OK;
    case 21:
        // transitions
        if ((inval == 13 /* '\r' */) && !is_end) {
            state->state = 24;
        }
        else {
            state->state = 25;
        }
        return HTTP_SERVE_OK;
    case 22:
        // transitions
        if ((inval == 10 /* '\n' */) && !is_end) {
            // terminating state

            // action <nmfu.SetTo object at 0x7f635761b438> 
            state->c.error_code = HTTP_SERVE_ERROR_CODE_URL_TOO_LONG;

            // action <nmfu.FinishAction object at 0x7f635761b7f0> 
            return HTTP_SERVE_DONE;
        }
        else {
            state->state = 25;
        }
        return HTTP_SERVE_OK;
    case 23:
        // transitions
        if ((inval == 13 /* '\r' */) && !is_end) {
            state->state = 22;
        }
        else {
            state->state = 25;
        }
        return HTTP_SERVE_OK;
    case 24:
        // transitions
        if ((inval == 10 /* '\n' */) && !is_end) {
            state->state = 23;
        }
        else {
            state->state = 25;
        }
        return HTTP_SERVE_OK;
    case 25:
        // transitions
        if ((inval == 13 /* '\r' */) && !is_end) {
            state->state = 24;
        }
        else {
            state->state = 25;
        }
        return HTTP_SERVE_OK;
    case 26:
        // transitions
        if ((inval == 46 /* '.' */) && !is_end) {
            state->state = 27;
        }
        else {
            state->state = 30;
        }
        return HTTP_SERVE_OK;
    case 27:
        // transitions
        if ((inval == 49 /* '1' */) && !is_end) {
            state->state = 28;
        }
        else if ((inval == 48 /* '0' */) && !is_end) {
            state->state = 29;
        }
        else {
            state->state = 30;
        }
        return HTTP_SERVE_OK;
    case 28:
        // transitions
        if ((inval == 13 /* '\r' */) && !is_end) {
            state->state = 263;
        }
        else {
            state->state = 272;
        }
        return HTTP_SERVE_OK;
    case 29:
        // transitions
        if ((inval == 13 /* '\r' */) && !is_end) {
            state->state = 263;
        }
        else {
            state->state = 272;
        }
        return HTTP_SERVE_OK;
    case 30:
        // transitions
        if ((inval == 13 /* '\r' */) && !is_end) {
            state->state = 31;
        }
        else {
            state->state = 30;
        }
        return HTTP_SERVE_OK;
    case 31:
        // transitions
        if ((inval == 10 /* '\n' */) && !is_end) {
            state->state = 32;
        }
        else {
            state->state = 30;
        }
        return HTTP_SERVE_OK;
    case 32:
        // transitions
        if ((inval == 13 /* '\r' */) && !is_end) {
            state->state = 33;
        }
        else {
            state->state = 30;
        }
        return HTTP_SERVE_OK;
    case 33:
        // transitions
        if ((inval == 10 /* '\n' */) && !is_end) {
            // terminating state

            // action <nmfu.SetTo object at 0x7f635761b630> 
            state->c.error_code = HTTP_SERVE_ERROR_CODE_UNSUPPORTED_VERSION;

            // action <nmfu.FinishAction object at 0x7f635761b6d8> 
            return HTTP_SERVE_DONE;
        }
        else {
            state->state = 30;
        }
        return HTTP_SERVE_OK;
    case 34:
        // transitions
        if ((inval == 99 /* 'c' */ || inval == 67 /* 'C' */) && !is_end) {
            state->state = 35;
        }
        else if ((inval == 13 /* '\r' */) && !is_end) {
            state->state = 36;
        }
        else if ((inval == 116 /* 't' */ || inval == 84 /* 'T' */) && !is_end) {
            state->state = 37;
        }
        else if ((inval == 65 /* 'A' */ || inval == 97 /* 'a' */) && !is_end) {
            state->state = 38;
        }
        else if ((inval == 105 /* 'i' */ || inval == 73 /* 'I' */) && !is_end) {
            state->state = 39;
        }
        else {
            state->state = 117;
        }
        return HTTP_SERVE_OK;
    case 35:
        // transitions
        if ((inval == 111 /* 'o' */ || inval == 79 /* 'O' */) && !is_end) {
            state->state = 40;
        }
        else {
            state->state = 117;
        }
        return HTTP_SERVE_OK;
    case 36:
        // transitions
        if ((inval == 10 /* '\n' */) && !is_end) {
            // terminating state

            // action <nmfu.SetTo object at 0x7f6357b45198> 
            state->c.error_code = HTTP_SERVE_ERROR_CODE_OK;

            // action <nmfu.FinishAction object at 0x7f6357b450f0> 
            return HTTP_SERVE_DONE;
        }
        else {
            state->state = 117;
        }
        return HTTP_SERVE_OK;
    case 37:
        // transitions
        if ((inval == 114 /* 'r' */ || inval == 82 /* 'R' */) && !is_end) {
            state->state = 41;
        }
        else {
            state->state = 117;
        }
        return HTTP_SERVE_OK;
    case 38:
        // transitions
        if ((inval == 99 /* 'c' */ || inval == 67 /* 'C' */) && !is_end) {
            state->state = 42;
        }
        else if ((inval == 85 /* 'U' */ || inval == 117 /* 'u' */) && !is_end) {
            state->state = 43;
        }
        else {
            state->state = 117;
        }
        return HTTP_SERVE_OK;
    case 39:
        // transitions
        if ((inval == 70 /* 'F' */ || inval == 102 /* 'f' */) && !is_end) {
            state->state = 44;
        }
        else {
            state->state = 117;
        }
        return HTTP_SERVE_OK;
    case 40:
        // transitions
        if ((inval == 78 /* 'N' */ || inval == 110 /* 'n' */) && !is_end) {
            state->state = 45;
        }
        else {
            state->state = 117;
        }
        return HTTP_SERVE_OK;
    case 41:
        // transitions
        if ((inval == 65 /* 'A' */ || inval == 97 /* 'a' */) && !is_end) {
            state->state = 46;
        }
        else {
            state->state = 117;
        }
        return HTTP_SERVE_OK;
    case 42:
        // transitions
        if ((inval == 99 /* 'c' */ || inval == 67 /* 'C' */) && !is_end) {
            state->state = 47;
        }
        else {
            state->state = 117;
        }
        return HTTP_SERVE_OK;
    case 43:
        // transitions
        if ((inval == 116 /* 't' */ || inval == 84 /* 'T' */) && !is_end) {
            state->state = 48;
        }
        else {
            state->state = 117;
        }
        return HTTP_SERVE_OK;
    case 44:
        // transitions
        if ((inval == 45 /* '-' */) && !is_end) {
            state->state = 49;
        }
        else {
            state->state = 117;
        }
        return HTTP_SERVE_OK;
    case 45:
        // transitions
        if ((inval == 116 /* 't' */ || inval == 84 /* 'T' */) && !is_end) {
            state->state = 50;
        }
        else {
            state->state = 117;
        }
        return HTTP_SERVE_OK;
    case 46:
        // transitions
        if ((inval == 78 /* 'N' */ || inval == 110 /* 'n' */) && !is_end) {
            state->state = 51;
        }
        else {
            state->state = 117;
        }
        return HTTP_SERVE_OK;
    case 47:
        // transitions
        if ((inval == 69 /* 'E' */ || inval == 101 /* 'e' */) && !is_end) {
            state->state = 52;
        }
        else {
            state->state = 117;
        }
        return HTTP_SERVE_OK;
    case 48:
        // transitions
        if ((inval == 72 /* 'H' */ || inval == 104 /* 'h' */) && !is_end) {
            state->state = 53;
        }
        else {
            state->state = 117;
        }
        return HTTP_SERVE_OK;
    case 49:
        // transitions
        if ((inval == 78 /* 'N' */ || inval == 110 /* 'n' */) && !is_end) {
            state->state = 54;
        }
        else {
            state->state = 117;
        }
        return HTTP_SERVE_OK;
    case 50:
        // transitions
        if ((inval == 69 /* 'E' */ || inval == 101 /* 'e' */) && !is_end) {
            state->state = 55;
        }
        else {
            state->state = 117;
        }
        return HTTP_SERVE_OK;
    case 51:
        // transitions
        if ((inval == 115 /* 's' */ || inval == 83 /* 'S' */) && !is_end) {
            state->state = 56;
        }
        else {
            state->state = 117;
        }
        return HTTP_SERVE_OK;
    case 52:
        // transitions
        if ((inval == 80 /* 'P' */ || inval == 112 /* 'p' */) && !is_end) {
            state->state = 57;
        }
        else {
            state->state = 117;
        }
        return HTTP_SERVE_OK;
    case 53:
        // transitions
        if ((inval == 111 /* 'o' */ || inval == 79 /* 'O' */) && !is_end) {
            state->state = 58;
        }
        else {
            state->state = 117;
        }
        return HTTP_SERVE_OK;
    case 54:
        // transitions
        if ((inval == 111 /* 'o' */ || inval == 79 /* 'O' */) && !is_end) {
            state->state = 59;
        }
        else {
            state->state = 117;
        }
        return HTTP_SERVE_OK;
    case 55:
        // transitions
        if ((inval == 78 /* 'N' */ || inval == 110 /* 'n' */) && !is_end) {
            state->state = 60;
        }
        else {
            state->state = 117;
        }
        return HTTP_SERVE_OK;
    case 56:
        // transitions
        if ((inval == 70 /* 'F' */ || inval == 102 /* 'f' */) && !is_end) {
            state->state = 61;
        }
        else {
            state->state = 117;
        }
        return HTTP_SERVE_OK;
    case 57:
        // transitions
        if ((inval == 116 /* 't' */ || inval == 84 /* 'T' */) && !is_end) {
            state->state = 62;
        }
        else {
            state->state = 117;
        }
        return HTTP_SERVE_OK;
    case 58:
        // transitions
        if ((inval == 114 /* 'r' */ || inval == 82 /* 'R' */) && !is_end) {
            state->state = 63;
        }
        else {
            state->state = 117;
        }
        return HTTP_SERVE_OK;
    case 59:
        // transitions
        if ((inval == 78 /* 'N' */ || inval == 110 /* 'n' */) && !is_end) {
            state->state = 64;
        }
        else {
            state->state = 117;
        }
        return HTTP_SERVE_OK;
    case 60:
        // transitions
        if ((inval == 116 /* 't' */ || inval == 84 /* 'T' */) && !is_end) {
            state->state = 65;
        }
        else {
            state->state = 117;
        }
        return HTTP_SERVE_OK;
    case 61:
        // transitions
        if ((inval == 69 /* 'E' */ || inval == 101 /* 'e' */) && !is_end) {
            state->state = 66;
        }
        else {
            state->state = 117;
        }
        return HTTP_SERVE_OK;
    case 62:
        // transitions
        if ((inval == 45 /* '-' */) && !is_end) {
            state->state = 67;
        }
        else {
            state->state = 117;
        }
        return HTTP_SERVE_OK;
    case 63:
        // transitions
        if ((inval == 105 /* 'i' */ || inval == 73 /* 'I' */) && !is_end) {
            state->state = 68;
        }
        else {
            state->state = 117;
        }
        return HTTP_SERVE_OK;
    case 64:
        // transitions
        if ((inval == 69 /* 'E' */ || inval == 101 /* 'e' */) && !is_end) {
            state->state = 69;
        }
        else {
            state->state = 117;
        }
        return HTTP_SERVE_OK;
    case 65:
        // transitions
        if ((inval == 45 /* '-' */) && !is_end) {
            state->state = 70;
        }
        else {
            state->state = 117;
        }
        return HTTP_SERVE_OK;
    case 66:
        // transitions
        if ((inval == 114 /* 'r' */ || inval == 82 /* 'R' */) && !is_end) {
            state->state = 71;
        }
        else {
            state->state = 117;
        }
        return HTTP_SERVE_OK;
    case 67:
        // transitions
        if ((inval == 101 /* 'e' */ || inval == 69 /* 'E' */) && !is_end) {
            state->state = 72;
        }
        else {
            state->state = 117;
        }
        return HTTP_SERVE_OK;
    case 68:
        // transitions
        if ((inval == 122 /* 'z' */ || inval == 90 /* 'Z' */) && !is_end) {
            state->state = 73;
        }
        else {
            state->state = 117;
        }
        return HTTP_SERVE_OK;
    case 69:
        // transitions
        if ((inval == 45 /* '-' */) && !is_end) {
            state->state = 74;
        }
        else {
            state->state = 117;
        }
        return HTTP_SERVE_OK;
    case 70:
        // transitions
        if ((inval == 116 /* 't' */ || inval == 84 /* 'T' */) && !is_end) {
            state->state = 75;
        }
        else if ((inval == 76 /* 'L' */ || inval == 108 /* 'l' */) && !is_end) {
            state->state = 76;
        }
        else {
            state->state = 117;
        }
        return HTTP_SERVE_OK;
    case 71:
        // transitions
        if ((inval == 45 /* '-' */) && !is_end) {
            state->state = 77;
        }
        else {
            state->state = 117;
        }
        return HTTP_SERVE_OK;
    case 72:
        // transitions
        if ((inval == 78 /* 'N' */ || inval == 110 /* 'n' */) && !is_end) {
            state->state = 78;
        }
        else {
            state->state = 117;
        }
        return HTTP_SERVE_OK;
    case 73:
        // transitions
        if ((inval == 65 /* 'A' */ || inval == 97 /* 'a' */) && !is_end) {
            state->state = 79;
        }
        else {
            state->state = 117;
        }
        return HTTP_SERVE_OK;
    case 74:
        // transitions
        if ((inval == 109 /* 'm' */ || inval == 77 /* 'M' */) && !is_end) {
            state->state = 80;
        }
        else {
            state->state = 117;
        }
        return HTTP_SERVE_OK;
    case 75:
        // transitions
        if ((inval == 89 /* 'Y' */ || inval == 121 /* 'y' */) && !is_end) {
            state->state = 81;
        }
        else {
            state->state = 117;
        }
        return HTTP_SERVE_OK;
    case 76:
        // transitions
        if ((inval == 69 /* 'E' */ || inval == 101 /* 'e' */) && !is_end) {
            state->state = 82;
        }
        else {
            state->state = 117;
        }
        return HTTP_SERVE_OK;
    case 77:
        // transitions
        if ((inval == 101 /* 'e' */ || inval == 69 /* 'E' */) && !is_end) {
            state->state = 83;
        }
        else {
            state->state = 117;
        }
        return HTTP_SERVE_OK;
    case 78:
        // transitions
        if ((inval == 99 /* 'c' */ || inval == 67 /* 'C' */) && !is_end) {
            state->state = 84;
        }
        else {
            state->state = 117;
        }
        return HTTP_SERVE_OK;
    case 79:
        // transitions
        if ((inval == 116 /* 't' */ || inval == 84 /* 'T' */) && !is_end) {
            state->state = 85;
        }
        else {
            state->state = 117;
        }
        return HTTP_SERVE_OK;
    case 80:
        // transitions
        if ((inval == 65 /* 'A' */ || inval == 97 /* 'a' */) && !is_end) {
            state->state = 86;
        }
        else {
            state->state = 117;
        }
        return HTTP_SERVE_OK;
    case 81:
        // transitions
        if ((inval == 80 /* 'P' */ || inval == 112 /* 'p' */) && !is_end) {
            state->state = 87;
        }
        else {
            state->state = 117;
        }
        return HTTP_SERVE_OK;
    case 82:
        // transitions
        if ((inval == 78 /* 'N' */ || inval == 110 /* 'n' */) && !is_end) {
            state->state = 88;
        }
        else {
            state->state = 117;
        }
        return HTTP_SERVE_OK;
    case 83:
        // transitions
        if ((inval == 78 /* 'N' */ || inval == 110 /* 'n' */) && !is_end) {
            state->state = 89;
        }
        else {
            state->state = 117;
        }
        return HTTP_SERVE_OK;
    case 84:
        // transitions
        if ((inval == 111 /* 'o' */ || inval == 79 /* 'O' */) && !is_end) {
            state->state = 90;
        }
        else {
            state->state = 117;
        }
        return HTTP_SERVE_OK;
    case 85:
        // transitions
        if ((inval == 105 /* 'i' */ || inval == 73 /* 'I' */) && !is_end) {
            state->state = 91;
        }
        else {
            state->state = 117;
        }
        return HTTP_SERVE_OK;
    case 86:
        // transitions
        if ((inval == 116 /* 't' */ || inval == 84 /* 'T' */) && !is_end) {
            state->state = 92;
        }
        else {
            state->state = 117;
        }
        return HTTP_SERVE_OK;
    case 87:
        // transitions
        if ((inval == 69 /* 'E' */ || inval == 101 /* 'e' */) && !is_end) {
            state->state = 93;
        }
        else {
            state->state = 117;
        }
        return HTTP_SERVE_OK;
    case 88:
        // transitions
        if ((inval == 103 /* 'g' */ || inval == 71 /* 'G' */) && !is_end) {
            state->state = 94;
        }
        else {
            state->state = 117;
        }
        return HTTP_SERVE_OK;
    case 89:
        // transitions
        if ((inval == 99 /* 'c' */ || inval == 67 /* 'C' */) && !is_end) {
            state->state = 95;
        }
        else {
            state->state = 117;
        }
        return HTTP_SERVE_OK;
    case 90:
        // transitions
        if ((inval == 68 /* 'D' */ || inval == 100 /* 'd' */) && !is_end) {
            state->state = 96;
        }
        else {
            state->state = 117;
        }
        return HTTP_SERVE_OK;
    case 91:
        // transitions
        if ((inval == 111 /* 'o' */ || inval == 79 /* 'O' */) && !is_end) {
            state->state = 97;
        }
        else {
            state->state = 117;
        }
        return HTTP_SERVE_OK;
    case 92:
        // transitions
        if ((inval == 99 /* 'c' */ || inval == 67 /* 'C' */) && !is_end) {
            state->state = 98;
        }
        else {
            state->state = 117;
        }
        return HTTP_SERVE_OK;
    case 93:
        // transitions
        if ((inval == 58 /* ':' */) && !is_end) {
            state->state = 99;
        }
        else {
            state->state = 117;
        }
        return HTTP_SERVE_OK;
    case 94:
        // transitions
        if ((inval == 116 /* 't' */ || inval == 84 /* 'T' */) && !is_end) {
            state->state = 100;
        }
        else {
            state->state = 117;
        }
        return HTTP_SERVE_OK;
    case 95:
        // transitions
        if ((inval == 111 /* 'o' */ || inval == 79 /* 'O' */) && !is_end) {
            state->state = 101;
        }
        else {
            state->state = 117;
        }
        return HTTP_SERVE_OK;
    case 96:
        // transitions
        if ((inval == 105 /* 'i' */ || inval == 73 /* 'I' */) && !is_end) {
            state->state = 102;
        }
        else {
            state->state = 117;
        }
        return HTTP_SERVE_OK;
    case 97:
        // transitions
        if ((inval == 78 /* 'N' */ || inval == 110 /* 'n' */) && !is_end) {
            state->state = 103;
        }
        else {
            state->state = 117;
        }
        return HTTP_SERVE_OK;
    case 98:
        // transitions
        if ((inval == 72 /* 'H' */ || inval == 104 /* 'h' */) && !is_end) {
            state->state = 104;
        }
        else {
            state->state = 117;
        }
        return HTTP_SERVE_OK;
    case 99:
        // transitions
        if ((inval == 32 /* ' ' */) && !is_end) {
            state->state = 261;
        }
        else if ((inval == 109 /* 'm' */) && !is_end) {
            state->state = 182;
        }
        else if ((inval == 116 /* 't' */) && !is_end) {
            state->state = 183;
        }
        else if ((inval == 97 /* 'a' */) && !is_end) {
            state->state = 184;
        }
        else {
            state->state = 228;

            // action <nmfu.SetTo object at 0x7f6357442b00> 
            state->c.content_type = HTTP_SERVE_CONTENT_TYPE_OTHER;
        }
        return HTTP_SERVE_OK;
    case 100:
        // transitions
        if ((inval == 72 /* 'H' */ || inval == 104 /* 'h' */) && !is_end) {
            state->state = 105;
        }
        else {
            state->state = 117;
        }
        return HTTP_SERVE_OK;
    case 101:
        // transitions
        if ((inval == 68 /* 'D' */ || inval == 100 /* 'd' */) && !is_end) {
            state->state = 106;
        }
        else {
            state->state = 117;
        }
        return HTTP_SERVE_OK;
    case 102:
        // transitions
        if ((inval == 78 /* 'N' */ || inval == 110 /* 'n' */) && !is_end) {
            state->state = 107;
        }
        else {
            state->state = 117;
        }
        return HTTP_SERVE_OK;
    case 103:
        // transitions
        if ((inval == 58 /* ':' */) && !is_end) {
            state->state = 108;
        }
        else {
            state->state = 117;
        }
        return HTTP_SERVE_OK;
    case 104:
        // transitions
        if ((inval == 58 /* ':' */) && !is_end) {
            state->state = 109;
        }
        else {
            state->state = 117;
        }
        return HTTP_SERVE_OK;
    case 105:
        // transitions
        if ((inval == 58 /* ':' */) && !is_end) {
            state->state = 110;
        }
        else {
            state->state = 117;
        }
        return HTTP_SERVE_OK;
    case 106:
        // transitions
        if ((inval == 105 /* 'i' */ || inval == 73 /* 'I' */) && !is_end) {
            state->state = 111;
        }
        else {
            state->state = 117;
        }
        return HTTP_SERVE_OK;
    case 107:
        // transitions
        if ((inval == 103 /* 'g' */ || inval == 71 /* 'G' */) && !is_end) {
            state->state = 112;
        }
        else {
            state->state = 117;
        }
        return HTTP_SERVE_OK;
    case 108:
        // transitions
        if ((inval == 32 /* ' ' */) && !is_end) {
            state->state = 181;
        }
        else if ((inval == 66 /* 'B' */ || inval == 98 /* 'b' */) && !is_end) {
            state->state = 165;

            // action <nmfu.SetToStr object at 0x7f6357442128> 
            strncpy(state->c.auth_string, "", 52);
            state->auth_string_counter = 0;
        }
        else {
            state->state = 178;

            // action <nmfu.SetToStr object at 0x7f6357442128> 
            strncpy(state->c.auth_string, "", 52);
            state->auth_string_counter = 0;
        }
        return HTTP_SERVE_OK;
    case 109:
        // transitions
        if ((inval == 32 /* ' ' */) && !is_end) {
            state->state = 143;
        }
        else if ((inval == 87 /* 'W' */ || inval == 119 /* 'w' */) && !is_end) {
            state->state = 128;

            // action <nmfu.SetToStr object at 0x7f6357b18160> 
            strncpy(state->c.etag, "", 16);
            state->etag_counter = 0;
        }
        else if ((inval == 34 /* '"' */) && !is_end) {
            state->state = 130;

            // action <nmfu.SetToStr object at 0x7f6357b18160> 
            strncpy(state->c.etag, "", 16);
            state->etag_counter = 0;
        }
        else {
            state->state = 272;
        }
        return HTTP_SERVE_OK;
    case 110:
        // transitions
        if ((inval == 32 /* ' ' */) && !is_end) {
            state->state = 127;
        }
        else if ((inval == 13 /* '\r' */) && !is_end) {
            state->state = 126;

            // action <nmfu.SetTo object at 0x7f6357b45cf8> 
            state->c.content_length = 0;
        }
        else if (((48 <= inval && inval <= 57 /* '0' - '9' */)) && !is_end) {
            state->state = 125;

            // action <nmfu.SetTo object at 0x7f6357b45cf8> 
            state->c.content_length = 0;

            // action <nmfu.SetTo object at 0x7f6357b45f60> 
            state->c.content_length = ((state->c.content_length) * (10)) + (((int32_t)(inval)) - (48));
        }
        else {
            state->state = 272;
        }
        return HTTP_SERVE_OK;
    case 111:
        // transitions
        if ((inval == 78 /* 'N' */ || inval == 110 /* 'n' */) && !is_end) {
            state->state = 113;
        }
        else {
            state->state = 117;
        }
        return HTTP_SERVE_OK;
    case 112:
        // transitions
        if ((inval == 58 /* ':' */) && !is_end) {
            state->state = 114;
        }
        else {
            state->state = 117;
        }
        return HTTP_SERVE_OK;
    case 113:
        // transitions
        if ((inval == 103 /* 'g' */ || inval == 71 /* 'G' */) && !is_end) {
            state->state = 115;
        }
        else {
            state->state = 117;
        }
        return HTTP_SERVE_OK;
    case 114:
        // transitions
        if ((inval == 32 /* ' ' */) && !is_end) {
            state->state = 124;
        }
        else if ((inval == 103 /* 'g' */) && !is_end) {
            state->state = 120;
        }
        else if ((inval == 13 /* '\r' */) && !is_end) {
            state->state = 121;
        }
        else {
            state->state = 119;
        }
        return HTTP_SERVE_OK;
    case 115:
        // transitions
        if ((inval == 58 /* ':' */) && !is_end) {
            state->state = 116;
        }
        else {
            state->state = 117;
        }
        return HTTP_SERVE_OK;
    case 116:
        // transitions
        if ((inval == 32 /* ' ' */) && !is_end) {
            state->state = 164;
        }
        else if ((inval == 99 /* 'c' */) && !is_end) {
            state->state = 144;
        }
        else if ((inval == 105 /* 'i' */) && !is_end) {
            state->state = 145;
        }
        else {
            state->state = 159;
        }
        return HTTP_SERVE_OK;
    case 117:
        // transitions
        if ((inval == 13 /* '\r' */) && !is_end) {
            state->state = 118;
        }
        else {
            state->state = 117;
        }
        return HTTP_SERVE_OK;
    case 118:
        // transitions
        if ((inval == 10 /* '\n' */) && !is_end) {
            state->state = 34;
        }
        else {
            state->state = 117;
        }
        return HTTP_SERVE_OK;
    case 119:
        // transitions
        if ((inval == 103 /* 'g' */) && !is_end) {
            state->state = 120;
        }
        else if ((inval == 13 /* '\r' */) && !is_end) {
            state->state = 121;
        }
        else {
            state->state = 119;
        }
        return HTTP_SERVE_OK;
    case 120:
        // transitions
        if ((inval == 122 /* 'z' */) && !is_end) {
            state->state = 122;
        }
        else {
            state->state = 119;
        }
        return HTTP_SERVE_OK;
    case 121:
        // transitions
        if ((inval == 10 /* '\n' */) && !is_end) {
            state->state = 34;
        }
        else {
            state->state = 119;
        }
        return HTTP_SERVE_OK;
    case 122:
        // transitions
        if ((inval == 105 /* 'i' */) && !is_end) {
            state->state = 123;
        }
        else {
            state->state = 119;
        }
        return HTTP_SERVE_OK;
    case 123:
        // transitions
        if ((inval == 112 /* 'p' */) && !is_end) {
            state->state = 119;

            // action <nmfu.SetTo object at 0x7f6357b45588> 
            state->c.is_gzipable = true;
        }
        else {
            state->state = 119;
        }
        return HTTP_SERVE_OK;
    case 124:
        // transitions
        if ((inval == 103 /* 'g' */) && !is_end) {
            state->state = 120;
        }
        else if ((inval == 13 /* '\r' */) && !is_end) {
            state->state = 121;
        }
        else {
            state->state = 119;
        }
        return HTTP_SERVE_OK;
    case 125:
        // transitions
        if ((inval == 13 /* '\r' */) && !is_end) {
            state->state = 126;
        }
        else if (((48 <= inval && inval <= 57 /* '0' - '9' */)) && !is_end) {
            state->state = 125;

            // action <nmfu.SetTo object at 0x7f6357b45f60> 
            state->c.content_length = ((state->c.content_length) * (10)) + (((int32_t)(inval)) - (48));
        }
        else {
            state->state = 272;
        }
        return HTTP_SERVE_OK;
    case 126:
        // transitions
        if ((inval == 10 /* '\n' */) && !is_end) {
            state->state = 34;
        }
        else {
            state->state = 272;
        }
        return HTTP_SERVE_OK;
    case 127:
        // transitions
        if ((inval == 13 /* '\r' */) && !is_end) {
            state->state = 126;

            // action <nmfu.SetTo object at 0x7f6357b45cf8> 
            state->c.content_length = 0;
        }
        else if (((48 <= inval && inval <= 57 /* '0' - '9' */)) && !is_end) {
            state->state = 125;

            // action <nmfu.SetTo object at 0x7f6357b45cf8> 
            state->c.content_length = 0;

            // action <nmfu.SetTo object at 0x7f6357b45f60> 
            state->c.content_length = ((state->c.content_length) * (10)) + (((int32_t)(inval)) - (48));
        }
        else {
            state->state = 272;

            // action <nmfu.SetTo object at 0x7f6357b45cf8> 
            state->c.content_length = 0;
        }
        return HTTP_SERVE_OK;
    case 128:
        // transitions
        if ((inval == 47 /* '/' */) && !is_end) {
            state->state = 129;
        }
        else {
            state->state = 272;
        }
        return HTTP_SERVE_OK;
    case 129:
        // transitions
        if ((inval == 34 /* '"' */) && !is_end) {
            state->state = 130;
        }
        else {
            state->state = 272;
        }
        return HTTP_SERVE_OK;
    case 130:
        // transitions
        if (is_end) {
            state->state = 272;
        }
        else if ((inval == 34 /* '"' */) && !is_end) {
            state->state = 131;

            // action <nmfu.SetTo object at 0x7f6357b189e8> 
            state->c.is_conditional = true;
        }
        else {
            state->state = 139;

            // action <nmfu.AppendTo object at 0x7f6357b18b70> 
            if (state->etag_counter == 15) state->state = 140;
            else {
                state->c.etag[state->etag_counter++] = (char)(inval);
                state->c.etag[state->etag_counter] = 0;
            }
        }
        return HTTP_SERVE_OK;
    case 131:
        // transitions
        if ((inval == 44 /* ',' */) && !is_end) {
            state->state = 138;
        }
        else if ((inval == 13 /* '\r' */) && !is_end) {
            state->state = 132;
        }
        else {
            state->state = 272;
        }
        return HTTP_SERVE_OK;
    case 132:
        // transitions
        if ((inval == 10 /* '\n' */) && !is_end) {
            state->state = 34;
        }
        else {
            state->state = 272;
        }
        return HTTP_SERVE_OK;
    case 133:
        // transitions
        if ((inval == 34 /* '"' */) && !is_end) {
            state->state = 137;
        }
        else if ((inval == 87 /* 'W' */ || inval == 119 /* 'w' */) && !is_end) {
            state->state = 135;
        }
        else {
            state->state = 272;
        }
        return HTTP_SERVE_OK;
    case 134:
        // transitions
        if ((inval == 34 /* '"' */) && !is_end) {
            state->state = 137;
        }
        else {
            state->state = 272;
        }
        return HTTP_SERVE_OK;
    case 135:
        // transitions
        if ((inval == 47 /* '/' */) && !is_end) {
            state->state = 134;
        }
        else {
            state->state = 272;
        }
        return HTTP_SERVE_OK;
    case 136:
        // transitions
        if ((inval == 44 /* ',' */) && !is_end) {
            state->state = 138;
        }
        else if ((inval == 13 /* '\r' */) && !is_end) {
            state->state = 132;
        }
        else {
            state->state = 272;
        }
        return HTTP_SERVE_OK;
    case 137:
        // transitions
        if ((inval == 34 /* '"' */) && !is_end) {
            state->state = 136;
        }
        else if (is_end) {
            state->state = 272;
        }
        else {
            state->state = 137;
        }
        return HTTP_SERVE_OK;
    case 138:
        // transitions
        if ((inval == 34 /* '"' */) && !is_end) {
            state->state = 137;
        }
        else if ((inval == 87 /* 'W' */ || inval == 119 /* 'w' */) && !is_end) {
            state->state = 135;
        }
        else if ((inval == 32 /* ' ' */) && !is_end) {
            state->state = 133;
        }
        else {
            state->state = 272;
        }
        return HTTP_SERVE_OK;
    case 139:
        // transitions
        if (is_end) {
            state->state = 272;
        }
        else if ((inval == 34 /* '"' */) && !is_end) {
            state->state = 131;

            // action <nmfu.SetTo object at 0x7f6357b189e8> 
            state->c.is_conditional = true;
        }
        else {
            state->state = 139;

            // action <nmfu.AppendTo object at 0x7f6357b18b70> 
            if (state->etag_counter == 15) state->state = 140;
            else {
                state->c.etag[state->etag_counter++] = (char)(inval);
                state->c.etag[state->etag_counter] = 0;
            }
        }
        return HTTP_SERVE_OK;
    case 140:
        // transitions
        if ((inval == 13 /* '\r' */) && !is_end) {
            state->state = 141;
        }
        else {
            state->state = 142;
        }
        return HTTP_SERVE_OK;
    case 141:
        // transitions
        if ((inval == 10 /* '\n' */) && !is_end) {
            state->state = 34;
        }
        else {
            state->state = 142;
        }
        return HTTP_SERVE_OK;
    case 142:
        // transitions
        if ((inval == 13 /* '\r' */) && !is_end) {
            state->state = 141;
        }
        else {
            state->state = 142;
        }
        return HTTP_SERVE_OK;
    case 143:
        // transitions
        if ((inval == 87 /* 'W' */ || inval == 119 /* 'w' */) && !is_end) {
            state->state = 128;

            // action <nmfu.SetToStr object at 0x7f6357b18160> 
            strncpy(state->c.etag, "", 16);
            state->etag_counter = 0;
        }
        else if ((inval == 34 /* '"' */) && !is_end) {
            state->state = 130;

            // action <nmfu.SetToStr object at 0x7f6357b18160> 
            strncpy(state->c.etag, "", 16);
            state->etag_counter = 0;
        }
        else {
            state->state = 272;

            // action <nmfu.SetToStr object at 0x7f6357b18160> 
            strncpy(state->c.etag, "", 16);
            state->etag_counter = 0;
        }
        return HTTP_SERVE_OK;
    case 144:
        // transitions
        if ((inval == 104 /* 'h' */) && !is_end) {
            state->state = 146;
        }
        else {
            state->state = 159;
        }
        return HTTP_SERVE_OK;
    case 145:
        // transitions
        if ((inval == 100 /* 'd' */) && !is_end) {
            state->state = 147;
        }
        else {
            state->state = 159;
        }
        return HTTP_SERVE_OK;
    case 146:
        // transitions
        if ((inval == 117 /* 'u' */) && !is_end) {
            state->state = 148;
        }
        else {
            state->state = 159;
        }
        return HTTP_SERVE_OK;
    case 147:
        // transitions
        if ((inval == 101 /* 'e' */) && !is_end) {
            state->state = 149;
        }
        else {
            state->state = 159;
        }
        return HTTP_SERVE_OK;
    case 148:
        // transitions
        if ((inval == 110 /* 'n' */) && !is_end) {
            state->state = 150;
        }
        else {
            state->state = 159;
        }
        return HTTP_SERVE_OK;
    case 149:
        // transitions
        if ((inval == 110 /* 'n' */) && !is_end) {
            state->state = 151;
        }
        else {
            state->state = 159;
        }
        return HTTP_SERVE_OK;
    case 150:
        // transitions
        if ((inval == 107 /* 'k' */) && !is_end) {
            state->state = 152;
        }
        else {
            state->state = 159;
        }
        return HTTP_SERVE_OK;
    case 151:
        // transitions
        if ((inval == 116 /* 't' */) && !is_end) {
            state->state = 153;
        }
        else {
            state->state = 159;
        }
        return HTTP_SERVE_OK;
    case 152:
        // transitions
        if ((inval == 101 /* 'e' */) && !is_end) {
            state->state = 154;
        }
        else {
            state->state = 159;
        }
        return HTTP_SERVE_OK;
    case 153:
        // transitions
        if ((inval == 105 /* 'i' */) && !is_end) {
            state->state = 155;
        }
        else {
            state->state = 159;
        }
        return HTTP_SERVE_OK;
    case 154:
        // transitions
        if ((inval == 100 /* 'd' */) && !is_end) {
            state->state = 156;

            // action <nmfu.SetTo object at 0x7f6357b45940> 
            state->c.is_chunked = true;
        }
        else {
            state->state = 159;
        }
        return HTTP_SERVE_OK;
    case 155:
        // transitions
        if ((inval == 116 /* 't' */) && !is_end) {
            state->state = 157;
        }
        else {
            state->state = 159;
        }
        return HTTP_SERVE_OK;
    case 156:
        // transitions
        if ((inval == 13 /* '\r' */) && !is_end) {
            state->state = 163;
        }
        else {
            state->state = 272;
        }
        return HTTP_SERVE_OK;
    case 157:
        // transitions
        if ((inval == 121 /* 'y' */) && !is_end) {
            state->state = 158;

            // action <nmfu.SetTo object at 0x7f6357b45828> 
            state->c.is_chunked = false;
        }
        else {
            state->state = 159;
        }
        return HTTP_SERVE_OK;
    case 158:
        // transitions
        if ((inval == 13 /* '\r' */) && !is_end) {
            state->state = 163;
        }
        else {
            state->state = 272;
        }
        return HTTP_SERVE_OK;
    case 159:
        // transitions
        if ((inval == 13 /* '\r' */) && !is_end) {
            state->state = 160;
        }
        else {
            state->state = 159;
        }
        return HTTP_SERVE_OK;
    case 160:
        // transitions
        if ((inval == 10 /* '\n' */) && !is_end) {
            state->state = 161;
        }
        else {
            state->state = 159;
        }
        return HTTP_SERVE_OK;
    case 161:
        // transitions
        if ((inval == 13 /* '\r' */) && !is_end) {
            state->state = 162;
        }
        else {
            state->state = 159;
        }
        return HTTP_SERVE_OK;
    case 162:
        // transitions
        if ((inval == 10 /* '\n' */) && !is_end) {
            // terminating state

            // action <nmfu.SetTo object at 0x7f6357b45a58> 
            state->c.error_code = HTTP_SERVE_ERROR_CODE_UNSUPPORTED_ENCODING;

            // action <nmfu.FinishAction object at 0x7f6357b459b0> 
            return HTTP_SERVE_DONE;
        }
        else {
            state->state = 159;
        }
        return HTTP_SERVE_OK;
    case 163:
        // transitions
        if ((inval == 10 /* '\n' */) && !is_end) {
            state->state = 34;
        }
        else {
            state->state = 272;
        }
        return HTTP_SERVE_OK;
    case 164:
        // transitions
        if ((inval == 99 /* 'c' */) && !is_end) {
            state->state = 144;
        }
        else if ((inval == 105 /* 'i' */) && !is_end) {
            state->state = 145;
        }
        else {
            state->state = 159;
        }
        return HTTP_SERVE_OK;
    case 165:
        // transitions
        if ((inval == 97 /* 'a' */ || inval == 65 /* 'A' */) && !is_end) {
            state->state = 166;
        }
        else {
            state->state = 178;
        }
        return HTTP_SERVE_OK;
    case 166:
        // transitions
        if ((inval == 115 /* 's' */ || inval == 83 /* 'S' */) && !is_end) {
            state->state = 167;
        }
        else {
            state->state = 178;
        }
        return HTTP_SERVE_OK;
    case 167:
        // transitions
        if ((inval == 105 /* 'i' */ || inval == 73 /* 'I' */) && !is_end) {
            state->state = 168;
        }
        else {
            state->state = 178;
        }
        return HTTP_SERVE_OK;
    case 168:
        // transitions
        if ((inval == 99 /* 'c' */ || inval == 67 /* 'C' */) && !is_end) {
            state->state = 169;
        }
        else {
            state->state = 178;
        }
        return HTTP_SERVE_OK;
    case 169:
        // transitions
        if ((inval == 32 /* ' ' */) && !is_end) {
            state->state = 170;
        }
        else {
            state->state = 178;
        }
        return HTTP_SERVE_OK;
    case 170:
        // transitions
        if (((47 <= inval && inval <= 57 /* '/' - '9' */) || (65 <= inval && inval <= 90 /* 'A' - 'Z' */) || (97 <= inval && inval <= 122 /* 'a' - 'z' */) || inval == 43 /* '+' */ || inval == 61 /* '=' */) && !is_end) {
            state->state = 177;

            // action <nmfu.AppendTo object at 0x7f6357442240> 
            if (state->auth_string_counter == 51) state->state = 176;
            else {
                state->c.auth_string[state->auth_string_counter++] = (char)(inval);
                state->c.auth_string[state->auth_string_counter] = 0;
            }
        }
        else {
            state->state = 178;
        }
        return HTTP_SERVE_OK;
    case 171:
        // transitions
        if ((inval == 10 /* '\n' */) && !is_end) {
            state->state = 34;

            // action <nmfu.SetTo object at 0x7f6357442940> 
            state->c.auth_type = HTTP_SERVE_AUTH_TYPE_OK;
        }
        else {
            state->state = 178;
        }
        return HTTP_SERVE_OK;
    case 172:
        // transitions
        if ((inval == 13 /* '\r' */) && !is_end) {
            state->state = 173;
        }
        else {
            state->state = 172;
        }
        return HTTP_SERVE_OK;
    case 173:
        // transitions
        if ((inval == 10 /* '\n' */) && !is_end) {
            state->state = 174;
        }
        else {
            state->state = 172;
        }
        return HTTP_SERVE_OK;
    case 174:
        // transitions
        if ((inval == 13 /* '\r' */) && !is_end) {
            state->state = 175;
        }
        else {
            state->state = 172;
        }
        return HTTP_SERVE_OK;
    case 175:
        // transitions
        if ((inval == 10 /* '\n' */) && !is_end) {
            // terminating state

            // action <nmfu.SetTo object at 0x7f63574422e8> 
            state->c.error_code = HTTP_SERVE_ERROR_CODE_AUTH_TOO_LONG;

            // action <nmfu.FinishAction object at 0x7f63574422b0> 
            return HTTP_SERVE_DONE;
        }
        else {
            state->state = 172;
        }
        return HTTP_SERVE_OK;
    case 176:
        // transitions
        if ((inval == 13 /* '\r' */) && !is_end) {
            state->state = 173;
        }
        else {
            state->state = 172;
        }
        return HTTP_SERVE_OK;
    case 177:
        // transitions
        if (((47 <= inval && inval <= 57 /* '/' - '9' */) || (65 <= inval && inval <= 90 /* 'A' - 'Z' */) || (97 <= inval && inval <= 122 /* 'a' - 'z' */) || inval == 43 /* '+' */ || inval == 61 /* '=' */) && !is_end) {
            state->state = 177;

            // action <nmfu.AppendTo object at 0x7f6357442240> 
            if (state->auth_string_counter == 51) state->state = 176;
            else {
                state->c.auth_string[state->auth_string_counter++] = (char)(inval);
                state->c.auth_string[state->auth_string_counter] = 0;
            }
        }
        else if ((inval == 13 /* '\r' */) && !is_end) {
            state->state = 171;
        }
        else {
            state->state = 178;
        }
        return HTTP_SERVE_OK;
    case 178:
        // transitions
        if ((inval == 13 /* '\r' */) && !is_end) {
            state->state = 179;

            // action <nmfu.SetTo object at 0x7f635761bba8> 
            state->c.auth_type = HTTP_SERVE_AUTH_TYPE_INVALID_TYPE;
        }
        else {
            state->state = 180;

            // action <nmfu.SetTo object at 0x7f635761bba8> 
            state->c.auth_type = HTTP_SERVE_AUTH_TYPE_INVALID_TYPE;
        }
        return HTTP_SERVE_OK;
    case 179:
        // transitions
        if ((inval == 10 /* '\n' */) && !is_end) {
            state->state = 34;
        }
        else {
            state->state = 180;
        }
        return HTTP_SERVE_OK;
    case 180:
        // transitions
        if ((inval == 13 /* '\r' */) && !is_end) {
            state->state = 179;
        }
        else {
            state->state = 180;
        }
        return HTTP_SERVE_OK;
    case 181:
        // transitions
        if ((inval == 66 /* 'B' */ || inval == 98 /* 'b' */) && !is_end) {
            state->state = 165;

            // action <nmfu.SetToStr object at 0x7f6357442128> 
            strncpy(state->c.auth_string, "", 52);
            state->auth_string_counter = 0;
        }
        else {
            state->state = 178;

            // action <nmfu.SetToStr object at 0x7f6357442128> 
            strncpy(state->c.auth_string, "", 52);
            state->auth_string_counter = 0;
        }
        return HTTP_SERVE_OK;
    case 182:
        // transitions
        if ((inval == 117 /* 'u' */) && !is_end) {
            state->state = 185;
        }
        else {
            state->state = 228;

            // action <nmfu.SetTo object at 0x7f6357442b00> 
            state->c.content_type = HTTP_SERVE_CONTENT_TYPE_OTHER;
        }
        return HTTP_SERVE_OK;
    case 183:
        // transitions
        if ((inval == 101 /* 'e' */) && !is_end) {
            state->state = 186;
        }
        else {
            state->state = 228;

            // action <nmfu.SetTo object at 0x7f6357442b00> 
            state->c.content_type = HTTP_SERVE_CONTENT_TYPE_OTHER;
        }
        return HTTP_SERVE_OK;
    case 184:
        // transitions
        if ((inval == 112 /* 'p' */) && !is_end) {
            state->state = 187;
        }
        else {
            state->state = 228;

            // action <nmfu.SetTo object at 0x7f6357442b00> 
            state->c.content_type = HTTP_SERVE_CONTENT_TYPE_OTHER;
        }
        return HTTP_SERVE_OK;
    case 185:
        // transitions
        if ((inval == 108 /* 'l' */) && !is_end) {
            state->state = 188;
        }
        else {
            state->state = 228;

            // action <nmfu.SetTo object at 0x7f6357442b00> 
            state->c.content_type = HTTP_SERVE_CONTENT_TYPE_OTHER;
        }
        return HTTP_SERVE_OK;
    case 186:
        // transitions
        if ((inval == 120 /* 'x' */) && !is_end) {
            state->state = 189;
        }
        else {
            state->state = 228;

            // action <nmfu.SetTo object at 0x7f6357442b00> 
            state->c.content_type = HTTP_SERVE_CONTENT_TYPE_OTHER;
        }
        return HTTP_SERVE_OK;
    case 187:
        // transitions
        if ((inval == 112 /* 'p' */) && !is_end) {
            state->state = 190;
        }
        else {
            state->state = 228;

            // action <nmfu.SetTo object at 0x7f6357442b00> 
            state->c.content_type = HTTP_SERVE_CONTENT_TYPE_OTHER;
        }
        return HTTP_SERVE_OK;
    case 188:
        // transitions
        if ((inval == 116 /* 't' */) && !is_end) {
            state->state = 191;
        }
        else {
            state->state = 228;

            // action <nmfu.SetTo object at 0x7f6357442b00> 
            state->c.content_type = HTTP_SERVE_CONTENT_TYPE_OTHER;
        }
        return HTTP_SERVE_OK;
    case 189:
        // transitions
        if ((inval == 116 /* 't' */) && !is_end) {
            state->state = 192;
        }
        else {
            state->state = 228;

            // action <nmfu.SetTo object at 0x7f6357442b00> 
            state->c.content_type = HTTP_SERVE_CONTENT_TYPE_OTHER;
        }
        return HTTP_SERVE_OK;
    case 190:
        // transitions
        if ((inval == 108 /* 'l' */) && !is_end) {
            state->state = 193;
        }
        else {
            state->state = 228;

            // action <nmfu.SetTo object at 0x7f6357442b00> 
            state->c.content_type = HTTP_SERVE_CONTENT_TYPE_OTHER;
        }
        return HTTP_SERVE_OK;
    case 191:
        // transitions
        if ((inval == 105 /* 'i' */) && !is_end) {
            state->state = 194;
        }
        else {
            state->state = 228;

            // action <nmfu.SetTo object at 0x7f6357442b00> 
            state->c.content_type = HTTP_SERVE_CONTENT_TYPE_OTHER;
        }
        return HTTP_SERVE_OK;
    case 192:
        // transitions
        if ((inval == 47 /* '/' */) && !is_end) {
            state->state = 195;
        }
        else {
            state->state = 228;

            // action <nmfu.SetTo object at 0x7f6357442b00> 
            state->c.content_type = HTTP_SERVE_CONTENT_TYPE_OTHER;
        }
        return HTTP_SERVE_OK;
    case 193:
        // transitions
        if ((inval == 105 /* 'i' */) && !is_end) {
            state->state = 196;
        }
        else {
            state->state = 228;

            // action <nmfu.SetTo object at 0x7f6357442b00> 
            state->c.content_type = HTTP_SERVE_CONTENT_TYPE_OTHER;
        }
        return HTTP_SERVE_OK;
    case 194:
        // transitions
        if ((inval == 112 /* 'p' */) && !is_end) {
            state->state = 197;
        }
        else {
            state->state = 228;

            // action <nmfu.SetTo object at 0x7f6357442b00> 
            state->c.content_type = HTTP_SERVE_CONTENT_TYPE_OTHER;
        }
        return HTTP_SERVE_OK;
    case 195:
        // transitions
        if ((inval == 106 /* 'j' */) && !is_end) {
            state->state = 198;
        }
        else if ((inval == 112 /* 'p' */) && !is_end) {
            state->state = 199;
        }
        else {
            state->state = 228;

            // action <nmfu.SetTo object at 0x7f6357442b00> 
            state->c.content_type = HTTP_SERVE_CONTENT_TYPE_OTHER;
        }
        return HTTP_SERVE_OK;
    case 196:
        // transitions
        if ((inval == 99 /* 'c' */) && !is_end) {
            state->state = 200;
        }
        else {
            state->state = 228;

            // action <nmfu.SetTo object at 0x7f6357442b00> 
            state->c.content_type = HTTP_SERVE_CONTENT_TYPE_OTHER;
        }
        return HTTP_SERVE_OK;
    case 197:
        // transitions
        if ((inval == 97 /* 'a' */) && !is_end) {
            state->state = 201;
        }
        else {
            state->state = 228;

            // action <nmfu.SetTo object at 0x7f6357442b00> 
            state->c.content_type = HTTP_SERVE_CONTENT_TYPE_OTHER;
        }
        return HTTP_SERVE_OK;
    case 198:
        // transitions
        if ((inval == 115 /* 's' */) && !is_end) {
            state->state = 202;
        }
        else {
            state->state = 228;

            // action <nmfu.SetTo object at 0x7f6357442b00> 
            state->c.content_type = HTTP_SERVE_CONTENT_TYPE_OTHER;
        }
        return HTTP_SERVE_OK;
    case 199:
        // transitions
        if ((inval == 108 /* 'l' */) && !is_end) {
            state->state = 203;
        }
        else {
            state->state = 228;

            // action <nmfu.SetTo object at 0x7f6357442b00> 
            state->c.content_type = HTTP_SERVE_CONTENT_TYPE_OTHER;
        }
        return HTTP_SERVE_OK;
    case 200:
        // transitions
        if ((inval == 97 /* 'a' */) && !is_end) {
            state->state = 204;
        }
        else {
            state->state = 228;

            // action <nmfu.SetTo object at 0x7f6357442b00> 
            state->c.content_type = HTTP_SERVE_CONTENT_TYPE_OTHER;
        }
        return HTTP_SERVE_OK;
    case 201:
        // transitions
        if ((inval == 114 /* 'r' */) && !is_end) {
            state->state = 205;
        }
        else {
            state->state = 228;

            // action <nmfu.SetTo object at 0x7f6357442b00> 
            state->c.content_type = HTTP_SERVE_CONTENT_TYPE_OTHER;
        }
        return HTTP_SERVE_OK;
    case 202:
        // transitions
        if ((inval == 111 /* 'o' */) && !is_end) {
            state->state = 206;
        }
        else {
            state->state = 228;

            // action <nmfu.SetTo object at 0x7f6357442b00> 
            state->c.content_type = HTTP_SERVE_CONTENT_TYPE_OTHER;
        }
        return HTTP_SERVE_OK;
    case 203:
        // transitions
        if ((inval == 97 /* 'a' */) && !is_end) {
            state->state = 207;
        }
        else {
            state->state = 228;

            // action <nmfu.SetTo object at 0x7f6357442b00> 
            state->c.content_type = HTTP_SERVE_CONTENT_TYPE_OTHER;
        }
        return HTTP_SERVE_OK;
    case 204:
        // transitions
        if ((inval == 116 /* 't' */) && !is_end) {
            state->state = 208;
        }
        else {
            state->state = 228;

            // action <nmfu.SetTo object at 0x7f6357442b00> 
            state->c.content_type = HTTP_SERVE_CONTENT_TYPE_OTHER;
        }
        return HTTP_SERVE_OK;
    case 205:
        // transitions
        if ((inval == 116 /* 't' */) && !is_end) {
            state->state = 209;
        }
        else {
            state->state = 228;

            // action <nmfu.SetTo object at 0x7f6357442b00> 
            state->c.content_type = HTTP_SERVE_CONTENT_TYPE_OTHER;
        }
        return HTTP_SERVE_OK;
    case 206:
        // transitions
        if ((inval == 110 /* 'n' */) && !is_end) {
            state->state = 210;

            // action <nmfu.SetTo object at 0x7f63577f5748> 
            state->c.content_type = HTTP_SERVE_CONTENT_TYPE_JSON;
        }
        else {
            state->state = 228;

            // action <nmfu.SetTo object at 0x7f6357442b00> 
            state->c.content_type = HTTP_SERVE_CONTENT_TYPE_OTHER;
        }
        return HTTP_SERVE_OK;
    case 207:
        // transitions
        if ((inval == 105 /* 'i' */) && !is_end) {
            state->state = 211;
        }
        else {
            state->state = 228;

            // action <nmfu.SetTo object at 0x7f6357442b00> 
            state->c.content_type = HTTP_SERVE_CONTENT_TYPE_OTHER;
        }
        return HTTP_SERVE_OK;
    case 208:
        // transitions
        if ((inval == 105 /* 'i' */) && !is_end) {
            state->state = 212;
        }
        else {
            state->state = 228;

            // action <nmfu.SetTo object at 0x7f6357442b00> 
            state->c.content_type = HTTP_SERVE_CONTENT_TYPE_OTHER;
        }
        return HTTP_SERVE_OK;
    case 209:
        // transitions
        if ((inval == 47 /* '/' */) && !is_end) {
            state->state = 213;
        }
        else {
            state->state = 228;

            // action <nmfu.SetTo object at 0x7f6357442b00> 
            state->c.content_type = HTTP_SERVE_CONTENT_TYPE_OTHER;
        }
        return HTTP_SERVE_OK;
    case 210:
        // transitions
        if ((inval == 13 /* '\r' */) && !is_end) {
            state->state = 259;
        }
        else {
            state->state = 260;
        }
        return HTTP_SERVE_OK;
    case 211:
        // transitions
        if ((inval == 110 /* 'n' */) && !is_end) {
            state->state = 214;

            // action <nmfu.SetTo object at 0x7f63577f52e8> 
            state->c.content_type = HTTP_SERVE_CONTENT_TYPE_PLAIN;
        }
        else {
            state->state = 228;

            // action <nmfu.SetTo object at 0x7f6357442b00> 
            state->c.content_type = HTTP_SERVE_CONTENT_TYPE_OTHER;
        }
        return HTTP_SERVE_OK;
    case 212:
        // transitions
        if ((inval == 111 /* 'o' */) && !is_end) {
            state->state = 215;
        }
        else {
            state->state = 228;

            // action <nmfu.SetTo object at 0x7f6357442b00> 
            state->c.content_type = HTTP_SERVE_CONTENT_TYPE_OTHER;
        }
        return HTTP_SERVE_OK;
    case 213:
        // transitions
        if ((inval == 102 /* 'f' */) && !is_end) {
            state->state = 216;
        }
        else {
            state->state = 228;

            // action <nmfu.SetTo object at 0x7f6357442b00> 
            state->c.content_type = HTTP_SERVE_CONTENT_TYPE_OTHER;
        }
        return HTTP_SERVE_OK;
    case 214:
        // transitions
        if ((inval == 13 /* '\r' */) && !is_end) {
            state->state = 259;
        }
        else {
            state->state = 260;
        }
        return HTTP_SERVE_OK;
    case 215:
        // transitions
        if ((inval == 110 /* 'n' */) && !is_end) {
            state->state = 217;
        }
        else {
            state->state = 228;

            // action <nmfu.SetTo object at 0x7f6357442b00> 
            state->c.content_type = HTTP_SERVE_CONTENT_TYPE_OTHER;
        }
        return HTTP_SERVE_OK;
    case 216:
        // transitions
        if ((inval == 111 /* 'o' */) && !is_end) {
            state->state = 218;
        }
        else {
            state->state = 228;

            // action <nmfu.SetTo object at 0x7f6357442b00> 
            state->c.content_type = HTTP_SERVE_CONTENT_TYPE_OTHER;
        }
        return HTTP_SERVE_OK;
    case 217:
        // transitions
        if ((inval == 47 /* '/' */) && !is_end) {
            state->state = 219;
        }
        else {
            state->state = 228;

            // action <nmfu.SetTo object at 0x7f6357442b00> 
            state->c.content_type = HTTP_SERVE_CONTENT_TYPE_OTHER;
        }
        return HTTP_SERVE_OK;
    case 218:
        // transitions
        if ((inval == 114 /* 'r' */) && !is_end) {
            state->state = 220;
        }
        else {
            state->state = 228;

            // action <nmfu.SetTo object at 0x7f6357442b00> 
            state->c.content_type = HTTP_SERVE_CONTENT_TYPE_OTHER;
        }
        return HTTP_SERVE_OK;
    case 219:
        // transitions
        if ((inval == 106 /* 'j' */) && !is_end) {
            state->state = 198;
        }
        else {
            state->state = 228;

            // action <nmfu.SetTo object at 0x7f6357442b00> 
            state->c.content_type = HTTP_SERVE_CONTENT_TYPE_OTHER;
        }
        return HTTP_SERVE_OK;
    case 220:
        // transitions
        if ((inval == 109 /* 'm' */) && !is_end) {
            state->state = 221;
        }
        else {
            state->state = 228;

            // action <nmfu.SetTo object at 0x7f6357442b00> 
            state->c.content_type = HTTP_SERVE_CONTENT_TYPE_OTHER;
        }
        return HTTP_SERVE_OK;
    case 221:
        // transitions
        if ((inval == 45 /* '-' */) && !is_end) {
            state->state = 222;
        }
        else {
            state->state = 228;

            // action <nmfu.SetTo object at 0x7f6357442b00> 
            state->c.content_type = HTTP_SERVE_CONTENT_TYPE_OTHER;
        }
        return HTTP_SERVE_OK;
    case 222:
        // transitions
        if ((inval == 100 /* 'd' */) && !is_end) {
            state->state = 223;
        }
        else {
            state->state = 228;

            // action <nmfu.SetTo object at 0x7f6357442b00> 
            state->c.content_type = HTTP_SERVE_CONTENT_TYPE_OTHER;
        }
        return HTTP_SERVE_OK;
    case 223:
        // transitions
        if ((inval == 97 /* 'a' */) && !is_end) {
            state->state = 224;
        }
        else {
            state->state = 228;

            // action <nmfu.SetTo object at 0x7f6357442b00> 
            state->c.content_type = HTTP_SERVE_CONTENT_TYPE_OTHER;
        }
        return HTTP_SERVE_OK;
    case 224:
        // transitions
        if ((inval == 116 /* 't' */) && !is_end) {
            state->state = 225;
        }
        else {
            state->state = 228;

            // action <nmfu.SetTo object at 0x7f6357442b00> 
            state->c.content_type = HTTP_SERVE_CONTENT_TYPE_OTHER;
        }
        return HTTP_SERVE_OK;
    case 225:
        // transitions
        if ((inval == 97 /* 'a' */) && !is_end) {
            state->state = 226;
        }
        else {
            state->state = 228;

            // action <nmfu.SetTo object at 0x7f6357442b00> 
            state->c.content_type = HTTP_SERVE_CONTENT_TYPE_OTHER;
        }
        return HTTP_SERVE_OK;
    case 226:
        // transitions
        if ((inval == 59 /* ';' */) && !is_end) {
            state->state = 227;
        }
        else {
            state->state = 228;

            // action <nmfu.SetTo object at 0x7f6357442b00> 
            state->c.content_type = HTTP_SERVE_CONTENT_TYPE_OTHER;
        }
        return HTTP_SERVE_OK;
    case 227:
        // transitions
        if ((inval == 32 /* ' ' */) && !is_end) {
            state->state = 258;

            // action <nmfu.SetTo object at 0x7f63574429e8> 
            state->c.content_type = HTTP_SERVE_CONTENT_TYPE_MULTIPART_FILE;

            // action <nmfu.SetToStr object at 0x7f6357442d68> 
            strncpy(state->c.multipart_boundary, "", 71);
            state->multipart_boundary_counter = 0;
        }
        else if ((inval == 99 /* 'c' */) && !is_end) {
            state->state = 229;

            // action <nmfu.SetTo object at 0x7f63574429e8> 
            state->c.content_type = HTTP_SERVE_CONTENT_TYPE_MULTIPART_FILE;

            // action <nmfu.SetToStr object at 0x7f6357442d68> 
            strncpy(state->c.multipart_boundary, "", 71);
            state->multipart_boundary_counter = 0;
        }
        else if ((inval == 98 /* 'b' */) && !is_end) {
            state->state = 257;

            // action <nmfu.SetTo object at 0x7f63574429e8> 
            state->c.content_type = HTTP_SERVE_CONTENT_TYPE_MULTIPART_FILE;

            // action <nmfu.SetToStr object at 0x7f6357442d68> 
            strncpy(state->c.multipart_boundary, "", 71);
            state->multipart_boundary_counter = 0;
        }
        else {
            state->state = 272;

            // action <nmfu.SetTo object at 0x7f63574429e8> 
            state->c.content_type = HTTP_SERVE_CONTENT_TYPE_MULTIPART_FILE;

            // action <nmfu.SetToStr object at 0x7f6357442d68> 
            strncpy(state->c.multipart_boundary, "", 71);
            state->multipart_boundary_counter = 0;
        }
        return HTTP_SERVE_OK;
    case 228:
        // transitions
        if ((inval == 13 /* '\r' */) && !is_end) {
            state->state = 259;
        }
        else {
            state->state = 260;
        }
        return HTTP_SERVE_OK;
    case 229:
        // transitions
        if ((inval == 104 /* 'h' */) && !is_end) {
            state->state = 230;
        }
        else {
            state->state = 272;
        }
        return HTTP_SERVE_OK;
    case 230:
        // transitions
        if ((inval == 97 /* 'a' */) && !is_end) {
            state->state = 231;
        }
        else {
            state->state = 272;
        }
        return HTTP_SERVE_OK;
    case 231:
        // transitions
        if ((inval == 114 /* 'r' */) && !is_end) {
            state->state = 232;
        }
        else {
            state->state = 272;
        }
        return HTTP_SERVE_OK;
    case 232:
        // transitions
        if ((inval == 115 /* 's' */) && !is_end) {
            state->state = 233;
        }
        else {
            state->state = 272;
        }
        return HTTP_SERVE_OK;
    case 233:
        // transitions
        if ((inval == 101 /* 'e' */) && !is_end) {
            state->state = 234;
        }
        else {
            state->state = 272;
        }
        return HTTP_SERVE_OK;
    case 234:
        // transitions
        if ((inval == 116 /* 't' */) && !is_end) {
            state->state = 235;
        }
        else {
            state->state = 272;
        }
        return HTTP_SERVE_OK;
    case 235:
        // transitions
        if ((inval == 61 /* '=' */) && !is_end) {
            state->state = 236;
        }
        else {
            state->state = 272;
        }
        return HTTP_SERVE_OK;
    case 236:
        // transitions
        if (is_end || inval == 10 /* '\n' */ || inval == 13 /* '\r' */ || inval == 32 /* ' ' */ || inval == 59 /* ';' */) {
            state->state = 272;
        }
        else {
            state->state = 237;
        }
        return HTTP_SERVE_OK;
    case 237:
        // transitions
        if ((inval == 59 /* ';' */) && !is_end) {
            state->state = 238;
        }
        else if (is_end || inval == 10 /* '\n' */ || inval == 13 /* '\r' */ || inval == 32 /* ' ' */) {
            state->state = 272;
        }
        else {
            state->state = 237;
        }
        return HTTP_SERVE_OK;
    case 238:
        // transitions
        if ((inval == 32 /* ' ' */) && !is_end) {
            state->state = 239;
        }
        else if ((inval == 98 /* 'b' */) && !is_end) {
            state->state = 257;
        }
        else {
            state->state = 272;
        }
        return HTTP_SERVE_OK;
    case 239:
        // transitions
        if ((inval == 98 /* 'b' */) && !is_end) {
            state->state = 257;
        }
        else {
            state->state = 272;
        }
        return HTTP_SERVE_OK;
    case 240:
        // transitions
        if (is_end || inval == 34 /* '"' */) {
            state->state = 272;
        }
        else {
            state->state = 243;

            // action <nmfu.AppendTo object at 0x7f63578120b8> 
            if (state->multipart_boundary_counter == 70) state->state = 244;
            else {
                state->c.multipart_boundary[state->multipart_boundary_counter++] = (char)(inval);
                state->c.multipart_boundary[state->multipart_boundary_counter] = 0;
            }
        }
        return HTTP_SERVE_OK;
    case 241:
        // transitions
        if (((48 <= inval && inval <= 57 /* '0' - '9' */) || (65 <= inval && inval <= 90 /* 'A' - 'Z' */) || (97 <= inval && inval <= 122 /* 'a' - 'z' */) || inval == 45 /* '-' */ || inval == 95 /* '_' */) && !is_end) {
            state->state = 241;

            // action <nmfu.AppendTo object at 0x7f6357812198> 
            if (state->multipart_boundary_counter == 70) state->state = 244;
            else {
                state->c.multipart_boundary[state->multipart_boundary_counter++] = (char)(inval);
                state->c.multipart_boundary[state->multipart_boundary_counter] = 0;
            }
        }
        else if ((inval == 13 /* '\r' */) && !is_end) {
            state->state = 259;
        }
        else {
            state->state = 260;
        }
        return HTTP_SERVE_OK;
    case 242:
        // transitions
        if ((inval == 13 /* '\r' */) && !is_end) {
            state->state = 259;
        }
        else {
            state->state = 260;
        }
        return HTTP_SERVE_OK;
    case 243:
        // transitions
        if (is_end) {
            state->state = 272;
        }
        else if ((inval == 34 /* '"' */) && !is_end) {
            state->state = 242;
        }
        else {
            state->state = 243;

            // action <nmfu.AppendTo object at 0x7f63578120b8> 
            if (state->multipart_boundary_counter == 70) state->state = 244;
            else {
                state->c.multipart_boundary[state->multipart_boundary_counter++] = (char)(inval);
                state->c.multipart_boundary[state->multipart_boundary_counter] = 0;
            }
        }
        return HTTP_SERVE_OK;
    case 244:
        // transitions
        if ((inval == 13 /* '\r' */) && !is_end) {
            state->state = 247;
        }
        else {
            state->state = 248;
        }
        return HTTP_SERVE_OK;
    case 245:
        // transitions
        if ((inval == 10 /* '\n' */) && !is_end) {
            // terminating state

            // action <nmfu.SetTo object at 0x7f6357812390> 
            state->c.error_code = HTTP_SERVE_ERROR_CODE_BAD_REQUEST;

            // action <nmfu.FinishAction object at 0x7f63577f5d68> 
            return HTTP_SERVE_DONE;
        }
        else {
            state->state = 248;
        }
        return HTTP_SERVE_OK;
    case 246:
        // transitions
        if ((inval == 13 /* '\r' */) && !is_end) {
            state->state = 245;
        }
        else {
            state->state = 248;
        }
        return HTTP_SERVE_OK;
    case 247:
        // transitions
        if ((inval == 10 /* '\n' */) && !is_end) {
            state->state = 246;
        }
        else {
            state->state = 248;
        }
        return HTTP_SERVE_OK;
    case 248:
        // transitions
        if ((inval == 13 /* '\r' */) && !is_end) {
            state->state = 247;
        }
        else {
            state->state = 248;
        }
        return HTTP_SERVE_OK;
    case 249:
        // transitions
        if ((inval == 34 /* '"' */) && !is_end) {
            state->state = 240;
        }
        else {
            state->state = 241;

            // action <nmfu.AppendCharTo object at 0x7f63578122e8> 
            if (state->multipart_boundary_counter == 70) state->state = 244;
            else {
                state->c.multipart_boundary[state->multipart_boundary_counter++] = (char)((int32_t)(inval));
                state->c.multipart_boundary[state->multipart_boundary_counter] = 0;
            }
        }
        return HTTP_SERVE_OK;
    case 250:
        // transitions
        if ((inval == 61 /* '=' */) && !is_end) {
            state->state = 249;
        }
        else {
            state->state = 272;
        }
        return HTTP_SERVE_OK;
    case 251:
        // transitions
        if ((inval == 121 /* 'y' */) && !is_end) {
            state->state = 250;
        }
        else {
            state->state = 272;
        }
        return HTTP_SERVE_OK;
    case 252:
        // transitions
        if ((inval == 114 /* 'r' */) && !is_end) {
            state->state = 251;
        }
        else {
            state->state = 272;
        }
        return HTTP_SERVE_OK;
    case 253:
        // transitions
        if ((inval == 97 /* 'a' */) && !is_end) {
            state->state = 252;
        }
        else {
            state->state = 272;
        }
        return HTTP_SERVE_OK;
    case 254:
        // transitions
        if ((inval == 100 /* 'd' */) && !is_end) {
            state->state = 253;
        }
        else {
            state->state = 272;
        }
        return HTTP_SERVE_OK;
    case 255:
        // transitions
        if ((inval == 110 /* 'n' */) && !is_end) {
            state->state = 254;
        }
        else {
            state->state = 272;
        }
        return HTTP_SERVE_OK;
    case 256:
        // transitions
        if ((inval == 117 /* 'u' */) && !is_end) {
            state->state = 255;
        }
        else {
            state->state = 272;
        }
        return HTTP_SERVE_OK;
    case 257:
        // transitions
        if ((inval == 111 /* 'o' */) && !is_end) {
            state->state = 256;
        }
        else {
            state->state = 272;
        }
        return HTTP_SERVE_OK;
    case 258:
        // transitions
        if ((inval == 99 /* 'c' */) && !is_end) {
            state->state = 229;
        }
        else if ((inval == 98 /* 'b' */) && !is_end) {
            state->state = 257;
        }
        else {
            state->state = 272;
        }
        return HTTP_SERVE_OK;
    case 259:
        // transitions
        if ((inval == 10 /* '\n' */) && !is_end) {
            state->state = 34;
        }
        else {
            state->state = 260;
        }
        return HTTP_SERVE_OK;
    case 260:
        // transitions
        if ((inval == 13 /* '\r' */) && !is_end) {
            state->state = 259;
        }
        else {
            state->state = 260;
        }
        return HTTP_SERVE_OK;
    case 261:
        // transitions
        if ((inval == 109 /* 'm' */) && !is_end) {
            state->state = 182;
        }
        else if ((inval == 116 /* 't' */) && !is_end) {
            state->state = 183;
        }
        else if ((inval == 97 /* 'a' */) && !is_end) {
            state->state = 184;
        }
        else {
            state->state = 228;

            // action <nmfu.SetTo object at 0x7f6357442b00> 
            state->c.content_type = HTTP_SERVE_CONTENT_TYPE_OTHER;
        }
        return HTTP_SERVE_OK;
    case 262:
        // transitions
        if ((inval == 99 /* 'c' */ || inval == 67 /* 'C' */) && !is_end) {
            state->state = 35;
        }
        else if ((inval == 13 /* '\r' */) && !is_end) {
            state->state = 36;
        }
        else if ((inval == 116 /* 't' */ || inval == 84 /* 'T' */) && !is_end) {
            state->state = 37;
        }
        else if ((inval == 65 /* 'A' */ || inval == 97 /* 'a' */) && !is_end) {
            state->state = 38;
        }
        else if ((inval == 105 /* 'i' */ || inval == 73 /* 'I' */) && !is_end) {
            state->state = 39;
        }
        else {
            state->state = 117;
        }
        return HTTP_SERVE_OK;
    case 263:
        // transitions
        if ((inval == 10 /* '\n' */) && !is_end) {
            state->state = 262;
        }
        else {
            state->state = 272;
        }
        return HTTP_SERVE_OK;
    case 264:
        // transitions
        if ((inval == 49 /* '1' */) && !is_end) {
            state->state = 26;
        }
        else {
            state->state = 30;
        }
        return HTTP_SERVE_OK;
    case 265:
        // transitions
        if ((inval == 47 /* '/' */) && !is_end) {
            state->state = 264;
        }
        else {
            state->state = 272;
        }
        return HTTP_SERVE_OK;
    case 266:
        // transitions
        if ((inval == 80 /* 'P' */) && !is_end) {
            state->state = 265;
        }
        else {
            state->state = 272;
        }
        return HTTP_SERVE_OK;
    case 267:
        // transitions
        if ((inval == 84 /* 'T' */) && !is_end) {
            state->state = 266;
        }
        else {
            state->state = 272;
        }
        return HTTP_SERVE_OK;
    case 268:
        // transitions
        if ((inval == 84 /* 'T' */) && !is_end) {
            state->state = 267;
        }
        else {
            state->state = 272;
        }
        return HTTP_SERVE_OK;
    case 269:
        // transitions
        if ((inval == 72 /* 'H' */) && !is_end) {
            state->state = 268;
        }
        else {
            state->state = 272;
        }
        return HTTP_SERVE_OK;
    case 270:
        // transitions
        if (((45 <= inval && inval <= 57 /* '-' - '9' */) || (65 <= inval && inval <= 90 /* 'A' - 'Z' */) || (97 <= inval && inval <= 122 /* 'a' - 'z' */) || inval == 35 /* '#' */ || inval == 38 /* '&' */ || inval == 63 /* '?' */ || inval == 95 /* '_' */) && !is_end) {
            state->state = 20;

            // action <nmfu.AppendTo object at 0x7f635761b828> 
            if (state->url_counter == 39) state->state = 21;
            else {
                state->c.url[state->url_counter++] = (char)(inval);
                state->c.url[state->url_counter] = 0;
            }
        }
        else if ((inval == 32 /* ' ' */) && !is_end) {
            state->state = 269;
        }
        else {
            state->state = 272;
        }
        return HTTP_SERVE_OK;
    case 271:
        // transitions
        if ((inval == 47 /* '/' */) && !is_end) {
            state->state = 270;
        }
        else {
            state->state = 272;
        }
        return HTTP_SERVE_OK;
    case 272:
        // transitions
        if ((inval == 13 /* '\r' */) && !is_end) {
            state->state = 275;
        }
        else {
            state->state = 276;
        }
        return HTTP_SERVE_OK;
    case 273:
        // transitions
        if ((inval == 10 /* '\n' */) && !is_end) {
            // terminating state

            // action <nmfu.SetTo object at 0x7f63576b2b00> 
            state->c.error_code = HTTP_SERVE_ERROR_CODE_BAD_REQUEST;

            // action <nmfu.FinishAction object at 0x7f635781a160> 
            return HTTP_SERVE_DONE;
        }
        else {
            state->state = 276;
        }
        return HTTP_SERVE_OK;
    case 274:
        // transitions
        if ((inval == 13 /* '\r' */) && !is_end) {
            state->state = 273;
        }
        else {
            state->state = 276;
        }
        return HTTP_SERVE_OK;
    case 275:
        // transitions
        if ((inval == 10 /* '\n' */) && !is_end) {
            state->state = 274;
        }
        else {
            state->state = 276;
        }
        return HTTP_SERVE_OK;
    case 276:
        // transitions
        if ((inval == 13 /* '\r' */) && !is_end) {
            state->state = 275;
        }
        else {
            state->state = 276;
        }
        return HTTP_SERVE_OK;
    default: return HTTP_SERVE_FAIL;
    }
}
