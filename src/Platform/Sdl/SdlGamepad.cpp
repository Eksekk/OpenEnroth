#include "SdlGamepad.h"

#include <cassert>

#include "SdlPlatformSharedState.h"

SdlGamepad::SdlGamepad(SdlPlatformSharedState *state, SDL_GameController *gamepad, SDL_JoystickID id): _state(state), _gamepad(gamepad), _id(id) {
    assert(state);
    assert(gamepad);
}

SdlGamepad::~SdlGamepad() {
    SDL_GameControllerClose(_gamepad);
}

std::string SdlGamepad::model() const {
    const char *model = SDL_GameControllerName(_gamepad);
    if (model != NULL)
        return model;

    return {};
}

std::string SdlGamepad::serial() const {
    // TODO: Just update SDL
#if SDL_VERSION_ATLEAST(2, 0, 14)
    const char *serial = SDL_GameControllerGetSerial(_gamepad);
    if (serial != NULL)
        return serial;
#endif

    return {};
}
