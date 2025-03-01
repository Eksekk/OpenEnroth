#include "GUI/UI/Books/CalendarBook.h"

#include <string>

#include "Engine/Engine.h"
#include "Engine/LOD.h"
#include "Engine/Localization.h"
#include "Engine/Party.h"
#include "Engine/Time.h"

#include "Engine/Graphics/IRender.h"
#include "Engine/Graphics/Viewport.h"

#include "GUI/GUIButton.h"
#include "GUI/GUIFont.h"

#include "Media/Audio/AudioPlayer.h"

Image *ui_book_calendar_background = nullptr;

Image *ui_book_calendar_moon_new = nullptr;
Image *ui_book_calendar_moon_4 = nullptr;
Image *ui_book_calendar_moon_2 = nullptr;
Image *ui_book_calendar_moon_2_2 = nullptr;
Image *ui_book_calendar_moon_full = nullptr;

// 4E1B18
static std::array<int, 28> pDayMoonPhase = {
        0, 0, 0,
        1, 1, 1, 1,
        2, 2, 2,
        3, 3, 3, 3,
        4, 4, 4,
        3, 3, 3, 3,
        2, 2, 2,
        1, 1, 1, 1
};


GUIWindow_CalendarBook::GUIWindow_CalendarBook() : GUIWindow_Book() {
    this->wData.val = WINDOW_CalendarBook;  // inherited from GUIWindow::GUIWindow
    this->eWindowType = WindowType::WINDOW_CalendarBook;

    // --------------------------------
    // 004304E7 Game_EventLoop --- part
    pChildBooksOverlay = new GUIWindow_BooksButtonOverlay({570, 354}, {0, 0}, pBtn_Calendar);

    // ----------------------------------------------
    // 00411BFC GUIWindow::InitializeBookView -- part
    ui_book_calendar_background = assets->getImage_ColorKey("sbdate-time");
    ui_book_calendar_moon_new = assets->getImage_ColorKey("moon_new");
    ui_book_calendar_moon_4 = assets->getImage_ColorKey("moon_4");
    ui_book_calendar_moon_2 = assets->getImage_ColorKey("moon_2");
    ui_book_calendar_moon_2_2 = assets->getImage_ColorKey("moon_2");
    ui_book_calendar_moon_full = assets->getImage_ColorKey("moon_ful");
}

//----- (00413D3C) --------------------------------------------------------
static const char *GetDayPart() {
    if (pParty->uCurrentHour > 5 && pParty->uCurrentHour < 20) {
        return localization->GetString(LSTR_DAY_CAPITALIZED);
    } else if (pParty->uCurrentHour == 5) {
        return localization->GetString(LSTR_DAWN);
    } else if (pParty->uCurrentHour == 20) {
        return localization->GetString(LSTR_DUSK);
    } else {
        return localization->GetString(LSTR_NIGHT);
    }
}

void GUIWindow_CalendarBook::Update() {
    render->DrawTextureNew(471 / 640.0f, 445 / 480.0f, ui_exit_cancel_button_background);

    GUIWindow calendar_window;

    render->DrawTextureNew(pViewport->uViewportTL_X / 640.0f, pViewport->uViewportTL_Y / 480.0f, ui_book_calendar_background);
    int hour = pParty->uCurrentHour;
    int am;
    if (hour >= 12) {
        hour -= 12;
        if (!hour) {
            hour = 12;
        }
        am = 1;
    } else {
        am = 0;
    }

    calendar_window.uFrameX = game_viewport_x;
    calendar_window.uFrameY = game_viewport_y;
    calendar_window.uFrameWidth = game_viewport_width;
    calendar_window.uFrameHeight = game_viewport_height;
    calendar_window.uFrameZ = game_viewport_z;
    calendar_window.uFrameW = game_viewport_w;
    calendar_window.DrawTitleText(pBook2Font, 0, 22, ui_book_calendar_title_color, localization->GetString(LSTR_TIME_IN_ERATHIA), 3);

    std::string str = fmt::format("{}\t100:\t110{}:{:02} {} - {}", localization->GetString(LSTR_TIME), hour,
                                  pParty->uCurrentMinute, localization->GetAmPm(am), GetDayPart());
    calendar_window.DrawText(pBookFont, {70, 55}, ui_book_calendar_time_color, str, 0, 0, 0);

    str = fmt::format("{}\t100:\t110{} - {}", localization->GetString(LSTR_DAY_CAPITALIZED), pParty->uCurrentDayOfMonth + 1,
                      localization->GetDayName(pParty->uCurrentDayOfMonth % 7));
    calendar_window.DrawText(pBookFont, {70, 2 * pBookFont->GetHeight() + 49}, ui_book_calendar_day_color, str, 0, 0, 0);

    str = fmt::format("{}\t100:\t110{} - {}", localization->GetString(LSTR_MONTH), pParty->uCurrentMonth + 1,
                      localization->GetMonthName(pParty->uCurrentMonth));
    calendar_window.DrawText(pBookFont, {70, 4 * pBookFont->GetHeight() + 43}, ui_book_calendar_month_color, str, 0, 0, 0);

    str = fmt::format("{}\t100:\t110{}", localization->GetString(LSTR_YEAR), pParty->uCurrentYear);
    calendar_window.DrawText(pBookFont, {70, 6 * pBookFont->GetHeight() + 37}, ui_book_calendar_year_color, str, 0, 0, 0);

    str = fmt::format("{}\t100:\t110{}", localization->GetString(LSTR_MOON), localization->GetMoonPhaseName(pDayMoonPhase[pParty->uCurrentDayOfMonth]));
    calendar_window.DrawText(pBookFont, {70, 8 * (unsigned char)pBookFont->GetHeight() + 31}, ui_book_calendar_moon_color, str, 0, 0, 0);

    int pMapID = pMapStats->GetMapInfo(pCurrentMapName);
    std::string pMapName;
    if (pMapID) {
        pMapName = pMapStats->pInfos[pMapID].pName;
    } else {
        pMapName = "Unknown";
    }

    str = fmt::format("{}\t100:\t110{}", localization->GetString(LSTR_LOCATION), pMapName);
    calendar_window.DrawText(pBookFont, {70, 10 * (unsigned char)pBookFont->GetHeight() + 25}, ui_book_calendar_location_color, str, 0, 0, 0);
}
