#include "plugin.hpp"
#include "ShimmerReverbDSP.hpp"

/*
 * Shimmer Reverb
 *
 * A stereo shimmer reverb. The controls loosely follow the classic "shimmer"
 * pedal layout (decay, pre-delay, mix, tone, modulation, shimmer amount, pitch
 * interval, infinite hold); the DSP is our own, see ShimmerReverbDSP.hpp.
 */

// Panel layout (millimetres) shared with res/ShimmerReverb.svg
static const float COL_X[3] = {12.7f, 35.56f, 58.42f};
static const float ROW1_KNOB_Y = 26.f, ROW1_CV_Y = 37.5f;
static const float ROW2_KNOB_Y = 55.f, ROW2_CV_Y = 66.5f;
static const float ROW3_Y = 85.f, HOLD_GATE_Y = 97.f;
static const float IO_Y = 114.f;
static const float IO_X[4] = {10.f, 24.f, 47.12f, 61.12f};

struct ShimmerReverb : Module {
	enum ParamId {
		DECAY_PARAM,
		AMOUNT_PARAM,
		MIX_PARAM,
		PREDELAY_PARAM,
		TONE_PARAM,
		MOD_PARAM,
		PITCH_PARAM,
		HOLD_PARAM,
		PARAMS_LEN
	};

	enum InputId {
		DECAY_INPUT,
		AMOUNT_INPUT,
		MIX_INPUT,
		PREDELAY_INPUT,
		TONE_INPUT,
		MOD_INPUT,
		HOLD_INPUT,
		LEFT_INPUT,
		RIGHT_INPUT,
		INPUTS_LEN
	};

	enum OutputId {
		LEFT_OUTPUT,
		RIGHT_OUTPUT,
		OUTPUTS_LEN
	};

	enum LightId {
		HOLD_LIGHT,
		LIGHTS_LEN
	};

	shimmer::Reverb reverb;
	dsp::SchmittTrigger holdGate;

