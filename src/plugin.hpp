#pragma once
#include "components.hpp"
#include <math.h>
#include <rack.hpp>

using namespace rack;

// Declare the Plugin, defined in plugin.cpp
extern Plugin* pluginInstance;

// Declare each Model, defined in each module source file
// extern Model* modelAcceleration;
extern Model* modelBitshiftgain;
extern Model* modelCapacitor;
extern Model* modelCapacitor_stereo;
extern Model* modelChorus;
extern Model* modelConsole;
extern Model* modelConsole_mm;
extern Model* modelDistance;
extern Model* modelGolem;
extern Model* modelHolt;
extern Model* modelHombre;
extern Model* modelInterstage;
extern Model* modelMonitoring;
extern Model* modelMv;
extern Model* modelRasp;
extern Model* modelReseq;
extern Model* modelTape;
extern Model* modelTremolo;
extern Model* modelVibrato;

/* Other stuff */

/* #quality mode
======================================================================================== */
void saveQuality(bool quality);
bool loadQuality();
void saveHighQualityAsDefault(bool highQualityAsDefault);
bool loadHighQualityAsDefault();

struct highQualityDefaultItem : MenuItem {
    void onAction(const event::Action& e) override
    {
        saveHighQualityAsDefault(rightText.empty()); // implicitly toggled
    }
};

/* #console type (Console, Console MM)
======================================================================================== */
void saveConsoleType(int consoleType);
int loadConsoleType();

/* #direct out mode (Console MM)
======================================================================================== */
void saveDirectOutMode(int directOutMode);
int loadDirectOutMode();

/* #slew type (Rasp)
======================================================================================== */
void saveSlewType(int slewType);
int loadSlewType();

/* #delay mode (Golem)
======================================================================================== */
void saveDelayMode(int delayMode);
int loadDelayMode();

/* #themes
======================================================================================== */
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
