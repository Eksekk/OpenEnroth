#include "GUI/GUIButton.h"

#include <vector>

#include "Engine/Engine.h"
#include "Engine/EngineGlobals.h"
#include "Engine/LOD.h"
#include "Engine/Localization.h"
#include "Engine/Graphics/IRender.h"

#include "GUI/GUIFont.h"
#include "GUI/GUIWindow.h"


GUIButton *pBtn_CloseBook;
GUIButton *pBtn_InstallRemoveSpell;
GUIButton *pBtn_Autonotes_Instructors;
GUIButton *pBtn_Autonotes_Misc;
GUIButton *pBtn_Book_6;
GUIButton *pBtn_Book_5;
GUIButton *pBtn_Book_4;
GUIButton *pBtn_Book_3;
GUIButton *pBtn_Book_2;
GUIButton *pBtn_Book_1;


GUIButton *pPlayerCreationUI_BtnReset;
GUIButton *pPlayerCreationUI_BtnOK;
GUIButton *pBtn_ExitCancel;
GUIButton *pBtn_YES;
GUIButton *pPlayerCreationUI_BtnPlus;
GUIButton *pPlayerCreationUI_BtnMinus;


GUIButton *pButton_RestUI_Main;
GUIButton *pButton_RestUI_Exit;
GUIButton *pButton_RestUI_Wait5Minutes;
GUIButton *pButton_RestUI_WaitUntilDawn;
GUIButton *pButton_RestUI_Wait1Hour;


GUIButton *pCharacterScreen_ExitBtn;
GUIButton *pCharacterScreen_AwardsBtn;
GUIButton *pCharacterScreen_InventoryBtn;
GUIButton *pCharacterScreen_SkillsBtn;
GUIButton *pCharacterScreen_StatsBtn;
GUIButton *pCharacterScreen_DollBtn;
GUIButton *pCharacterScreen_DetalizBtn;


GUIButton *pBtn_NPCRight;
GUIButton *pBtn_NPCLeft;
GUIButton *pBtn_GameSettings;
GUIButton *pBtn_QuickReference;
GUIButton *pBtn_CastSpell;
GUIButton *pBtn_Rest;
GUIButton *pBtn_History;
GUIButton *pBtn_Calendar;
GUIButton *pBtn_Maps;
GUIButton *pBtn_Autonotes;
GUIButton *pBtn_Quests;


GUIButton *pMMT_MainMenu_BtnMM6;
GUIButton *pMMT_MainMenu_BtnMM7;
GUIButton *pMMT_MainMenu_BtnMM8;
GUIButton *pMMT_MainMenu_BtnContinue;
GUIButton *pMMT_MainMenu_BtnExit;


GUIButton *pBtn_Up;
GUIButton *pBtn_Down;
GUIButton *pBtn_Scroll;

GUIButton *pBtn_Resume;
GUIButton *pBtn_QuitGame;
GUIButton *pBtn_GameControls;
GUIButton *pBtn_LoadGame;
GUIButton *pBtn_SaveGame;
GUIButton *pBtn_NewGame;

GUIButton *pBtn_SliderRight;
GUIButton *pBtn_SliderLeft;


GUIButton *pBtnDownArrow;
GUIButton *pBtnArrowUp;
GUIButton *pBtnCancel;
GUIButton *pBtnLoadSlot;

std::array<GUIButton *, 4> pCreationUI_BtnPressRight2;
std::array<GUIButton *, 4> pCreationUI_BtnPressLeft2;
std::array<GUIButton *, 4> pCreationUI_BtnPressLeft;
std::array<GUIButton *, 4> pCreationUI_BtnPressRight;

void GUIButton::Release() {
    std::vector<GUIButton *>::iterator it =
        std::find(pParent->vButtons.begin(), pParent->vButtons.end(), this);
    if (it != pParent->vButtons.end()) {
        pParent->vButtons.erase(it);
    }
    delete this;
}

void GUIButton::DrawLabel(const std::string &label_text, GUIFont *pFont, int a5, int uFontShadowColor) {
    return pParent->DrawText(pFont,
        {this->uX + (int)(this->uWidth - pFont->GetLineWidth(label_text)) / 2, this->uY + (int)(this->uHeight - pFont->GetHeight()) / 2}, a5,
        label_text, 0, 0, uFontShadowColor);
}

bool GUIButton::Contains(unsigned int x, unsigned int y) {
    return (x >= uX && x <= uZ && y >= uY && y <= uW);
}

void CreateShopDialogueButtonAtRow(int row, DIALOGUE_TYPE type) {
    pDialogueWindow->CreateButton({480, 146 + 30 * row}, {140, 30}, 1, 0, UIMSG_SelectShopDialogueOption, type, InputAction::Invalid, "");
}

void UI_CreateEndConversationButton() {
    pDialogueWindow->Release();
    pDialogueWindow = new GUIWindow(WINDOW_Dialogue, {0, 0}, {render->GetPresentDimensions().w, 345}, 0);
    pBtn_ExitCancel = pDialogueWindow->CreateButton({471, 445}, {169, 35}, 1, 0, UIMSG_Escape, 0, InputAction::Invalid,
        localization->GetString(LSTR_END_CONVERSATION), {ui_exit_cancel_button_background});
    pDialogueWindow->CreateButton({8, 8}, {450, 320}, 1, 0, UIMSG_BuyInShop_Identify_Repair, 0, InputAction::Invalid, "");
}
