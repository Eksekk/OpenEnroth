#pragma once

#include <string>

#include "GUI/GUIWindow.h"

class GUIWindow_Travel : public GUIWindow {
 public:
    GUIWindow_Travel();
    virtual ~GUIWindow_Travel() {}

    virtual void Update();
    virtual void Release();
};

class GUIWindow_Transition : public GUIWindow {
 public:
    GUIWindow_Transition(uint32_t anim_id, uint32_t exit_pic_id, int x, int y, int z, int directiony, int directionx, int a8, const std::string &locationName);
    virtual ~GUIWindow_Transition() {}

    virtual void Update();
    virtual void Release();

    int mapid{};
    std::string _mapName{};
};

extern std::string transition_button_label;
