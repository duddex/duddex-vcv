#include "plugin.hpp"

struct Blinkenlights : Module {
	enum ParamId {
		FREQ_PARAM,
		COLOR_PARAM,
		PARAMS_LEN
	};

	enum InputId {
		INPUTS_LEN
	};

	enum OutputId {
		OUTPUTS_LEN
	};

	enum LightId {
		BLINK_LIGHT_R,
		BLINK_LIGHT_G,
		BLINK_LIGHT_B,
		LIGHTS_LEN
	};

	float phase = 0.f;
	int colorIndex = 0; // 0=red, 1=yellow, 2=green
	dsp::BooleanTrigger buttonTrigger;

	Blinkenlights() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
		configParam(FREQ_PARAM, 0.1f, 20.f, 1.f, "Blink frequency", " Hz");
		configButton(COLOR_PARAM, "Cycle color");
	}

	void process(const ProcessArgs& args) override {
		if (buttonTrigger.process(params[COLOR_PARAM].getValue() > 0.f)) {
			colorIndex = (colorIndex + 1) % 3;
		}

		float freq = params[FREQ_PARAM].getValue();

		phase += freq * args.sampleTime;
		if (phase >= 1.f)
			phase -= 1.f;

		float brightness = phase < 0.5f ? 1.f : 0.f;

		// Set RGB based on colorIndex
		switch (colorIndex) {
			case 0: // Red
				lights[BLINK_LIGHT_R].setBrightness(brightness);
				lights[BLINK_LIGHT_G].setBrightness(0.f);
				lights[BLINK_LIGHT_B].setBrightness(0.f);
				break;
			case 1: // Yellow (red + green)
				lights[BLINK_LIGHT_R].setBrightness(brightness);
				lights[BLINK_LIGHT_G].setBrightness(brightness);
				lights[BLINK_LIGHT_B].setBrightness(0.f);
				break;
			case 2: // Green
				lights[BLINK_LIGHT_R].setBrightness(0.f);
				lights[BLINK_LIGHT_G].setBrightness(brightness);
				lights[BLINK_LIGHT_B].setBrightness(0.f);
				break;
		}
	}

	json_t* dataToJson() override {
		json_t* rootJ = json_object();
		json_object_set_new(rootJ, "colorIndex", json_integer(colorIndex));
		return rootJ;
	}

	void dataFromJson(json_t* rootJ) override {
		json_t* colorJ = json_object_get(rootJ, "colorIndex");
		if (colorJ)
			colorIndex = json_integer_value(colorJ);
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

		// LED (RGB)
		addChild(createLightCentered<LargeLight<RedGreenBlueLight>>(mm2px(Vec(center, 50.f)), module, Blinkenlights::BLINK_LIGHT_R));

		// Color button
		addChild(createBlinkenLabel(mm2px(Vec(center, 62.f)), "COLOR", 10.f, pink));
		addParam(createParamCentered<VCVButton>(mm2px(Vec(center, 69.f)), module, Blinkenlights::COLOR_PARAM));

		// Frequency knob
		addChild(createBlinkenLabel(mm2px(Vec(center, 80.f)), "FREQ", 10.f, pink));
		addParam(createParamCentered<RoundBigBlackKnob>(mm2px(Vec(center, 90.f)), module, Blinkenlights::FREQ_PARAM));
	}
};

Model* modelBlinkenlights = createModel<Blinkenlights, BlinkenlightsWidget>("Blinkenlights");
