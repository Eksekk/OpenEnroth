#include "Engine/Tables/MessageScrollTable.h"
#include "Engine/LOD.h"
#include "Utility/String.h"

IndexedArray<const char *, ITEM_FIRST_MESSAGE_SCROLL, ITEM_LAST_MESSAGE_SCROLL> pMessageScrolls;
std::string pMessageScrollsTXT_Raw;

void initializeMessageScrolls() {
    char *test_string;
    unsigned char c;
    bool break_loop;
    unsigned int temp_str_len;
    char *tmp_pos;
    int decode_step;

    pMessageScrollsTXT_Raw = pEvents_LOD->LoadCompressedTexture("scroll.txt").string_view();
    strtok(pMessageScrollsTXT_Raw.data(), "\r");
    for (ITEM_TYPE i : pMessageScrolls.indices()) {
        test_string = strtok(NULL, "\r") + 1;
        break_loop = false;
        decode_step = 0;
        do {
            c = *(unsigned char *)test_string;
            temp_str_len = 0;
            while ((c != '\t') && (c > 0)) {
                ++temp_str_len;
                c = test_string[temp_str_len];
            }
            tmp_pos = test_string + temp_str_len;
            if (*tmp_pos == 0) break_loop = true;
            *tmp_pos = 0;
            if (temp_str_len) {
                if (decode_step == 1) pMessageScrolls[i] = removeQuotes(test_string);
            } else {
                break_loop = true;
            }
            ++decode_step;
            test_string = tmp_pos + 1;
        } while ((decode_step < 2) && !break_loop);
    }
}

