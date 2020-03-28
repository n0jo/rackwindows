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
extern Model* modelChorus;
extern Model* modelHombre;
extern Model* modelMv;
extern Model* modelTape;
extern Model* modelVibrato;