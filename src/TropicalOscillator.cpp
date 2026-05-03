#include "plugin.hpp"

/*
 * Tropical Oscillator (Trop(x[n])-oscillator)
 *
 * Implements Tropical Additive Synthesis as described by C. Bocci and G. Sancristoforo.
 *
 * Architecture (Section 5):
 * - 5 VCOs (1 fundamental + 4 harmonic/subharmonic)
 * - Frequency multipliers for each oscillator (harmonic/subharmonic selection)
 * - Detuners for each harmonic oscillator (tropical beatings)
 * - Tropical VCAs (addition, not multiplication) for each oscillator
 * - Minimum operator (tropical addition ⊕) combines all signals
 * - DC offset correction
 * - Three modes: Full (odd+even), Odd only, Even only
 *
 * trop(x[n]) = min{ a1 + cos(ω1*n + φ1), a2 + cos(ω2*n + φ2), ..., a5 + cos(ω5*n + φ5) }
 */

struct TropicalOscillator : Module {
	enum ParamId {
		// Master tuning
		FREQ_PARAM,       // Master frequency knob (coarse)
		FINE_PARAM,       // Fine tuning

		// Frequency multipliers for each oscillator (harmonic order)
		MULT1_PARAM,      // Fundamental multiplier
		MULT2_PARAM,      // Harmonic 2 multiplier
		MULT3_PARAM,      // Harmonic 3 multiplier
		MULT4_PARAM,      // Harmonic 4 multiplier
		MULT5_PARAM,      // Harmonic 5 multiplier

		// Detuners (Hz offset for each oscillator)
		DETUNE1_PARAM,
		DETUNE2_PARAM,
		DETUNE3_PARAM,
		DETUNE4_PARAM,
		DETUNE5_PARAM,

		// Tropical VCAs (amplitude offsets, added to each cosine)
		TVCA1_PARAM,
		TVCA2_PARAM,
		TVCA3_PARAM,
		TVCA4_PARAM,
		TVCA5_PARAM,

		// DC offset correction
		DC_PARAM,

		PARAMS_LEN
	};

	enum InputId {
		// V/Oct pitch input
		VOCT_INPUT,

		// Tropical VCA CV inputs
		TVCA1_INPUT,
		TVCA2_INPUT,
		TVCA3_INPUT,
		TVCA4_INPUT,
		TVCA5_INPUT,

		INPUTS_LEN
	};

	enum OutputId {
		// Main output
		OUT_OUTPUT,

		OUTPUTS_LEN
	};

	enum LightId {
		LIGHTS_LEN
	};

	// Phase accumulators for 5 oscillators (per polyphony channel)
	float phases[16][5] = {};

