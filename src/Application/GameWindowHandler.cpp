#include "GameWindowHandler.h"

#include <vector>

#include "Arcomage/Arcomage.h"

#include "Engine/Engine.h"
#include "Engine/EngineGlobals.h"
#include "Engine/Graphics/Camera.h"
#include "Engine/Graphics/Viewport.h"
#include "Engine/Graphics/Vis.h"
#include "Engine/Graphics/Nuklear.h"
#include "Engine/Graphics/NuklearEventHandler.h"
#include "Engine/IocContainer.h"
#include "Engine/Party.h"
#include "Engine/Time.h"

#include "GUI/GUIWindow.h"

#include "Io/InputAction.h"
#include "Io/KeyboardInputHandler.h"
#include "Io/Mouse.h"

#include "Media/Audio/AudioPlayer.h"
#include "Media/MediaPlayer.h"


#include "IocContainer.h"


using EngineIoc = Engine_::IocContainer;
using ApplicationIoc = Application::IocContainer;
using Application::GameWindowHandler;
using Io::InputAction;

static char PlatformKeyToChar(PlatformKey key, PlatformModifiers mods) {
    if (key >= PlatformKey::Digit0 && key <= PlatformKey::Digit9) {
        return std::to_underlying(key) - std::to_underlying(PlatformKey::Digit0) + '0';
    } else if (key >= PlatformKey::A && key <= PlatformKey::Z) {
        if (mods & PlatformModifier::Shift) {
            return std::to_underlying(key) - std::to_underlying(PlatformKey::A) + 'A';
        } else {
            return std::to_underlying(key) - std::to_underlying(PlatformKey::A) + 'a';
        }
    }

    return 0;
}


GameWindowHandler::GameWindowHandler() {
    this->mouse = EngineIoc::ResolveMouse();
    this->keyboardController_ = std::make_unique<GameKeyboardController>();
}

void GameWindowHandler::UpdateWindowFromConfig() {
    window->SetVisible(true);

    std::vector<Recti> displays = platform->DisplayGeometries();

    Pointi pos = {engine->config->window.PositionX.Get(), engine->config->window.PositionY.Get()};
    Sizei size = {engine->config->window.Width.Get(), engine->config->window.Height.Get()};

    int display = engine->config->window.Display.Get();
    if (display < 0 || display >= displays.size())
        display = 0;

    Recti displayRect = !displays.empty() ? displays[display] : Recti();

    if (engine->config->window.Fullscreen.Get()) {
        pos = displayRect.TopLeft();
    } else if (displayRect.Contains(pos)) {
        pos += displayRect.TopLeft();
    } else {
        pos = displayRect.TopLeft();
    }

    window->SetPosition(pos);
    window->Resize(size);
    window->SetTitle(engine->config->window.Title.Get());
    window->SetBorderless(engine->config->window.Borderless.Get());
    window->SetGrabsMouse(engine->config->window.MouseGrab.Get());
    window->SetFullscreen(engine->config->window.Fullscreen.Get());
    window->SetVisible(true);
}

void GameWindowHandler::UpdateConfigFromWindow() {
    std::vector<Recti> displays = platform->DisplayGeometries();

    Pointi pos = window->Position();
    Sizei size = window->Size();

    Pointi relativePos = Pointi();
    int display = 0;

    for (size_t i = 0; i < displays.size(); i++) {
        if (displays[i].Contains(pos)) {
            display = i;
            relativePos = pos - displays[i].TopLeft();
            break;
        }
    }

    engine->config->window.PositionX.Set(relativePos.x);
    engine->config->window.PositionY.Set(relativePos.y);
    engine->config->window.Width.Set(size.w);
    engine->config->window.Height.Set(size.h);
    engine->config->window.Borderless.Set(window->IsBorderless());
    engine->config->window.MouseGrab.Set(window->GrabsMouse());
    engine->config->window.Fullscreen.Set(window->IsFullscreen());
}

void GameWindowHandler::OnScreenshot() {
    if (render) {
        render->SavePCXScreenshot();
    }
}