	ShimmerReverb() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);

		// Display values match the DSP mappings in ShimmerReverbDSP.hpp
		configParam(DECAY_PARAM, 0.f, 1.f, 0.5f, "Decay time", " s", 100.f, 0.4f);
		configParam(AMOUNT_PARAM, 0.f, 1.f, 0.5f, "Shimmer amount", "%", 0.f, 100.f);
		configParam(MIX_PARAM, 0.f, 1.f, 0.4f, "Dry/wet mix", "%", 0.f, 100.f);
		configParam(PREDELAY_PARAM, 0.f, 0.5f, 0.02f, "Pre-delay", " ms", 0.f, 1000.f);
		configParam(TONE_PARAM, 0.f, 1.f, 0.65f, "Tone (damping cutoff)", " Hz", 36.f, 500.f);
		configParam(MOD_PARAM, 0.f, 1.f, 0.3f, "Modulation depth", "%", 0.f, 100.f);

		std::vector<std::string> pitchLabels;
		for (int i = 0; i < shimmer::kNumPitchModes; i++)
			pitchLabels.push_back(shimmer::kPitchModes[i].name);
		configSwitch(PITCH_PARAM, 0.f, shimmer::kNumPitchModes - 1, 0.f, "Shimmer pitch", pitchLabels);
		configSwitch(HOLD_PARAM, 0.f, 1.f, 0.f, "Hold (infinite decay)", {"Off", "On"});

		configInput(DECAY_INPUT, "Decay CV");
		configInput(AMOUNT_INPUT, "Shimmer amount CV");
		configInput(MIX_INPUT, "Mix CV");
		configInput(PREDELAY_INPUT, "Pre-delay CV");
		configInput(TONE_INPUT, "Tone CV");
		configInput(MOD_INPUT, "Modulation CV");
		configInput(HOLD_INPUT, "Hold gate");
		configInput(LEFT_INPUT, "Left / mono");
		configInput(RIGHT_INPUT, "Right");

		configOutput(LEFT_OUTPUT, "Left");
		configOutput(RIGHT_OUTPUT, "Right");

		configBypass(LEFT_INPUT, LEFT_OUTPUT);
		configBypass(RIGHT_INPUT, RIGHT_OUTPUT);

		reverb.init(APP->engine->getSampleRate());
	}

	void onSampleRateChange(const SampleRateChangeEvent& e) override {
		reverb.init(e.sampleRate);
	}

	void onReset(const ResetEvent& e) override {
		Module::onReset(e);
		reverb.clear();
	}

	// Knob plus CV (10V = full range of the knob), clamped to the knob range
	float paramWithCV(int paramId, int inputId) {
		ParamQuantity* pq = paramQuantities[paramId];
		float v = params[paramId].getValue();
		if (inputs[inputId].isConnected())
			v += inputs[inputId].getVoltage() / 10.f * (pq->getMaxValue() - pq->getMinValue());
		return clamp(v, pq->getMinValue(), pq->getMaxValue());
	}

	void process(const ProcessArgs& args) override {
		holdGate.process(inputs[HOLD_INPUT].getVoltage(), 0.1f, 1.f);
		bool hold = params[HOLD_PARAM].getValue() > 0.5f || holdGate.isHigh();

		shimmer::Params p;
		p.decay = paramWithCV(DECAY_PARAM, DECAY_INPUT);
		p.amount = paramWithCV(AMOUNT_PARAM, AMOUNT_INPUT);
		p.preDelay = paramWithCV(PREDELAY_PARAM, PREDELAY_INPUT);
		p.tone = paramWithCV(TONE_PARAM, TONE_INPUT);
		p.mod = paramWithCV(MOD_PARAM, MOD_INPUT);
		p.pitchMode = (int) std::round(params[PITCH_PARAM].getValue());
		p.hold = hold;

		// Right input is normalled to the left input
		float dryL = inputs[LEFT_INPUT].getVoltage();
		float dryR = inputs[RIGHT_INPUT].isConnected() ? inputs[RIGHT_INPUT].getVoltage() : dryL;

		float wetL, wetR;
		reverb.process(dryL / 5.f, dryR / 5.f, p, &wetL, &wetR);
		wetL *= 5.f;
		wetR *= 5.f;

		// Equal-power dry/wet crossfade
		float mix = paramWithCV(MIX_PARAM, MIX_INPUT);
		float dryGain = std::cos(mix * 0.5f * M_PI);
		float wetGain = std::sin(mix * 0.5f * M_PI);

		// Gentle limiting keeps long, loud tails within the Rack voltage range
		outputs[LEFT_OUTPUT].setVoltage(10.f * std::tanh((dryL * dryGain + wetL * wetGain) / 10.f));
		outputs[RIGHT_OUTPUT].setVoltage(10.f * std::tanh((dryR * dryGain + wetR * wetGain) / 10.f));

		lights[HOLD_LIGHT].setBrightnessSmooth(hold ? 1.f : 0.f, args.sampleTime);
	}
};

// Panel text label (nanosvg does not render SVG <text>, so labels are drawn here)
struct ShimmerLabel : widget::TransparentWidget {
	std::string text;
	float fontSize = 8.f;
	NVGcolor color = nvgRGB(0xcc, 0xcc, 0xcc);

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

static ShimmerLabel* createShimmerLabel(Vec pos, const char* text, float fontSize, NVGcolor color) {
	ShimmerLabel* label = new ShimmerLabel;
	label->box.size = Vec(80, fontSize + 4);
	label->box.pos = pos.minus(label->box.size.div(2.f));
	label->text = text;
	label->fontSize = fontSize;
	label->color = color;
	return label;
}

// Shows the name of the selected pitch interval
struct ShimmerPitchDisplay : widget::TransparentWidget {
	ShimmerReverb* module = nullptr;

