#pragma once

#include <string>
#include <vector>

#include "GameStarterOptions.h"

class GameConfig;
class Platform;

struct GameOptions : public GameStarterOptions {
    enum class Subcommand {
        SUBCOMMAND_GAME,
        SUBCOMMAND_RETRACE
    };
    using enum Subcommand;

    struct RetraceOptions {
        std::vector<std::string> traces;
    };

    Subcommand subcommand = SUBCOMMAND_GAME;
    bool helpPrinted = false; // True means that help message was already printed.
    RetraceOptions retrace;

    /**
     * Parses OpenEnroth command line options.
     *
     * @param argc                      argc as passed to main.
     * @param argv                      argv as passed to main.
     * @throw std::exception            On errors.
     */
    static GameOptions Parse(int argc, char **argv);
};