bool GameWindowHandler::OnChar(PlatformKey key, int c) {
    bool textInputHandled = false;

    if (!keyboardInputHandler)
        return false;

    // backspace, enter, esc (text input), controls binding
    textInputHandled |= keyboardInputHandler->ProcessTextInput(key, c);

    // regular text input
    if (c != -1) {
        textInputHandled |= keyboardInputHandler->ProcessTextInput(PlatformKey::Char, c);
    }

    if (!textInputHandled && !viewparams->field_4C) {
        return GUI_HandleHotkey(key);  // try other hotkeys
    }
    return false;
}

void GameWindowHandler::OnMouseLeftClick(int x, int y) {
    if (pArcomageGame->bGameInProgress) {
        pArcomageGame->stru1.am_input_type = 7;
        pArcomageGame->check_exit = 0;
        pArcomageGame->force_redraw_1 = 1;
        ArcomageGame::OnMouseClick(0, true);
    } else {
        pMediaPlayer->StopMovie();

        mouse->SetMouseClick(x, y);

        if (GetCurrentMenuID() == MENU_CREATEPARTY) {
            UI_OnKeyDown(PlatformKey::Select);
        }

        if (engine) {
            engine->PickMouse(engine->config->gameplay.MouseInteractionDepth.Get(), x, y, false,
                              &vis_sprite_filter_3, &vis_door_filter);
        }

        mouse->UI_OnMouseLeftClick();
    }
}

void GameWindowHandler::OnMouseRightClick(int x, int y) {
    if (pArcomageGame->bGameInProgress) {
        pArcomageGame->stru1.am_input_type = 8;
        pArcomageGame->check_exit = 0;
        pArcomageGame->force_redraw_1 = 1;
        ArcomageGame::OnMouseClick(1, true);
    } else {
        pMediaPlayer->StopMovie();

        mouse->SetMouseClick(x, y);

        if (engine) {
            engine->PickMouse(pCamera3D->GetMouseInfoDepth(), x, y, 0, &vis_sprite_filter_2, &vis_door_filter);
        }

        UI_OnMouseRightClick(x, y);
    }
}

void GameWindowHandler::OnMouseLeftUp() {
    if (pArcomageGame->bGameInProgress) {
        pArcomageGame->stru1.am_input_type = 3;
        ArcomageGame::OnMouseClick(0, 0);
    } else {
        back_to_game();
    }
}

void GameWindowHandler::OnMouseRightUp() {
    if (pArcomageGame->bGameInProgress) {
        pArcomageGame->stru1.am_input_type = 4;
        ArcomageGame::OnMouseClick(1, false);
    } else {
        back_to_game();
    }
}

void GameWindowHandler::OnMouseLeftDoubleClick(int x, int y) {
    if (pArcomageGame->bGameInProgress) {
        pArcomageGame->stru1.am_input_type = 7;
    } else {
        OnMouseLeftClick(x, y);
    }
}

void GameWindowHandler::OnMouseRightDoubleClick(int x, int y) {
    if (pArcomageGame->bGameInProgress) {
        pArcomageGame->stru1.am_input_type = 8;
    } else {
        OnMouseRightClick(x, y);
    }
}

void GameWindowHandler::OnMouseMove(int x, int y, bool left_button, bool right_button) {
    if (pArcomageGame->bGameInProgress) {
        ArcomageGame::OnMouseMove(x, y);
        ArcomageGame::OnMouseClick(0, left_button);
        ArcomageGame::OnMouseClick(1, right_button);
    } else {
        if (mouse) {
            mouse->SetMouseClick(x, y);
        }
    }
}


extern InputAction currently_selected_action_for_binding;