	void drawLayer(const DrawArgs& args, int layer) override {
		if (layer == 1) {
			int mode = 0;
			if (module)
				mode = clamp((int) std::round(module->params[ShimmerReverb::PITCH_PARAM].getValue()), 0, shimmer::kNumPitchModes - 1);
			nvgFontSize(args.vg, 9.f);
			nvgFontFaceId(args.vg, APP->window->uiFont->handle);
			nvgTextAlign(args.vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
			nvgFillColor(args.vg, nvgRGB(0x7f, 0xd4, 0xff));
			nvgText(args.vg, box.size.x / 2.f, box.size.y / 2.f, shimmer::kPitchModes[mode].name, NULL);
		}
		TransparentWidget::drawLayer(args, layer);
	}
};

struct ShimmerReverbWidget : ModuleWidget {
	ShimmerReverbWidget(ShimmerReverb* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/ShimmerReverb.svg")));

		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		const int row1Params[3] = {ShimmerReverb::DECAY_PARAM, ShimmerReverb::AMOUNT_PARAM, ShimmerReverb::MIX_PARAM};
		const int row1Inputs[3] = {ShimmerReverb::DECAY_INPUT, ShimmerReverb::AMOUNT_INPUT, ShimmerReverb::MIX_INPUT};
		const int row2Params[3] = {ShimmerReverb::PREDELAY_PARAM, ShimmerReverb::TONE_PARAM, ShimmerReverb::MOD_PARAM};
		const int row2Inputs[3] = {ShimmerReverb::PREDELAY_INPUT, ShimmerReverb::TONE_INPUT, ShimmerReverb::MOD_INPUT};
		for (int c = 0; c < 3; c++) {
			addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(COL_X[c], ROW1_KNOB_Y)), module, row1Params[c]));
			addInput(createInputCentered<PJ301MPort>(mm2px(Vec(COL_X[c], ROW1_CV_Y)), module, row1Inputs[c]));
			addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(COL_X[c], ROW2_KNOB_Y)), module, row2Params[c]));
			addInput(createInputCentered<PJ301MPort>(mm2px(Vec(COL_X[c], ROW2_CV_Y)), module, row2Inputs[c]));
		}

		addParam(createParamCentered<RoundBlackSnapKnob>(mm2px(Vec(COL_X[0], ROW3_Y)), module, ShimmerReverb::PITCH_PARAM));

		ShimmerPitchDisplay* display = createWidget<ShimmerPitchDisplay>(mm2px(Vec(COL_X[1] - 11.f, ROW3_Y - 4.f)));
		display->box.size = mm2px(Vec(22.f, 8.f));
		display->module = module;
		addChild(display);

		addParam(createLightParamCentered<VCVLightLatch<MediumSimpleLight<WhiteLight>>>(mm2px(Vec(COL_X[2], ROW3_Y)), module, ShimmerReverb::HOLD_PARAM, ShimmerReverb::HOLD_LIGHT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(COL_X[2], HOLD_GATE_Y)), module, ShimmerReverb::HOLD_INPUT));

		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(IO_X[0], IO_Y)), module, ShimmerReverb::LEFT_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(IO_X[1], IO_Y)), module, ShimmerReverb::RIGHT_INPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(IO_X[2], IO_Y)), module, ShimmerReverb::LEFT_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(IO_X[3], IO_Y)), module, ShimmerReverb::RIGHT_OUTPUT));

		// Text labels (drawn in NanoVG; nanosvg ignores SVG <text>)
		NVGcolor accent = nvgRGB(0x7f, 0xd4, 0xff);
		NVGcolor grey = nvgRGB(0x8a, 0x8a, 0xa0);
		NVGcolor light = nvgRGB(0xd0, 0xd0, 0xe0);

		addChild(createShimmerLabel(mm2px(Vec(35.56f, 7.5f)), "SHIMMER", 12.f, accent));
		addChild(createShimmerLabel(mm2px(Vec(35.56f, 12.5f)), "REVERB", 6.5f, grey));

		const char* row1Names[3] = {"DECAY", "AMOUNT", "MIX"};
		const char* row2Names[3] = {"PRE-DLY", "TONE", "MOD"};
		for (int c = 0; c < 3; c++) {
			addChild(createShimmerLabel(mm2px(Vec(COL_X[c], ROW1_KNOB_Y - 7.f)), row1Names[c], 8.f, light));
			addChild(createShimmerLabel(mm2px(Vec(COL_X[c], ROW2_KNOB_Y - 7.f)), row2Names[c], 8.f, light));
		}

		addChild(createShimmerLabel(mm2px(Vec(COL_X[0], ROW3_Y - 7.f)), "PITCH", 8.f, light));
		addChild(createShimmerLabel(mm2px(Vec(COL_X[2], ROW3_Y - 7.f)), "HOLD", 8.f, light));
		addChild(createShimmerLabel(mm2px(Vec(COL_X[2] - 9.f, HOLD_GATE_Y)), "GATE", 6.5f, grey));

		addChild(createShimmerLabel(mm2px(Vec((IO_X[0] + IO_X[1]) / 2.f, IO_Y - 7.f)), "IN L / R", 7.f, grey));
		addChild(createShimmerLabel(mm2px(Vec((IO_X[2] + IO_X[3]) / 2.f, IO_Y - 7.f)), "OUT L / R", 7.f, accent));
	}
};

Model* modelShimmerReverb = createModel<ShimmerReverb, ShimmerReverbWidget>("ShimmerReverb");
