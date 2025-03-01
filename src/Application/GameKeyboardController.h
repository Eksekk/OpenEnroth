#pragma once

#include <array>

#include "Utility/IndexedArray.h"

#include "Platform/Filters/PlatformEventFilter.h"

#include "Io/IKeyboardController.h"

class GameKeyboardController: public Io::IKeyboardController, public PlatformEventFilter {
 public:
    GameKeyboardController();

    virtual bool ConsumeKeyPress(PlatformKey key) override;
    virtual bool IsKeyDown(PlatformKey key) const override;

    void reset();

 private:
    virtual bool keyPressEvent(const PlatformKeyEvent *event) override;
    virtual bool keyReleaseEvent(const PlatformKeyEvent *event) override;

 private:
    IndexedArray<bool, PlatformKey::Count> isKeyDown_ = {{}};
    IndexedArray<bool, PlatformKey::Count> isKeyDownReported_ = {{}};
};