void GameWindowHandler::OnKey(PlatformKey key) {
    if (!keyboardInputHandler)
        return;

    if (currently_selected_action_for_binding != InputAction::Invalid) {
        // we're setting a key binding in options
        keyboardInputHandler->ProcessTextInput(key, -1);
    } else if (pArcomageGame->bGameInProgress) {
        pArcomageGame->stru1.am_input_type = 1;

        set_stru1_field_8_InArcomage(0);
        if (key == PlatformKey::Escape) {
            pArcomageGame->stru1.am_input_type = 10;
        } else if (pArcomageGame->check_exit) {
           pArcomageGame->check_exit = 0;
           pArcomageGame->force_redraw_1 = 1;
        }

        if (key == PlatformKey::F3) {
            OnScreenshot();
        } else if (key == PlatformKey::F4 && !pMovie_Track) {
            OnToggleFullscreen();
            pArcomageGame->stru1.am_input_type = 9;
        }
    } else {
        pMediaPlayer->StopMovie();
        if (key == PlatformKey::Return) {
            if (!viewparams->field_4C) UI_OnKeyDown(key);
        } else if (key == PlatformKey::Escape) {
            pMessageQueue_50CBD0->AddGUIMessage(UIMSG_Escape, window_SpeakInHouse != 0, 0);
        } else if (key == PlatformKey::F4 && !pMovie_Track) {
            OnToggleFullscreen();
        } else if (key == PlatformKey::Tilde) {
            pMessageQueue_50CBD0->AddGUIMessage(UIMSG_OpenDebugMenu, window_SpeakInHouse != 0, 0);
        } else if (key == PlatformKey::Backspace && current_screen_type == CURRENT_SCREEN::SCREEN_GAME) {
            pMessageQueue_50CBD0->AddGUIMessage(UIMSG_DebugReloadShader, window_SpeakInHouse != 0, 0);
        } else if (key == PlatformKey::Left || key == PlatformKey::Right || key == PlatformKey::Up || key == PlatformKey::Down) {
            if (current_screen_type != CURRENT_SCREEN::SCREEN_GAME &&
                current_screen_type != CURRENT_SCREEN::SCREEN_MODAL_WINDOW) {
                if (!viewparams->field_4C) {
                    UI_OnKeyDown(key);
                }
            }
        }
    }
}

void GameWindowHandler::OnFocus() {
    __debugbreak();
}

void GameWindowHandler::OnFocusLost() {
    __debugbreak();
}

void GameWindowHandler::OnPaint() {
    if (pArcomageGame->bGameInProgress) {
        pArcomageGame->force_redraw_1 = 1;
    }
    if (render && render->AreRenderSurfacesOk()) {
        render->Present();
    }
}

void GameWindowHandler::OnActivated() {
    if (dword_6BE364_game_settings_1 & GAME_SETTINGS_APP_INACTIVE) {
        // dword_4E98BC_bApplicationActive = 1;
        //        Resume video playback
        //          pMediaPlayer->
        //          if (pMovie_Track)
        //            pMediaPlayer->bPlaying_Movie = true;

        dword_6BE364_game_settings_1 &= ~GAME_SETTINGS_APP_INACTIVE;

        if (pArcomageGame->bGameInProgress) {
            pArcomageGame->force_redraw_1 = 1;
        } else {
            if (dword_6BE364_game_settings_1 & GAME_SETTINGS_0200_EVENT_TIMER)
                dword_6BE364_game_settings_1 &= ~GAME_SETTINGS_0200_EVENT_TIMER;
            else
                pEventTimer->Resume();
            if (dword_6BE364_game_settings_1 & GAME_SETTINGS_0400_MISC_TIMER)
                dword_6BE364_game_settings_1 &= ~GAME_SETTINGS_0400_MISC_TIMER;
            else
                pMiscTimer->Resume();

            viewparams->bRedrawGameUI = true;
            if (pMovie_Track) {  // pVideoPlayer->pSmackerMovie )
                render->RestoreFrontBuffer();
                render->RestoreBackBuffer();
                // BackToHouseMenu();
            }
        }

        pAudioPlayer->ResumeSounds();
        if (!bGameoverLoop && !pMovie_Track) {  // continue an audio track
            pAudioPlayer->MusicResume();
        }
    }
}

