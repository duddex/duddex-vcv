#include "plugin.hpp"

struct Blinkenlights : Module {
	enum ParamId {
		FREQ_PARAM,
		PARAMS_LEN
	};

	enum InputId {
		INPUTS_LEN
	};

	enum OutputId {
		OUTPUTS_LEN
	};

	enum LightId {
		BLINK_LIGHT,
		LIGHTS_LEN
	};

	float phase = 0.f;

	Blinkenlights() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
		configParam(FREQ_PARAM, 0.1f, 20.f, 1.f, "Blink frequency", " Hz");
	}

	void process(const ProcessArgs& args) override {
		float freq = params[FREQ_PARAM].getValue();

		phase += freq * args.sampleTime;
		if (phase >= 1.f)
			phase -= 1.f;

		lights[BLINK_LIGHT].setBrightness(phase < 0.5f ? 1.f : 0.f);
	}
};

// Reuse the same label widget as TropicalOscillator
struct BlinkenLabel : widget::TransparentWidget {
	std::string text;
	float fontSize = 11.f;
	NVGcolor color = nvgRGB(0xaa, 0xaa, 0xaa);

	void drawLayer(const DrawArgs& args, int layer) override {
		if (layer != 1)
			return;
		nvgFontSize(args.vg, fontSize);
		nvgFontFaceId(args.vg, APP->window->uiFont->handle);
		nvgTextAlign(args.vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
		nvgFillColor(args.vg, color);
		nvgText(args.vg, box.size.x / 2.f, box.size.y / 2.f, text.c_str(), NULL);
		TransparentWidget::drawLayer(args, layer);
	}
};

BlinkenLabel* createBlinkenLabel(Vec pos, const char* text, float fontSize = 10.f, NVGcolor color = nvgRGB(0xaa, 0xaa, 0xaa)) {
	BlinkenLabel* label = new BlinkenLabel;
	label->box.pos = pos;
	label->box.size = Vec(60, fontSize + 4);
	label->text = text;
	label->fontSize = fontSize;
	label->color = color;
	label->box.pos.x -= label->box.size.x / 2.f;
	label->box.pos.y -= label->box.size.y / 2.f;
	return label;
}

struct BlinkenlightsWidget : ModuleWidget {
	BlinkenlightsWidget(Blinkenlights* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/Blinkenlights.svg")));

		// Screws
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		float center = 10.16f; // center of 4HP = 20.32/2

		NVGcolor grey = nvgRGB(0xaa, 0xaa, 0xaa);
		NVGcolor pink = nvgRGB(0xe9, 0x45, 0x60);

		// Title
		addChild(createBlinkenLabel(mm2px(Vec(center, 9.f)), "BLINKEN", 10.f, grey));
		addChild(createBlinkenLabel(mm2px(Vec(center, 15.f)), "LIGHTS", 10.f, grey));

		// LED
		addChild(createLightCentered<LargeLight<RedLight>>(mm2px(Vec(center, 50.f)), module, Blinkenlights::BLINK_LIGHT));

		// Frequency knob
		addChild(createBlinkenLabel(mm2px(Vec(center, 75.f)), "FREQ", 10.f, pink));
		addParam(createParamCentered<RoundBigBlackKnob>(mm2px(Vec(center, 85.f)), module, Blinkenlights::FREQ_PARAM));
	}
};

Model* modelBlinkenlights = createModel<Blinkenlights, BlinkenlightsWidget>("Blinkenlights");
