#include "Engine/Tables/AutonoteTable.h"
#include "Engine/LOD.h"

#include "Utility/String.h"

std::array<Autonote, 196> pAutonoteTxt;
std::string pAutonoteTXT_Raw;

void initializeAutonotes() {
    int i;
    char *test_string;
    unsigned char c;
    bool break_loop;
    unsigned int temp_str_len;
    char *tmp_pos;
    int decode_step;

    pAutonoteTXT_Raw = pEvents_LOD->LoadCompressedTexture("autonote.txt").string_view();
    strtok(pAutonoteTXT_Raw.data(), "\r");

    for (i = 0; i < 195; ++i) {
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
                switch (decode_step) {
                    case 1:
                        pAutonoteTxt[i + 1].pText = removeQuotes(test_string);
                        break;
                    case 2: {
                        if (iequals(test_string, "potion")) {
                            pAutonoteTxt[i + 1].eType = AUTONOTE_POTION_RECEPIE;
                            break;
                        }
                        if (iequals(test_string, "stat")) {
                            pAutonoteTxt[i + 1].eType = AUTONOTE_STAT_HINT;
                            break;
                        }
                        if (iequals(test_string, "seer")) {
                            pAutonoteTxt[i + 1].eType = AUTONOTE_SEER;
                            break;
                        }
                        if (iequals(test_string, "obelisk")) {
                            pAutonoteTxt[i + 1].eType = AUTONOTE_OBELISK;
                            break;
                        }
                        if (iequals(test_string, "teacher")) {
                            pAutonoteTxt[i + 1].eType = AUTONOTE_TEACHER;
                            break;
                        }
                        pAutonoteTxt[i + 1].eType = AUTONOTE_MISC;
                        break;
                    }
                }
            } else {
                break_loop = true;
            }
            ++decode_step;
            test_string = tmp_pos + 1;
        } while ((decode_step < 3) && !break_loop);
    }
}

