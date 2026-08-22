#include "plugin.hpp"

// 606-inspired synth drum voices (header-only DSP from the git submodule)
#include "BassDrum.hpp"
#include "Clap.hpp"
#include "HiHats.hpp"
#include "Snare.hpp"
#include "Toms.hpp"

/*
 * DrumKit606
 *
 * A seven-voice analog-style drum module wrapping the "606 Inspired Synth Drums"
 * DSP by Matthew Fecher (analogcode/606-Inspired-Synth-Drums), included here as a
 * git submodule under lib/.
 *
 * Voices: Kick, Snare, Clap, Closed Hat, Open Hat, Low Tom, High Tom.
 * Each voice has a trigger input, per-parameter knobs, and CV inputs that add to
 * the knobs. All voices are summed to a single mono audio output.
 */

// Panel layout (millimetres) shared with res/DrumKit606.svg
static const float TRIG_X = 22.f;
static const float TUNE_X = 42.f, TUNE_CV_X = 55.f;
static const float DECAY_X = 73.f, DECAY_CV_X = 86.f;
static const float CHAR_X = 104.f, CHAR_CV_X = 117.f;
static const float LEVEL_X = 138.f;
static const float ROW_Y[7] = {23.f, 37.5f, 52.f, 66.5f, 81.f, 95.5f, 110.f};
static const float OUT_X = 138.f, OUT_Y = 122.f;

struct DrumKit606 : Module {
	enum ParamId {
		KICK_TUNE_PARAM, KICK_DECAY_PARAM, KICK_SNAP_PARAM, KICK_LEVEL_PARAM,
		SNARE_TUNE_PARAM, SNARE_DECAY_PARAM, SNARE_SNAP_PARAM, SNARE_LEVEL_PARAM,
		CLAP_TUNE_PARAM, CLAP_DECAY_PARAM, CLAP_NOISE_PARAM, CLAP_LEVEL_PARAM,
		CHAT_TUNE_PARAM, CHAT_DECAY_PARAM, CHAT_LEVEL_PARAM,
		OHAT_TUNE_PARAM, OHAT_DECAY_PARAM, OHAT_LEVEL_PARAM,
		LTOM_TUNE_PARAM, LTOM_DECAY_PARAM, LTOM_LEVEL_PARAM,
		HTOM_TUNE_PARAM, HTOM_DECAY_PARAM, HTOM_LEVEL_PARAM,
		PARAMS_LEN
	};

	enum InputId {
		KICK_TRIG_INPUT, SNARE_TRIG_INPUT, CLAP_TRIG_INPUT,
		CHAT_TRIG_INPUT, OHAT_TRIG_INPUT, LTOM_TRIG_INPUT, HTOM_TRIG_INPUT,

		KICK_TUNE_INPUT, KICK_DECAY_INPUT, KICK_SNAP_INPUT,
		SNARE_TUNE_INPUT, SNARE_DECAY_INPUT, SNARE_SNAP_INPUT,
		CLAP_TUNE_INPUT, CLAP_DECAY_INPUT, CLAP_NOISE_INPUT,
		CHAT_TUNE_INPUT, CHAT_DECAY_INPUT,
		OHAT_TUNE_INPUT, OHAT_DECAY_INPUT,
		LTOM_TUNE_INPUT, LTOM_DECAY_INPUT,
		HTOM_TUNE_INPUT, HTOM_DECAY_INPUT,
		INPUTS_LEN
	};

	enum OutputId {
		MIX_OUTPUT,
		OUTPUTS_LEN
	};

	enum LightId {
		KICK_LIGHT, SNARE_LIGHT, CLAP_LIGHT,
		CHAT_LIGHT, OHAT_LIGHT, LTOM_LIGHT, HTOM_LIGHT,
		LIGHTS_LEN
	};

	SynthDrums606::BassDrumVoice kick_;
	SynthDrums606::SnareVoice snare_;
	SynthDrums606::ClapVoice clap_;
	SynthDrums606::MetalHiHatVoice closedHat_;
	SynthDrums606::MetalHiHatVoice openHat_;
	SynthDrums606::TomVoice lowTom_;
	SynthDrums606::TomVoice highTom_;