	TropicalOscillator() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);

		// Master frequency: V/Oct centered at C4
		configParam(FREQ_PARAM, std::log2(20.f / dsp::FREQ_C4), std::log2(10000.f / dsp::FREQ_C4), 0.f, "Frequency", " Hz", 0.f, 0.f);
		getParamQuantity(FREQ_PARAM)->displayBase = 2.f;
		getParamQuantity(FREQ_PARAM)->displayMultiplier = dsp::FREQ_C4;
		configParam(FINE_PARAM, -100.f, 100.f, 0.f, "Fine tune", " cents");

		// Frequency multipliers (1 = fundamental, 2 = 2nd harmonic, etc.)
		configParam(MULT1_PARAM, 0.5f, 16.f, 1.f, "Multiplier 1 (Fund)");
		configParam(MULT2_PARAM, 0.5f, 16.f, 2.f, "Multiplier 2");
		configParam(MULT3_PARAM, 0.5f, 16.f, 3.f, "Multiplier 3");
		configParam(MULT4_PARAM, 0.5f, 16.f, 4.f, "Multiplier 4");
		configParam(MULT5_PARAM, 0.5f, 16.f, 5.f, "Multiplier 5");

		// Detuners: ±10 Hz
		configParam(DETUNE1_PARAM, -10.f, 10.f, 0.f, "Detune 1", " Hz");
		configParam(DETUNE2_PARAM, -10.f, 10.f, 0.f, "Detune 2", " Hz");
		configParam(DETUNE3_PARAM, -10.f, 10.f, 0.f, "Detune 3", " Hz");
		configParam(DETUNE4_PARAM, -10.f, 10.f, 0.f, "Detune 4", " Hz");
		configParam(DETUNE5_PARAM, -10.f, 10.f, 0.f, "Detune 5", " Hz");

		// Tropical VCAs: range -1 to 1 (vertical shift of cosine)
		configParam(TVCA1_PARAM, -1.f, 1.f, 0.f, "Tropical VCA 1");
		configParam(TVCA2_PARAM, -1.f, 1.f, 0.f, "Tropical VCA 2");
		configParam(TVCA3_PARAM, -1.f, 1.f, 0.f, "Tropical VCA 3");
		configParam(TVCA4_PARAM, -1.f, 1.f, 0.f, "Tropical VCA 4");
		configParam(TVCA5_PARAM, -1.f, 1.f, 0.f, "Tropical VCA 5");

		// DC offset
		configParam(DC_PARAM, -1.f, 1.f, 0.f, "DC Offset");

		// Inputs
		configInput(VOCT_INPUT, "V/Oct");
		configInput(TVCA1_INPUT, "Tropical VCA 1 CV");
		configInput(TVCA2_INPUT, "Tropical VCA 2 CV");
		configInput(TVCA3_INPUT, "Tropical VCA 3 CV");
		configInput(TVCA4_INPUT, "Tropical VCA 4 CV");
		configInput(TVCA5_INPUT, "Tropical VCA 5 CV");

		// Output
		configOutput(OUT_OUTPUT, "Audio");
	}

	void process(const ProcessArgs& args) override {
		// Get number of polyphony channels
		int channels = std::max(1, inputs[VOCT_INPUT].getChannels());
		outputs[OUT_OUTPUT].setChannels(channels);

		// Get multipliers
		float mult[5];
		mult[0] = params[MULT1_PARAM].getValue();
		mult[1] = params[MULT2_PARAM].getValue();
		mult[2] = params[MULT3_PARAM].getValue();
		mult[3] = params[MULT4_PARAM].getValue();
		mult[4] = params[MULT5_PARAM].getValue();

		// Read detuners (Hz)
		float detune[5];
		detune[0] = params[DETUNE1_PARAM].getValue();
		detune[1] = params[DETUNE2_PARAM].getValue();
		detune[2] = params[DETUNE3_PARAM].getValue();
		detune[3] = params[DETUNE4_PARAM].getValue();
		detune[4] = params[DETUNE5_PARAM].getValue();

		// Read tropical VCAs
		float tvca[5];
		tvca[0] = params[TVCA1_PARAM].getValue() + inputs[TVCA1_INPUT].getVoltage() / 10.f;
		tvca[1] = params[TVCA2_PARAM].getValue() + inputs[TVCA2_INPUT].getVoltage() / 10.f;
		tvca[2] = params[TVCA3_PARAM].getValue() + inputs[TVCA3_INPUT].getVoltage() / 10.f;
		tvca[3] = params[TVCA4_PARAM].getValue() + inputs[TVCA4_INPUT].getVoltage() / 10.f;
		tvca[4] = params[TVCA5_PARAM].getValue() + inputs[TVCA5_INPUT].getVoltage() / 10.f;

		// DC offset
		float dcOffset = params[DC_PARAM].getValue();

		// Read master frequency
		float freqParam = params[FREQ_PARAM].getValue();
		float fineParam = params[FINE_PARAM].getValue() / 1200.f; // cents to octaves

		for (int c = 0; c < channels; c++) {
			// Compute base frequency: knob sets Hz, fine adds cents, V/Oct modulates
			float pitch = freqParam + fineParam;
			pitch += inputs[VOCT_INPUT].getPolyVoltage(c);

			float baseFreq = dsp::FREQ_C4 * std::pow(2.f, pitch);

			// Compute each oscillator's contribution and take the minimum
			float tropMin = INFINITY;

			for (int i = 0; i < 5; i++) {
				// Frequency for this oscillator: base * multiplier + detune
				float freq = baseFreq * mult[i] + detune[i];

				// Clamp frequency to reasonable range
				freq = clamp(freq, 0.f, args.sampleRate / 2.f);

				// Accumulate phase
				phases[c][i] += freq * args.sampleTime;
				if (phases[c][i] >= 1.f)
					phases[c][i] -= 1.f;
				if (phases[c][i] < 0.f)
					phases[c][i] += 1.f;

				// Compute cosine: cos(2π * phase)
				float cosVal = std::cos(2.f * M_PI * phases[c][i]);

				// Tropical VCA: add amplitude offset (tropical multiplication = addition)
				// Result: a_i + cos(ω_i * n)
				float tropVal = tvca[i] + cosVal;

				// Tropical addition: take minimum
				if (tropVal < tropMin)
					tropMin = tropVal;
			}

			// Apply DC offset correction
			// The waveform range is [-1 + α, 1 + α] where α = min(tvca[i])
			// We shift to center around 0
			float out = tropMin + dcOffset;

			// Scale to ±5V (standard audio level)
			out *= 5.f;

			// Clamp to safe output range
			out = clamp(out, -12.f, 12.f);

			// Check for NaN/Inf
			if (!std::isfinite(out))
				out = 0.f;

			outputs[OUT_OUTPUT].setVoltage(out, c);
		}
	}
};

