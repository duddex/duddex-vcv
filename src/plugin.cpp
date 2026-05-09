#include "plugin.hpp"

Plugin* pluginInstance;

void init(Plugin* p) {
	pluginInstance = p;

	p->addModel(modelTropicalOscillator);
	p->addModel(modelBlinkenlights);
	p->addModel(modelBlinkenlightsPlus);
}
