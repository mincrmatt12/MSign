// ============================================
// source file for nmfu parser multipart_header
// ============================================
#include "multipart_header.h"
#include <string.h>

multipart_header_result_t multipart_header_start(multipart_header_state_t *state) {
    // initialize append counter for name
    state->name_counter = 0;
    // initialize append counter for filename
    state->filename_counter = 0;
    // initialize default for content_length
    state->c.content_length = -1;
    // initialize default for ok
    state->c.ok = false;
    // initialize default for is_end
    state->c.is_end = false;
    // set starting state
    state->state = 0;
    return MULTIPART_HEADER_OK;
}
multipart_header_result_t multipart_header_feed(uint8_t inval, bool is_end, multipart_header_state_t *state) {
    switch (state->state) {
    case 0:
        // transitions
        if ((inval == 45 /* '-' */) && !is_end) {
            state->state = 1;
        }
        else if ((inval == 13 /* '\r' */) && !is_end) {
            state->state = 71;
        }
        else {
            state->state = 72;
        }
        return MULTIPART_HEADER_OK;
    case 1:
        // transitions
        if ((inval == 45 /* '-' */) && !is_end) {
            // terminating state

            // action <nmfu.SetTo object at 0x7feb424559b0> 
            state->c.is_end = true;

            // action <nmfu.FinishAction object at 0x7feb42455908> 
            return MULTIPART_HEADER_DONE;
        }
        else {
            state->state = 73;
        }
        return MULTIPART_HEADER_OK;
    case 2:
        // transitions
        if ((inval == 13 /* '\r' */) && !is_end) {
            state->state = 3;
        }
        else if ((inval == 67 /* 'C' */ || inval == 99 /* 'c' */) && !is_end) {
            state->state = 4;
        }
        else {
            state->state = 31;
        }
        return MULTIPART_HEADER_OK;
    case 3:
        // transitions
        if ((inval == 10 /* '\n' */) && !is_end) {
            state->state = 2;

            // action <nmfu.FinishAction object at 0x7feb42455828> 
            return MULTIPART_HEADER_DONE;
        }
        else {
            state->state = 31;
        }
        return MULTIPART_HEADER_OK;
    case 4:
        // transitions
        if ((inval == 111 /* 'o' */ || inval == 79 /* 'O' */) && !is_end) {
            state->state = 5;
        }
        else {
            state->state = 31;
        }
        return MULTIPART_HEADER_OK;
    case 5:
        // transitions
        if ((inval == 78 /* 'N' */ || inval == 110 /* 'n' */) && !is_end) {
            state->state = 6;
        }
        else {
            state->state = 31;
        }
        return MULTIPART_HEADER_OK;
    case 6:
        // transitions
        if ((inval == 84 /* 'T' */ || inval == 116 /* 't' */) && !is_end) {
            state->state = 7;
        }
        else {
            state->state = 31;
        }
        return MULTIPART_HEADER_OK;
    case 7:
        // transitions
        if ((inval == 101 /* 'e' */ || inval == 69 /* 'E' */) && !is_end) {
            state->state = 8;
        }
        else {
            state->state = 31;
        }
        return MULTIPART_HEADER_OK;
    case 8:
        // transitions
        if ((inval == 78 /* 'N' */ || inval == 110 /* 'n' */) && !is_end) {
            state->state = 9;
        }
        else {
            state->state = 31;
        }
        return MULTIPART_HEADER_OK;
    case 9:
        // transitions
        if ((inval == 84 /* 'T' */ || inval == 116 /* 't' */) && !is_end) {
            state->state = 10;
        }
        else {
            state->state = 31;
        }
        return MULTIPART_HEADER_OK;
    case 10:
        // transitions
        if ((inval == 45 /* '-' */) && !is_end) {
            state->state = 11;
        }
        else {
            state->state = 31;
        }
        return MULTIPART_HEADER_OK;
    case 11:
        // transitions
        if ((inval == 108 /* 'l' */ || inval == 76 /* 'L' */) && !is_end) {
            state->state = 12;
        }
        else if ((inval == 68 /* 'D' */ || inval == 100 /* 'd' */) && !is_end) {
            state->state = 13;
        }
        else {
            state->state = 31;
        }
        return MULTIPART_HEADER_OK;
    case 12:
        // transitions
        if ((inval == 101 /* 'e' */ || inval == 69 /* 'E' */) && !is_end) {
            state->state = 14;
        }
        else {
            state->state = 31;
        }
        return MULTIPART_HEADER_OK;
    case 13:
        // transitions
        if ((inval == 73 /* 'I' */ || inval == 105 /* 'i' */) && !is_end) {
            state->state = 15;
        }
        else {
            state->state = 31;
        }
        return MULTIPART_HEADER_OK;
    case 14:
        // transitions
        if ((inval == 78 /* 'N' */ || inval == 110 /* 'n' */) && !is_end) {
            state->state = 16;
        }
        else {
            state->state = 31;
        }
        return MULTIPART_HEADER_OK;
    case 15:
        // transitions
        if ((inval == 83 /* 'S' */ || inval == 115 /* 's' */) && !is_end) {
            state->state = 17;
        }
        else {
            state->state = 31;
        }
        return MULTIPART_HEADER_OK;
    case 16:
        // transitions
        if ((inval == 71 /* 'G' */ || inval == 103 /* 'g' */) && !is_end) {
            state->state = 18;
        }
        else {
            state->state = 31;
        }
        return MULTIPART_HEADER_OK;
    case 17:
        // transitions
        if ((inval == 112 /* 'p' */ || inval == 80 /* 'P' */) && !is_end) {
            state->state = 19;
        }
        else {
            state->state = 31;
        }
        return MULTIPART_HEADER_OK;
    case 18:
        // transitions
        if ((inval == 84 /* 'T' */ || inval == 116 /* 't' */) && !is_end) {
            state->state = 20;
        }
        else {
            state->state = 31;
        }
        return MULTIPART_HEADER_OK;
    case 19:
        // transitions
        if ((inval == 111 /* 'o' */ || inval == 79 /* 'O' */) && !is_end) {
            state->state = 21;
        }
        else {
            state->state = 31;
        }
        return MULTIPART_HEADER_OK;
    case 20:
        // transitions
        if ((inval == 72 /* 'H' */ || inval == 104 /* 'h' */) && !is_end) {
            state->state = 22;
        }
        else {
            state->state = 31;
        }
        return MULTIPART_HEADER_OK;
    case 21:
        // transitions
        if ((inval == 83 /* 'S' */ || inval == 115 /* 's' */) && !is_end) {
            state->state = 23;
        }
        else {
            state->state = 31;
        }
        return MULTIPART_HEADER_OK;
    case 22:
        // transitions
        if ((inval == 58 /* ':' */) && !is_end) {
            state->state = 24;
        }
        else {
            state->state = 31;
        }
        return MULTIPART_HEADER_OK;
    case 23:
        // transitions
        if ((inval == 73 /* 'I' */ || inval == 105 /* 'i' */) && !is_end) {
            state->state = 25;
        }
        else {
            state->state = 31;
        }
        return MULTIPART_HEADER_OK;
    case 24:
        // transitions
        if ((inval == 32 /* ' ' */) && !is_end) {
            state->state = 35;
        }
        else if ((inval == 13 /* '\r' */) && !is_end) {
            state->state = 34;

            // action <nmfu.SetTo object at 0x7feb424555c0> 
            state->c.content_length = 0;
        }
        else if (((48 <= inval && inval <= 57 /* '0' - '9' */)) && !is_end) {
            state->state = 33;

            // action <nmfu.SetTo object at 0x7feb424555c0> 
            state->c.content_length = 0;

            // action <nmfu.SetTo object at 0x7feb42455588> 
            state->c.content_length = ((state->c.content_length) * (10)) + (((int32_t)(inval)) - (48));
        }
        else {
            state->state = 73;
        }
        return MULTIPART_HEADER_OK;
    case 25:
        // transitions
        if ((inval == 84 /* 'T' */ || inval == 116 /* 't' */) && !is_end) {
            state->state = 26;
        }
        else {
            state->state = 31;
        }
        return MULTIPART_HEADER_OK;
    case 26:
        // transitions
        if ((inval == 73 /* 'I' */ || inval == 105 /* 'i' */) && !is_end) {
            state->state = 27;
        }
        else {
            state->state = 31;
        }
        return MULTIPART_HEADER_OK;
    case 27:
        // transitions
        if ((inval == 111 /* 'o' */ || inval == 79 /* 'O' */) && !is_end) {
            state->state = 28;
        }
        else {
            state->state = 31;
        }
        return MULTIPART_HEADER_OK;
    case 28:
        // transitions
        if ((inval == 78 /* 'N' */ || inval == 110 /* 'n' */) && !is_end) {
            state->state = 29;
        }
        else {
            state->state = 31;
        }
        return MULTIPART_HEADER_OK;
    case 29:
        // transitions
        if ((inval == 58 /* ':' */) && !is_end) {
            state->state = 30;
        }
        else {
            state->state = 31;
        }
        return MULTIPART_HEADER_OK;
    case 30:
        // transitions
        if ((inval == 32 /* ' ' */) && !is_end) {
            state->state = 69;
        }
        else if ((inval == 102 /* 'f' */) && !is_end) {
            state->state = 36;

            // action <nmfu.SetTo object at 0x7feb424550f0> 
            state->c.ok = true;
        }
        else {
            state->state = 73;
        }
        return MULTIPART_HEADER_OK;
    case 31:
        // transitions
        if ((inval == 13 /* '\r' */) && !is_end) {
            state->state = 32;
        }
        else {
            state->state = 31;
        }
        return MULTIPART_HEADER_OK;
    case 32:
        // transitions
        if ((inval == 10 /* '\n' */) && !is_end) {
            state->state = 2;
        }
        else {
            state->state = 31;
        }
        return MULTIPART_HEADER_OK;
    case 33:
        // transitions
        if ((inval == 13 /* '\r' */) && !is_end) {
            state->state = 34;
        }
        else if (((48 <= inval && inval <= 57 /* '0' - '9' */)) && !is_end) {
            state->state = 33;

            // action <nmfu.SetTo object at 0x7feb42455588> 
            state->c.content_length = ((state->c.content_length) * (10)) + (((int32_t)(inval)) - (48));
        }
        else {
            state->state = 73;
        }
        return MULTIPART_HEADER_OK;
    case 34:
        // transitions
        if ((inval == 10 /* '\n' */) && !is_end) {
            state->state = 2;
        }
        else {
            state->state = 73;
        }
        return MULTIPART_HEADER_OK;
    case 35:
        // transitions
        if ((inval == 13 /* '\r' */) && !is_end) {
            state->state = 34;

            // action <nmfu.SetTo object at 0x7feb424555c0> 
            state->c.content_length = 0;
        }
        else if (((48 <= inval && inval <= 57 /* '0' - '9' */)) && !is_end) {
            state->state = 33;

            // action <nmfu.SetTo object at 0x7feb424555c0> 
            state->c.content_length = 0;

            // action <nmfu.SetTo object at 0x7feb42455588> 
            state->c.content_length = ((state->c.content_length) * (10)) + (((int32_t)(inval)) - (48));
        }
        else {
            state->state = 73;

            // action <nmfu.SetTo object at 0x7feb424555c0> 
            state->c.content_length = 0;
        }
        return MULTIPART_HEADER_OK;
    case 36:
        // transitions
        if ((inval == 111 /* 'o' */) && !is_end) {
            state->state = 37;
        }
        else {
            state->state = 73;
        }
        return MULTIPART_HEADER_OK;
    case 37:
        // transitions
        if ((inval == 114 /* 'r' */) && !is_end) {
            state->state = 38;
        }
        else {
            state->state = 73;
        }
        return MULTIPART_HEADER_OK;
    case 38:
        // transitions
        if ((inval == 109 /* 'm' */) && !is_end) {
            state->state = 39;
        }
        else {
            state->state = 73;
        }
        return MULTIPART_HEADER_OK;
    case 39:
        // transitions
        if ((inval == 45 /* '-' */) && !is_end) {
            state->state = 40;
        }
        else {
            state->state = 73;
        }
        return MULTIPART_HEADER_OK;
    case 40:
        // transitions
        if ((inval == 100 /* 'd' */) && !is_end) {
            state->state = 41;
        }
        else {
            state->state = 73;
        }
        return MULTIPART_HEADER_OK;
    case 41:
        // transitions
        if ((inval == 97 /* 'a' */) && !is_end) {
            state->state = 42;
        }
        else {
            state->state = 73;
        }
        return MULTIPART_HEADER_OK;
    case 42:
        // transitions
        if ((inval == 116 /* 't' */) && !is_end) {
            state->state = 43;
        }
        else {
            state->state = 73;
        }
        return MULTIPART_HEADER_OK;
    case 43:
        // transitions
        if ((inval == 97 /* 'a' */) && !is_end) {
            state->state = 44;
        }
        else {
            state->state = 73;
        }
        return MULTIPART_HEADER_OK;
    case 44:
        // transitions
        if ((inval == 59 /* ';' */) && !is_end) {
            state->state = 45;
        }
        else {
            state->state = 73;
        }
        return MULTIPART_HEADER_OK;
    case 45:
        // transitions
        if ((inval == 32 /* ' ' */) && !is_end) {
            state->state = 67;
        }
        else if ((inval == 102 /* 'f' */) && !is_end) {
            state->state = 46;
        }
        else if ((inval == 110 /* 'n' */) && !is_end) {
            state->state = 47;
        }
        else {
            state->state = 73;
        }
        return MULTIPART_HEADER_OK;
    case 46:
        // transitions
        if ((inval == 105 /* 'i' */) && !is_end) {
            state->state = 48;
        }
        else {
            state->state = 73;
        }
        return MULTIPART_HEADER_OK;
    case 47:
        // transitions
        if ((inval == 97 /* 'a' */) && !is_end) {
            state->state = 49;
        }
        else {
            state->state = 73;
        }
        return MULTIPART_HEADER_OK;
    case 48:
        // transitions
        if ((inval == 108 /* 'l' */) && !is_end) {
            state->state = 50;
        }
        else {
            state->state = 73;
        }
        return MULTIPART_HEADER_OK;
    case 49:
        // transitions
        if ((inval == 109 /* 'm' */) && !is_end) {
            state->state = 51;
        }
        else {
            state->state = 73;
        }
        return MULTIPART_HEADER_OK;
    case 50:
        // transitions
        if ((inval == 101 /* 'e' */) && !is_end) {
            state->state = 52;
        }
        else {
            state->state = 73;
        }
        return MULTIPART_HEADER_OK;
    case 51:
        // transitions
        if ((inval == 101 /* 'e' */) && !is_end) {
            state->state = 53;
        }
        else {
            state->state = 73;
        }
        return MULTIPART_HEADER_OK;
    case 52:
        // transitions
        if ((inval == 110 /* 'n' */) && !is_end) {
            state->state = 54;
        }
        else {
            state->state = 73;
        }
        return MULTIPART_HEADER_OK;
    case 53:
        // transitions
        if ((inval == 61 /* '=' */) && !is_end) {
            state->state = 55;
        }
        else {
            state->state = 73;
        }
        return MULTIPART_HEADER_OK;
    case 54:
        // transitions
        if ((inval == 97 /* 'a' */) && !is_end) {
            state->state = 56;
        }
        else {
            state->state = 73;
        }
        return MULTIPART_HEADER_OK;
    case 55:
        // transitions
        if ((inval == 34 /* '"' */) && !is_end) {
            state->state = 65;
        }
        else {
            state->state = 73;
        }
        return MULTIPART_HEADER_OK;
    case 56:
        // transitions
        if ((inval == 109 /* 'm' */) && !is_end) {
            state->state = 57;
        }
        else {
            state->state = 73;
        }
        return MULTIPART_HEADER_OK;
    case 57:
        // transitions
        if ((inval == 101 /* 'e' */) && !is_end) {
            state->state = 58;
        }
        else {
            state->state = 73;
        }
        return MULTIPART_HEADER_OK;
    case 58:
        // transitions
        if ((inval == 61 /* '=' */) && !is_end) {
            state->state = 59;
        }
        else {
            state->state = 73;
        }
        return MULTIPART_HEADER_OK;
    case 59:
        // transitions
        if ((inval == 34 /* '"' */) && !is_end) {
            state->state = 62;
        }
        else {
            state->state = 73;
        }
        return MULTIPART_HEADER_OK;
    case 60:
        // transitions
        if (is_end) {
            state->state = 73;
        }
        else if ((inval == 34 /* '"' */) && !is_end) {
            state->state = 61;
        }
        else {
            state->state = 60;

            // action <nmfu.AppendTo object at 0x7feb42456d68> 
            if (state->filename_counter == 31) state->state = 73;
            else {
                state->c.filename[state->filename_counter++] = (char)(inval);
                state->c.filename[state->filename_counter] = 0;
            }
        }
        return MULTIPART_HEADER_OK;
    case 61:
        // transitions
        if ((inval == 59 /* ';' */) && !is_end) {
            state->state = 68;
        }
        else if ((inval == 13 /* '\r' */) && !is_end) {
            state->state = 66;
        }
        else {
            state->state = 73;
        }
        return MULTIPART_HEADER_OK;
    case 62:
        // transitions
        if (is_end || inval == 34 /* '"' */) {
            state->state = 73;
        }
        else {
            state->state = 60;

            // action <nmfu.AppendTo object at 0x7feb42456d68> 
            if (state->filename_counter == 31) state->state = 73;
            else {
                state->c.filename[state->filename_counter++] = (char)(inval);
                state->c.filename[state->filename_counter] = 0;
            }
        }
        return MULTIPART_HEADER_OK;
    case 63:
        // transitions
        if (is_end) {
            state->state = 73;
        }
        else if ((inval == 34 /* '"' */) && !is_end) {
            state->state = 64;
        }
        else {
            state->state = 63;

            // action <nmfu.AppendTo object at 0x7feb42456978> 
            if (state->name_counter == 31) state->state = 73;
            else {
                state->c.name[state->name_counter++] = (char)(inval);
                state->c.name[state->name_counter] = 0;
            }
        }
        return MULTIPART_HEADER_OK;
    case 64:
        // transitions
        if ((inval == 59 /* ';' */) && !is_end) {
            state->state = 68;
        }
        else if ((inval == 13 /* '\r' */) && !is_end) {
            state->state = 66;
        }
        else {
            state->state = 73;
        }
        return MULTIPART_HEADER_OK;
    case 65:
        // transitions
        if (is_end || inval == 34 /* '"' */) {
            state->state = 73;
        }
        else {
            state->state = 63;

            // action <nmfu.AppendTo object at 0x7feb42456978> 
            if (state->name_counter == 31) state->state = 73;
            else {
                state->c.name[state->name_counter++] = (char)(inval);
                state->c.name[state->name_counter] = 0;
            }
        }
        return MULTIPART_HEADER_OK;
    case 66:
        // transitions
        if ((inval == 10 /* '\n' */) && !is_end) {
            state->state = 2;
        }
        else {
            state->state = 73;
        }
        return MULTIPART_HEADER_OK;
    case 67:
        // transitions
        if ((inval == 102 /* 'f' */) && !is_end) {
            state->state = 46;
        }
        else if ((inval == 110 /* 'n' */) && !is_end) {
            state->state = 47;
        }
        else {
            state->state = 73;
        }
        return MULTIPART_HEADER_OK;
    case 68:
        // transitions
        if ((inval == 32 /* ' ' */) && !is_end) {
            state->state = 67;
        }
        else if ((inval == 102 /* 'f' */) && !is_end) {
            state->state = 46;
        }
        else if ((inval == 110 /* 'n' */) && !is_end) {
            state->state = 47;
        }
        else {
            state->state = 73;
        }
        return MULTIPART_HEADER_OK;
    case 69:
        // transitions
        if ((inval == 102 /* 'f' */) && !is_end) {
            state->state = 36;

            // action <nmfu.SetTo object at 0x7feb424550f0> 
            state->c.ok = true;
        }
        else {
            state->state = 73;

            // action <nmfu.SetTo object at 0x7feb424550f0> 
            state->c.ok = true;
        }
        return MULTIPART_HEADER_OK;
    case 70:
        // transitions
        if ((inval == 13 /* '\r' */) && !is_end) {
            state->state = 3;
        }
        else if ((inval == 67 /* 'C' */ || inval == 99 /* 'c' */) && !is_end) {
            state->state = 4;
        }
        else {
            state->state = 31;
        }
        return MULTIPART_HEADER_OK;
    case 71:
        // transitions
        if ((inval == 10 /* '\n' */) && !is_end) {
            state->state = 70;
        }
        else {
            state->state = 72;
        }
        return MULTIPART_HEADER_OK;
    case 72:
        // transitions
        if ((inval == 13 /* '\r' */) && !is_end) {
            state->state = 71;
        }
        else {
            state->state = 72;
        }
        return MULTIPART_HEADER_OK;
    case 73:
        return MULTIPART_HEADER_FAIL;
    default: return MULTIPART_HEADER_FAIL;
    }
}