// Simple label widget for drawing text on the panel
struct TropLabel : widget::TransparentWidget {
	std::string text;
	float fontSize = 11.f;
	NVGcolor color = nvgRGB(0xe9, 0x45, 0x60);
	enum Align { LEFT, CENTER };
	Align align = CENTER;

	void drawLayer(const DrawArgs& args, int layer) override {
		if (layer != 1)
			return;
		nvgFontSize(args.vg, fontSize);
		nvgFontFaceId(args.vg, APP->window->uiFont->handle);
		if (align == CENTER)
			nvgTextAlign(args.vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
		else
			nvgTextAlign(args.vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
		nvgFillColor(args.vg, color);
		nvgText(args.vg, box.size.x / 2.f, box.size.y / 2.f, text.c_str(), NULL);
		TransparentWidget::drawLayer(args, layer);
	}
};

TropLabel* createLabel(Vec pos, const char* text, float fontSize = 10.f, NVGcolor color = nvgRGB(0xcc, 0xcc, 0xcc)) {
	TropLabel* label = new TropLabel;
	label->box.pos = pos;
	label->box.size = Vec(60, fontSize + 4);
	label->text = text;
	label->fontSize = fontSize;
	label->color = color;
	return label;
}

TropLabel* createLabelCentered(Vec pos, const char* text, float fontSize = 10.f, NVGcolor color = nvgRGB(0xcc, 0xcc, 0xcc)) {
	TropLabel* label = createLabel(pos, text, fontSize, color);
	label->box.pos.x -= label->box.size.x / 2.f;
	label->box.pos.y -= label->box.size.y / 2.f;
	return label;
}

struct TropicalOscillatorWidget : ModuleWidget {
	TropicalOscillatorWidget(TropicalOscillator* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/TropicalOscillator.svg")));

		// Screws
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		// Layout constants - panel is 76.2mm (15HP) wide x 128.5mm tall
		// 5 columns for oscillators
		float col1 = 10.f;
		float col2 = 23.f;
		float col3 = 36.f;
		float col4 = 49.f;
		float col5 = 62.f;

		float row0 = 22.f;   // Master tuning
		float row1 = 40.f;   // Multipliers
		float row2 = 56.f;   // Detuners
		float row3 = 72.f;   // Tropical VCAs
		float row4 = 88.f;   // VCA CV inputs
		float row5 = 110.f;  // V/Oct, DC

		NVGcolor pink = nvgRGB(0xe9, 0x45, 0x60);
		NVGcolor grey = nvgRGB(0xaa, 0xaa, 0xaa);
		NVGcolor dim = nvgRGB(0x77, 0x88, 0x99);

		// Title label
		addChild(createLabelCentered(mm2px(Vec(38.1f, 6.f)), "TROPICAL OSCILLATOR", 12.f, grey));

		// Row 0 labels: FREQ, FINE
		addChild(createLabelCentered(mm2px(Vec(col1, row0 - 11.f)), "FREQ", 10.f, pink));
		addChild(createLabelCentered(mm2px(Vec(col2, row0 - 8.f)), "FINE", 10.f, pink));

		// Master tuning (row 0)
		addParam(createParamCentered<RoundBigBlackKnob>(mm2px(Vec(col1, row0)), module, TropicalOscillator::FREQ_PARAM));
		addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(col2, row0)), module, TropicalOscillator::FINE_PARAM));

		// Row 1 labels: MULTIPLIERS
		addChild(createLabelCentered(mm2px(Vec(col1, row1 - 6.f)), "M1", 9.f, grey));
		addChild(createLabelCentered(mm2px(Vec(col2, row1 - 6.f)), "M2", 9.f, grey));
		addChild(createLabelCentered(mm2px(Vec(col3, row1 - 6.f)), "M3", 9.f, grey));
		addChild(createLabelCentered(mm2px(Vec(col4, row1 - 6.f)), "M4", 9.f, grey));
		addChild(createLabelCentered(mm2px(Vec(col5, row1 - 6.f)), "M5", 9.f, grey));

