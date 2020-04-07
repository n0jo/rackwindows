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
extern Model* modelDistance;
extern Model* modelElectrohat;
extern Model* modelHombre;
extern Model* modelMv;
// extern Model* modelTape;
extern Model* modelTremolo;
extern Model* modelVibrato;