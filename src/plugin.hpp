#pragma once
#include <rack.hpp>


using namespace rack;

// Declare the Plugin, defined in plugin.cpp
extern Plugin* pluginInstance;

// Declare each Model, defined in each module source file
// extern Model* modelMyModule - "model" is an SDK convention you must follow. Prepend to Module name.
extern Model* modelPleats;
extern Model* modelStrange;