	dsp::SchmittTrigger trigs_[7];
	dsp::PulseGenerator ledPulse_[7];

	DrumKit606() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);

		// Kick
		configParam(KICK_TUNE_PARAM, -12.f, 12.f, 0.f, "Kick tune", " semitones");
		configParam(KICK_DECAY_PARAM, 0.f, 1.f, 0.6f, "Kick decay", "%", 0.f, 100.f);
		configParam(KICK_SNAP_PARAM, 0.f, 1.f, 0.4f, "Kick snap", "%", 0.f, 100.f);
		configParam(KICK_LEVEL_PARAM, 0.f, 1.f, 0.9f, "Kick level", "%", 0.f, 100.f);
		// Snare
		configParam(SNARE_TUNE_PARAM, -12.f, 12.f, 0.f, "Snare tune", " semitones");
		configParam(SNARE_DECAY_PARAM, 0.f, 1.f, 0.6f, "Snare decay", "%", 0.f, 100.f);
		configParam(SNARE_SNAP_PARAM, 0.f, 1.f, 0.6f, "Snare snappy", "%", 0.f, 100.f);
		configParam(SNARE_LEVEL_PARAM, 0.f, 1.f, 0.8f, "Snare level", "%", 0.f, 100.f);
		// Clap
		configParam(CLAP_TUNE_PARAM, -12.f, 12.f, 0.f, "Clap tune", " semitones");
		configParam(CLAP_DECAY_PARAM, 0.f, 1.f, 0.7f, "Clap decay", "%", 0.f, 100.f);
		configParam(CLAP_NOISE_PARAM, 0.f, 1.f, 0.5f, "Clap noise/air", "%", 0.f, 100.f);
		configParam(CLAP_LEVEL_PARAM, 0.f, 1.f, 0.8f, "Clap level", "%", 0.f, 100.f);
		// Closed hat
		configParam(CHAT_TUNE_PARAM, -12.f, 12.f, 0.f, "Closed hat tune", " semitones");
		configParam(CHAT_DECAY_PARAM, 0.f, 1.f, 0.35f, "Closed hat decay", "%", 0.f, 100.f);
		configParam(CHAT_LEVEL_PARAM, 0.f, 1.f, 0.7f, "Closed hat level", "%", 0.f, 100.f);
		// Open hat
		configParam(OHAT_TUNE_PARAM, -12.f, 12.f, 0.f, "Open hat tune", " semitones");
		configParam(OHAT_DECAY_PARAM, 0.f, 1.f, 0.6f, "Open hat decay", "%", 0.f, 100.f);
		configParam(OHAT_LEVEL_PARAM, 0.f, 1.f, 0.7f, "Open hat level", "%", 0.f, 100.f);
		// Low tom
		configParam(LTOM_TUNE_PARAM, -12.f, 12.f, -3.f, "Low tom tune", " semitones");
		configParam(LTOM_DECAY_PARAM, 0.f, 1.f, 0.6f, "Low tom decay", "%", 0.f, 100.f);
		configParam(LTOM_LEVEL_PARAM, 0.f, 1.f, 0.8f, "Low tom level", "%", 0.f, 100.f);
		// High tom
		configParam(HTOM_TUNE_PARAM, -12.f, 12.f, 3.f, "High tom tune", " semitones");
		configParam(HTOM_DECAY_PARAM, 0.f, 1.f, 0.6f, "High tom decay", "%", 0.f, 100.f);
		configParam(HTOM_LEVEL_PARAM, 0.f, 1.f, 0.8f, "High tom level", "%", 0.f, 100.f);

		configInput(KICK_TRIG_INPUT, "Kick trigger");
		configInput(SNARE_TRIG_INPUT, "Snare trigger");
		configInput(CLAP_TRIG_INPUT, "Clap trigger");
		configInput(CHAT_TRIG_INPUT, "Closed hat trigger");
		configInput(OHAT_TRIG_INPUT, "Open hat trigger");
		configInput(LTOM_TRIG_INPUT, "Low tom trigger");
		configInput(HTOM_TRIG_INPUT, "High tom trigger");

		configInput(KICK_TUNE_INPUT, "Kick tune CV");
		configInput(KICK_DECAY_INPUT, "Kick decay CV");
		configInput(KICK_SNAP_INPUT, "Kick snap CV");
		configInput(SNARE_TUNE_INPUT, "Snare tune CV");
		configInput(SNARE_DECAY_INPUT, "Snare decay CV");
		configInput(SNARE_SNAP_INPUT, "Snare snappy CV");
		configInput(CLAP_TUNE_INPUT, "Clap tune CV");
		configInput(CLAP_DECAY_INPUT, "Clap decay CV");
		configInput(CLAP_NOISE_INPUT, "Clap noise/air CV");
		configInput(CHAT_TUNE_INPUT, "Closed hat tune CV");
		configInput(CHAT_DECAY_INPUT, "Closed hat decay CV");
		configInput(OHAT_TUNE_INPUT, "Open hat tune CV");
		configInput(OHAT_DECAY_INPUT, "Open hat decay CV");
		configInput(LTOM_TUNE_INPUT, "Low tom tune CV");
		configInput(LTOM_DECAY_INPUT, "Low tom decay CV");
		configInput(HTOM_TUNE_INPUT, "High tom tune CV");
		configInput(HTOM_DECAY_INPUT, "High tom decay CV");

		configOutput(MIX_OUTPUT, "Mix");

		initVoices(APP->engine->getSampleRate());
	}

	void initVoices(float sampleRate) {
		double sr = sampleRate;
		kick_.init(sr, 0x606001u);
		snare_.init(sr, 0x606002u);
		clap_.init(sr, 0x606003u);
		closedHat_.init(sr, 0x606004u);
		openHat_.init(sr, 0x606005u);
		lowTom_.init(sr, 0x606006u);
		highTom_.init(sr, 0x606007u);
	}

	void onSampleRateChange(const SampleRateChangeEvent& e) override {
		initVoices(e.sampleRate);
	}

	// Knob (0..1) plus CV (10V = full scale), clamped to 0..1.
	float normParam(int paramId, int inputId) {
		float v = params[paramId].getValue();
		if (inputs[inputId].isConnected())
			v += inputs[inputId].getVoltage() / 10.f;
		return clamp(v, 0.f, 1.f);
	}

	// Tune knob (semitones) plus 1V/oct CV, returned as a pitch ratio.
	float tuneRatio(int paramId, int inputId) {
		float semi = params[paramId].getValue();
		if (inputs[inputId].isConnected())
			semi += inputs[inputId].getVoltage() * 12.f;
		return std::pow(2.f, semi / 12.f);
	}

	float tuneSemitones(int paramId, int inputId) {
		float semi = params[paramId].getValue();
		if (inputs[inputId].isConnected())
			semi += inputs[inputId].getVoltage() * 12.f;
		return semi;
	}

	void process(const ProcessArgs& args) override {
		// Trigger detection + voice retrigger on rising edge
		if (trigs_[0].process(inputs[KICK_TRIG_INPUT].getVoltage(), 0.1f, 2.f)) {
			kick_.trigger(normParam(KICK_SNAP_PARAM, KICK_SNAP_INPUT),
			              normParam(KICK_DECAY_PARAM, KICK_DECAY_INPUT),
			              tuneSemitones(KICK_TUNE_PARAM, KICK_TUNE_INPUT));
			ledPulse_[0].trigger(0.05f);
		}
		if (trigs_[1].process(inputs[SNARE_TRIG_INPUT].getVoltage(), 0.1f, 2.f)) {
			snare_.trigger(normParam(SNARE_DECAY_PARAM, SNARE_DECAY_INPUT),
			               tuneRatio(SNARE_TUNE_PARAM, SNARE_TUNE_INPUT),
			               normParam(SNARE_SNAP_PARAM, SNARE_SNAP_INPUT));
			ledPulse_[1].trigger(0.05f);
		}
		if (trigs_[2].process(inputs[CLAP_TRIG_INPUT].getVoltage(), 0.1f, 2.f)) {
			clap_.trigger(normParam(CLAP_DECAY_PARAM, CLAP_DECAY_INPUT),
			              tuneRatio(CLAP_TUNE_PARAM, CLAP_TUNE_INPUT),
			              normParam(CLAP_NOISE_PARAM, CLAP_NOISE_INPUT));
			ledPulse_[2].trigger(0.05f);
		}
		if (trigs_[3].process(inputs[CHAT_TRIG_INPUT].getVoltage(), 0.1f, 2.f)) {
			// Closed hat chokes the open hat, as on the hardware
			openHat_.stop();
			closedHat_.trigger(SynthDrums606::kClosedHatSpec,
			                   normParam(CHAT_DECAY_PARAM, CHAT_DECAY_INPUT),
			                   tuneRatio(CHAT_TUNE_PARAM, CHAT_TUNE_INPUT));
			ledPulse_[3].trigger(0.05f);
		}
		if (trigs_[4].process(inputs[OHAT_TRIG_INPUT].getVoltage(), 0.1f, 2.f)) {
			openHat_.trigger(SynthDrums606::kOpenHatSpec,
			                 normParam(OHAT_DECAY_PARAM, OHAT_DECAY_INPUT),
			                 tuneRatio(OHAT_TUNE_PARAM, OHAT_TUNE_INPUT));
			ledPulse_[4].trigger(0.05f);
		}
		if (trigs_[5].process(inputs[LTOM_TRIG_INPUT].getVoltage(), 0.1f, 2.f)) {
			lowTom_.trigger(SynthDrums606::kLowTomSpec,
			                normParam(LTOM_DECAY_PARAM, LTOM_DECAY_INPUT),
			                tuneRatio(LTOM_TUNE_PARAM, LTOM_TUNE_INPUT));
			ledPulse_[5].trigger(0.05f);
		}
		if (trigs_[6].process(inputs[HTOM_TRIG_INPUT].getVoltage(), 0.1f, 2.f)) {
			highTom_.trigger(SynthDrums606::kHighTomSpec,
			                 normParam(HTOM_DECAY_PARAM, HTOM_DECAY_INPUT),
			                 tuneRatio(HTOM_TUNE_PARAM, HTOM_TUNE_INPUT));
			ledPulse_[6].trigger(0.05f);
		}

		float mix = 0.f;
		mix += kick_.process() * params[KICK_LEVEL_PARAM].getValue();
		mix += snare_.process() * params[SNARE_LEVEL_PARAM].getValue();
		mix += clap_.process() * params[CLAP_LEVEL_PARAM].getValue();
		mix += closedHat_.process() * params[CHAT_LEVEL_PARAM].getValue();
		mix += openHat_.process() * params[OHAT_LEVEL_PARAM].getValue();
		mix += lowTom_.process() * params[LTOM_LEVEL_PARAM].getValue();
		mix += highTom_.process() * params[HTOM_LEVEL_PARAM].getValue();

		// Soft-clip the bus and scale to ±5V line level
		outputs[MIX_OUTPUT].setVoltage(5.f * std::tanh(mix));

		for (int i = 0; i < 7; i++)
			lights[KICK_LIGHT + i].setSmoothBrightness(ledPulse_[i].process(args.sampleTime) ? 1.f : 0.f, args.sampleTime);
	}
};

