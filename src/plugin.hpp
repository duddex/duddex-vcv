#pragma once
#include <rack.hpp>

using namespace rack;

// Declare the Plugin, defined in plugin.cpp
extern Plugin* pluginInstance;

// Declare each Model, defined in each module source file
extern Model* modelTropicalOscillator;
extern Model* modelBlinkenlights;
extern Model* modelBlinkenlightsPlus;
