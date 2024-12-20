#include "plugin.hpp"


Plugin* pluginInstance;


void init(Plugin* p) {
	pluginInstance = p;

	// Add modules here
	// p->addModel(modelMyModule);  again, must prepend module name with "model"
	p->addModel(modelPleats);
	p->addModel(modelStrange);
	p->addModel(modelTrip);
	p->addModel(modelSmitty);

	// Any other plugin initialization may go here.
	// As an alternative, consider lazy-loading assets and lookup tables when your module is created to reduce startup times of Rack.
}
