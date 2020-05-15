#pragma once
#include "components.hpp"
#include <math.h>
#include <rack.hpp>

using namespace rack;

// Declare the Plugin, defined in plugin.cpp
extern Plugin* pluginInstance;

// Declare each Model, defined in each module source file
extern Model* modelBitshiftgain;
extern Model* modelCapacitor;
extern Model* modelCapacitor_stereo;
extern Model* modelChorus;
extern Model* modelConsole;
extern Model* modelDistance;
extern Model* modelHombre;
extern Model* modelMv;
extern Model* modelTape;
extern Model* modelTremolo;
extern Model* modelVibrato;

/* other stuff */

// console types
void saveConsoleType(int consoleType);
int loadConsoleType();

// themes
static const std::string lightPanelID = "Light Panel";
static const std::string darkPanelID = "Dark Panel";

// https://github.com/MarcBoule/Geodesics/blob/master/src/Geodesics.hpp
void saveDarkAsDefault(bool darkAsDefault);

bool loadDarkAsDefault();

struct DarkDefaultItem : MenuItem {
    void onAction(const event::Action& e) override
    {
        saveDarkAsDefault(rightText.empty()); // implicitly toggled
    }
};