// Panel text label (nanosvg does not render SVG <text>, so labels are drawn here)
struct DrumLabel : widget::TransparentWidget {
	std::string text;
	float fontSize = 8.f;
	NVGcolor color = nvgRGB(0xcc, 0xcc, 0xcc);
	bool leftAlign = false;

	void drawLayer(const DrawArgs& args, int layer) override {
		if (layer != 1)
			return;
		nvgFontSize(args.vg, fontSize);
		nvgFontFaceId(args.vg, APP->window->uiFont->handle);
		if (leftAlign) {
			nvgTextAlign(args.vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
			nvgFillColor(args.vg, color);
			nvgText(args.vg, 0.f, box.size.y / 2.f, text.c_str(), NULL);
		}
		else {
			nvgTextAlign(args.vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
			nvgFillColor(args.vg, color);
			nvgText(args.vg, box.size.x / 2.f, box.size.y / 2.f, text.c_str(), NULL);
		}
		TransparentWidget::drawLayer(args, layer);
	}
};

static DrumLabel* createDrumLabel(Vec pos, const char* text, float fontSize, NVGcolor color, bool leftAlign = false) {
	DrumLabel* label = new DrumLabel;
	label->box.size = Vec(60, fontSize + 4);
	label->text = text;
	label->fontSize = fontSize;
	label->color = color;
	label->leftAlign = leftAlign;
	label->box.pos = pos;
	label->box.pos.y -= label->box.size.y / 2.f;
	if (!leftAlign)
		label->box.pos.x -= label->box.size.x / 2.f;
	return label;
}

struct DrumKit606Widget : ModuleWidget {
	DrumKit606Widget(DrumKit606* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/DrumKit606.svg")));

		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		// Per-voice trigger inputs and activity lights
		const int trigInputs[7] = {
			DrumKit606::KICK_TRIG_INPUT, DrumKit606::SNARE_TRIG_INPUT, DrumKit606::CLAP_TRIG_INPUT,
			DrumKit606::CHAT_TRIG_INPUT, DrumKit606::OHAT_TRIG_INPUT, DrumKit606::LTOM_TRIG_INPUT, DrumKit606::HTOM_TRIG_INPUT
		};
		for (int r = 0; r < 7; r++) {
			addInput(createInputCentered<PJ301MPort>(mm2px(Vec(TRIG_X, ROW_Y[r])), module, trigInputs[r]));
			addChild(createLightCentered<SmallLight<RedLight>>(mm2px(Vec(TRIG_X + 6.5f, ROW_Y[r] - 5.f)), module, DrumKit606::KICK_LIGHT + r));
		}

		// Tune knob + CV, Decay knob + CV per voice
		const int tuneParam[7] = {
			DrumKit606::KICK_TUNE_PARAM, DrumKit606::SNARE_TUNE_PARAM, DrumKit606::CLAP_TUNE_PARAM,
			DrumKit606::CHAT_TUNE_PARAM, DrumKit606::OHAT_TUNE_PARAM, DrumKit606::LTOM_TUNE_PARAM, DrumKit606::HTOM_TUNE_PARAM
		};
		const int tuneInput[7] = {
			DrumKit606::KICK_TUNE_INPUT, DrumKit606::SNARE_TUNE_INPUT, DrumKit606::CLAP_TUNE_INPUT,
			DrumKit606::CHAT_TUNE_INPUT, DrumKit606::OHAT_TUNE_INPUT, DrumKit606::LTOM_TUNE_INPUT, DrumKit606::HTOM_TUNE_INPUT
		};
		const int decayParam[7] = {
			DrumKit606::KICK_DECAY_PARAM, DrumKit606::SNARE_DECAY_PARAM, DrumKit606::CLAP_DECAY_PARAM,
			DrumKit606::CHAT_DECAY_PARAM, DrumKit606::OHAT_DECAY_PARAM, DrumKit606::LTOM_DECAY_PARAM, DrumKit606::HTOM_DECAY_PARAM
		};
		const int decayInput[7] = {
			DrumKit606::KICK_DECAY_INPUT, DrumKit606::SNARE_DECAY_INPUT, DrumKit606::CLAP_DECAY_INPUT,
			DrumKit606::CHAT_DECAY_INPUT, DrumKit606::OHAT_DECAY_INPUT, DrumKit606::LTOM_DECAY_INPUT, DrumKit606::HTOM_DECAY_INPUT
		};
		const int levelParam[7] = {
			DrumKit606::KICK_LEVEL_PARAM, DrumKit606::SNARE_LEVEL_PARAM, DrumKit606::CLAP_LEVEL_PARAM,
			DrumKit606::CHAT_LEVEL_PARAM, DrumKit606::OHAT_LEVEL_PARAM, DrumKit606::LTOM_LEVEL_PARAM, DrumKit606::HTOM_LEVEL_PARAM
		};
		for (int r = 0; r < 7; r++) {
			addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(TUNE_X, ROW_Y[r])), module, tuneParam[r]));
			addInput(createInputCentered<PJ301MPort>(mm2px(Vec(TUNE_CV_X, ROW_Y[r])), module, tuneInput[r]));
			addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(DECAY_X, ROW_Y[r])), module, decayParam[r]));
			addInput(createInputCentered<PJ301MPort>(mm2px(Vec(DECAY_CV_X, ROW_Y[r])), module, decayInput[r]));
			addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(LEVEL_X, ROW_Y[r])), module, levelParam[r]));
		}

		// Character knob + CV only for the voices that have a third parameter
		addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(CHAR_X, ROW_Y[0])), module, DrumKit606::KICK_SNAP_PARAM));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(CHAR_CV_X, ROW_Y[0])), module, DrumKit606::KICK_SNAP_INPUT));
		addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(CHAR_X, ROW_Y[1])), module, DrumKit606::SNARE_SNAP_PARAM));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(CHAR_CV_X, ROW_Y[1])), module, DrumKit606::SNARE_SNAP_INPUT));
		addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(CHAR_X, ROW_Y[2])), module, DrumKit606::CLAP_NOISE_PARAM));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(CHAR_CV_X, ROW_Y[2])), module, DrumKit606::CLAP_NOISE_INPUT));

		// Main mix output
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(OUT_X, OUT_Y)), module, DrumKit606::MIX_OUTPUT));

		// Text labels (drawn in NanoVG; nanosvg ignores SVG <text>)
		NVGcolor pink = nvgRGB(0xe9, 0x45, 0x60);
		NVGcolor grey = nvgRGB(0x8a, 0x8a, 0xa0);
		NVGcolor light = nvgRGB(0xd0, 0xd0, 0xe0);

		addChild(createDrumLabel(mm2px(Vec(76.2f, 8.f)), "606 DRUMS", 12.f, pink));

		// Column headers
		addChild(createDrumLabel(mm2px(Vec(TRIG_X, 16.f)), "TRIG", 6.5f, grey));
		addChild(createDrumLabel(mm2px(Vec(TUNE_X, 16.f)), "TUNE", 6.5f, grey));
		addChild(createDrumLabel(mm2px(Vec(TUNE_CV_X, 16.f)), "CV", 6.5f, grey));
		addChild(createDrumLabel(mm2px(Vec(DECAY_X, 16.f)), "DECAY", 6.5f, grey));
		addChild(createDrumLabel(mm2px(Vec(DECAY_CV_X, 16.f)), "CV", 6.5f, grey));
		addChild(createDrumLabel(mm2px(Vec(CHAR_X, 16.f)), "CHAR", 6.5f, grey));
		addChild(createDrumLabel(mm2px(Vec(CHAR_CV_X, 16.f)), "CV", 6.5f, grey));
		addChild(createDrumLabel(mm2px(Vec(LEVEL_X, 16.f)), "LEVEL", 6.5f, grey));

		// Voice row names (left aligned)
		const char* voiceNames[7] = {"KICK", "SNARE", "CLAP", "C HAT", "O HAT", "L TOM", "H TOM"};
		for (int r = 0; r < 7; r++)
			addChild(createDrumLabel(mm2px(Vec(4.f, ROW_Y[r])), voiceNames[r], 9.f, light, true));

		// Output label
		addChild(createDrumLabel(mm2px(Vec(OUT_X, 116.f)), "MIX OUT", 8.f, pink));
	}
};

Model* modelDrumKit606 = createModel<DrumKit606, DrumKit606Widget>("DrumKit606");