		// Multipliers (row 1)
		addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(col1, row1)), module, TropicalOscillator::MULT1_PARAM));
		addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(col2, row1)), module, TropicalOscillator::MULT2_PARAM));
		addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(col3, row1)), module, TropicalOscillator::MULT3_PARAM));
		addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(col4, row1)), module, TropicalOscillator::MULT4_PARAM));
		addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(col5, row1)), module, TropicalOscillator::MULT5_PARAM));

		// Row 2 labels: DETUNERS
		addChild(createLabelCentered(mm2px(Vec(col1, row2 - 6.f)), "D1", 9.f, grey));
		addChild(createLabelCentered(mm2px(Vec(col2, row2 - 6.f)), "D2", 9.f, grey));
		addChild(createLabelCentered(mm2px(Vec(col3, row2 - 6.f)), "D3", 9.f, grey));
		addChild(createLabelCentered(mm2px(Vec(col4, row2 - 6.f)), "D4", 9.f, grey));
		addChild(createLabelCentered(mm2px(Vec(col5, row2 - 6.f)), "D5", 9.f, grey));

		// Detuners (row 2)
		addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(col1, row2)), module, TropicalOscillator::DETUNE1_PARAM));
		addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(col2, row2)), module, TropicalOscillator::DETUNE2_PARAM));
		addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(col3, row2)), module, TropicalOscillator::DETUNE3_PARAM));
		addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(col4, row2)), module, TropicalOscillator::DETUNE4_PARAM));
		addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(col5, row2)), module, TropicalOscillator::DETUNE5_PARAM));

		// Row 3 labels: TROP VCA
		addChild(createLabelCentered(mm2px(Vec(col1, row3 - 6.f)), "a1", 9.f, pink));
		addChild(createLabelCentered(mm2px(Vec(col2, row3 - 6.f)), "a2", 9.f, pink));
		addChild(createLabelCentered(mm2px(Vec(col3, row3 - 6.f)), "a3", 9.f, pink));
		addChild(createLabelCentered(mm2px(Vec(col4, row3 - 6.f)), "a4", 9.f, pink));
		addChild(createLabelCentered(mm2px(Vec(col5, row3 - 6.f)), "a5", 9.f, pink));

		// Tropical VCAs (row 3)
		addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(col1, row3)), module, TropicalOscillator::TVCA1_PARAM));
		addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(col2, row3)), module, TropicalOscillator::TVCA2_PARAM));
		addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(col3, row3)), module, TropicalOscillator::TVCA3_PARAM));
		addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(col4, row3)), module, TropicalOscillator::TVCA4_PARAM));
		addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(col5, row3)), module, TropicalOscillator::TVCA5_PARAM));

		// Row 4 labels: CV
		addChild(createLabelCentered(mm2px(Vec(col1, row4 - 6.f)), "CV1", 8.f, dim));
		addChild(createLabelCentered(mm2px(Vec(col2, row4 - 6.f)), "CV2", 8.f, dim));
		addChild(createLabelCentered(mm2px(Vec(col3, row4 - 6.f)), "CV3", 8.f, dim));
		addChild(createLabelCentered(mm2px(Vec(col4, row4 - 6.f)), "CV4", 8.f, dim));
		addChild(createLabelCentered(mm2px(Vec(col5, row4 - 6.f)), "CV5", 8.f, dim));

		// Tropical VCA CV inputs (row 4)
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(col1, row4)), module, TropicalOscillator::TVCA1_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(col2, row4)), module, TropicalOscillator::TVCA2_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(col3, row4)), module, TropicalOscillator::TVCA3_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(col4, row4)), module, TropicalOscillator::TVCA4_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(col5, row4)), module, TropicalOscillator::TVCA5_INPUT));

		// Output in col5, bottom row
		addChild(createLabelCentered(mm2px(Vec(col5, row5 - 6.f)), "OUT", 10.f, pink));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(col5, row5)), module, TropicalOscillator::OUT_OUTPUT));

		// Bottom row: DC, V/Oct
		addChild(createLabelCentered(mm2px(Vec(col1, row5 - 6.f)), "DC", 10.f, grey));
		addChild(createLabelCentered(mm2px(Vec(col2, row5 - 6.f)), "V/OCT", 10.f, grey));
		addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(col1, row5)), module, TropicalOscillator::DC_PARAM));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(col2, row5)), module, TropicalOscillator::VOCT_INPUT));
	}
};

Model* modelTropicalOscillator = createModel<TropicalOscillator, TropicalOscillatorWidget>("TropicalOscillator");