void GameWindowHandler::OnDeactivated() {
    if (!(dword_6BE364_game_settings_1 & GAME_SETTINGS_APP_INACTIVE)) {
        // dword_4E98BC_bApplicationActive = 0;

        dword_6BE364_game_settings_1 |= GAME_SETTINGS_APP_INACTIVE;
        if (pEventTimer != nullptr) {
            if (pEventTimer->bPaused)
                dword_6BE364_game_settings_1 |= GAME_SETTINGS_0200_EVENT_TIMER;
            else
                pEventTimer->Pause();
        }

        if (pMiscTimer != nullptr) {
            if (pMiscTimer->bPaused)
                dword_6BE364_game_settings_1 |= GAME_SETTINGS_0400_MISC_TIMER;
            else
                pMiscTimer->Pause();
        }

        if (pAudioPlayer != nullptr) {
            pAudioPlayer->PauseSounds(2);
            pAudioPlayer->MusicPause();
        }
    }
}

void GameWindowHandler::OnToggleFullscreen() {
    engine->config->window.Fullscreen.Toggle();
    if (engine->config->window.Fullscreen.Get()) {
        window->SetFullscreen(true);
    } else {
        window->SetBorderless(false);
        window->Resize({engine->config->window.Width.Get(), engine->config->window.Height.Get()});
        window->SetPosition({engine->config->window.PositionX.Get(), engine->config->window.PositionY.Get()});
    }
}

void GameWindowHandler::Event(PlatformWindow *window, const PlatformEvent *event) {
    if (nuklear && nuklearEventHandler)
        nuklearEventHandler->Event(window, event);

    PlatformEventHandler::Event(window, event);
}

void GameWindowHandler::KeyPressEvent(PlatformWindow *, const PlatformKeyEvent *event) {
    keyboardController_->ProcessKeyPressEvent(event);

    if (event->isAutoRepeat)
        return;

    PlatformKey key = event->key;
    char c = PlatformKeyToChar(key, event->mods);

    OnChar(key, -1);
    OnKey(key);

    if (c != 0)
        OnChar(key, c);
}

void GameWindowHandler::KeyReleaseEvent(PlatformWindow *, const PlatformKeyEvent *event) {
    keyboardController_->ProcessKeyReleaseEvent(event);

    if (event->key == PlatformKey::PrintScreen)
        OnScreenshot();
}

void GameWindowHandler::MouseMoveEvent(PlatformWindow *, const PlatformMouseEvent *event) {
    OnMouseMove(event->pos.x, event->pos.y, event->buttons & PlatformMouseButton::Left, event->buttons & PlatformMouseButton::Right);
}

void GameWindowHandler::MousePressEvent(PlatformWindow *, const PlatformMouseEvent *event) {
    int x = event->pos.x;
    int y = event->pos.y;

    if (event->button == PlatformMouseButton::Left) {
        if (event->isDoubleClick) {
            OnMouseLeftDoubleClick(x, y);
        } else {
            OnMouseLeftClick(x, y);
        }
    } else if (event->button == PlatformMouseButton::Right) {
        if (event->isDoubleClick) {
            OnMouseRightDoubleClick(x, y);
        } else {
            OnMouseRightClick(x, y);
        }
    }
}

void GameWindowHandler::MouseReleaseEvent(PlatformWindow *, const PlatformMouseEvent *event) {
    if (event->button == PlatformMouseButton::Left) {
        OnMouseLeftUp();
    } else if (event->button == PlatformMouseButton::Right) {
        OnMouseRightUp();
    }
}

void GameWindowHandler::WheelEvent(PlatformWindow *, const PlatformWheelEvent *) {}

void GameWindowHandler::MoveEvent(PlatformWindow *, const PlatformMoveEvent *) {}

void GameWindowHandler::ActivationEvent(PlatformWindow *, const PlatformEvent *event) {
    if (event->type == PlatformEvent::WindowActivate) {
        OnActivated();
    } else if (event->type == PlatformEvent::WindowDeactivate) {
        OnDeactivated();
    }
}

void GameWindowHandler::CloseEvent(PlatformWindow *window, const PlatformEvent *event) {
    UpdateConfigFromWindow();
    engine->config->SaveConfiguration();
    Engine_DeinitializeAndTerminate(0);
}
