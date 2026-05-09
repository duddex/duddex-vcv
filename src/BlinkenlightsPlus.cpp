#include "plugin.hpp"

struct BlinkenlightsPlus : Module {
	enum ParamId {
		FREQ_PARAM,
		COLOR_PARAM,
		BEZEL_PARAM,
		PARAMS_LEN
	};

	enum InputId {
		INPUTS_LEN
	};

	enum OutputId {
		OUTPUTS_LEN
	};

	enum LightId {
		BEZEL_LIGHT_R,
		BEZEL_LIGHT_G,
		BEZEL_LIGHT_B,
		SLIDER_LIGHT_R,
		SLIDER_LIGHT_G,
		SLIDER_LIGHT_B,
		LIGHTS_LEN
	};

	float phase = 0.f;

	BlinkenlightsPlus() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
		configParam(FREQ_PARAM, 0.1f, 5.f, 2.55f, "Blink frequency", " Hz");
		configParam(COLOR_PARAM, 0.f, 255.f, 36.5f, "Color", " RGB");
		configButton(BEZEL_PARAM, "Light bezel");
	}

	void process(const ProcessArgs& args) override {
		float freq = params[FREQ_PARAM].getValue();
		phase += freq * args.sampleTime;
		if (phase >= 1.f)
			phase -= 1.f;

		float brightness = phase < 0.5f ? 1.f : 0.f;
		float colorValue = clamp(params[COLOR_PARAM].getValue(), 0.f, 255.f);
		float t = colorValue / 255.f;
		float position = t * 7.f;
		int index = (int) std::floor(position);
		float mix = position - (float) index;
		if (index >= 7) {
			index = 6;
			mix = 1.f;
		}

		const float colorStops[8][3] = {
			{0.f, 0.f, 0.f},
			{1.f, 0.f, 0.f},
			{1.f, 1.f, 0.f},
			{0.f, 1.f, 0.f},
			{0.f, 1.f, 1.f},
			{0.f, 0.f, 1.f},
			{1.f, 0.f, 1.f},
			{1.f, 1.f, 1.f}
		};

		float r = colorStops[index][0] + (colorStops[index + 1][0] - colorStops[index][0]) * mix;
		float g = colorStops[index][1] + (colorStops[index + 1][1] - colorStops[index][1]) * mix;
		float b = colorStops[index][2] + (colorStops[index + 1][2] - colorStops[index][2]) * mix;

		auto setLightRGB = [this, brightness, r, g, b](int firstLightId) {
			lights[firstLightId + 0].setBrightness(brightness * r);
			lights[firstLightId + 1].setBrightness(brightness * g);
			lights[firstLightId + 2].setBrightness(brightness * b);
		};

		setLightRGB(BEZEL_LIGHT_R);
		setLightRGB(SLIDER_LIGHT_R);
	}
};

struct BlinkenPlusLabel : widget::TransparentWidget {
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

BlinkenPlusLabel* createBlinkenPlusLabel(Vec pos, const char* text, float fontSize = 10.f, NVGcolor color = nvgRGB(0xaa, 0xaa, 0xaa)) {
	BlinkenPlusLabel* label = new BlinkenPlusLabel;
	label->box.pos = pos;
	label->box.size = Vec(60, fontSize + 4);
	label->text = text;
	label->fontSize = fontSize;
	label->color = color;
	label->box.pos.x -= label->box.size.x / 2.f;
	label->box.pos.y -= label->box.size.y / 2.f;
	return label;
}

struct BlinkenlightsPlusWidget : ModuleWidget {
	BlinkenlightsPlusWidget(BlinkenlightsPlus* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/BlinkenlightsPlus.svg")));

		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		float center = 10.16f;
		NVGcolor grey = nvgRGB(0xaa, 0xaa, 0xaa);
		NVGcolor pink = nvgRGB(0xe9, 0x45, 0x60);

		addChild(createBlinkenPlusLabel(mm2px(Vec(center, 7.f)), "BLINKEN", 9.f, grey));
		addChild(createBlinkenPlusLabel(mm2px(Vec(center, 12.f)), "LIGHTS", 9.f, grey));
		addChild(createBlinkenPlusLabel(mm2px(Vec(center, 17.f)), "PLUS", 9.f, grey));

		addChild(createBlinkenPlusLabel(mm2px(Vec(center, 30.f)), "COLOR", 8.f, pink));
		addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(center, 36.f)), module, BlinkenlightsPlus::COLOR_PARAM));

		addParam(createLightParamCentered<VCVLightBezel<RedGreenBlueLight>>(mm2px(Vec(center, 53.f)), module, BlinkenlightsPlus::BEZEL_PARAM, BlinkenlightsPlus::BEZEL_LIGHT_R));

		addParam(createLightParamCentered<VCVLightSlider<RedGreenBlueLight>>(mm2px(Vec(center, 92.f)), module, BlinkenlightsPlus::FREQ_PARAM, BlinkenlightsPlus::SLIDER_LIGHT_R));
		addChild(createBlinkenPlusLabel(mm2px(Vec(center, 106.f)), "FREQ", 8.f, pink));
	}
};

Model* modelBlinkenlightsPlus = createModel<BlinkenlightsPlus, BlinkenlightsPlusWidget>("BlinkenlightsPlus");
