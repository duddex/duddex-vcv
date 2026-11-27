#pragma once
/*
 * ShimmerReverbDSP
 *
 * A shimmer reverb: a plate-style "figure-eight" allpass tank (topology from
 * J. Dattorro, "Effect Design Part 1: Reverberator and Other Filters",
 * J. Audio Eng. Soc. 45(9), 1997) with a pitch shifter in the right-to-left
 * cross-feed path of the tank. Every trip around the loop adds another shifted
 * copy, so the tail climbs in pitch (the classic Eno/Lanois "shimmer"), while the
 * tone filters make the upper layers fade out.
 *
 * This file has no dependency on VCV Rack so it can be tested offline.
 */

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <vector>

namespace shimmer {

static constexpr float kPi = 3.14159265358979f;

// Circular delay line with a power-of-two buffer.
// read(d) returns the sample written d samples ago (d = 1 is the most recent).
struct DelayLine {
	std::vector<float> buf;
	size_t mask = 0;
	size_t w = 0;

	void init(size_t maxDelay) {
		size_t size = 1;
		while (size < maxDelay + 4)
			size <<= 1;
		buf.assign(size, 0.f);
		mask = size - 1;
		w = 0;
	}

	void clear() {
		std::fill(buf.begin(), buf.end(), 0.f);
	}

	void write(float x) {
		buf[w] = x;
		w = (w + 1) & mask;
	}

	float read(size_t d) const {
		return buf[(w - d) & mask];
	}

	float readLinear(float d) const {
		size_t i = (size_t) d;
		float f = d - (float) i;
		float a = read(i);
		float b = read(i + 1);
		return a + (b - a) * f;
	}

	// 4-point Hermite interpolation (needs d >= 1)
	float readCubic(float d) const {
		size_t i = (size_t) d;
		float f = d - (float) i;
		float xm1 = read(i - 1);
		float x0 = read(i);
		float x1 = read(i + 1);
		float x2 = read(i + 2);
		float c1 = 0.5f * (x1 - xm1);
		float c2 = xm1 - 2.5f * x0 + 2.f * x1 - 0.5f * x2;
		float c3 = 0.5f * (x2 - xm1) + 1.5f * (x0 - x1);
		return ((c3 * f + c2) * f + c1) * f + x0;
	}
};

// Schroeder allpass with an optionally modulated (fractional) delay.
struct Allpass {
	DelayLine line;
	float gain = 0.5f;

	void init(size_t maxDelay, float g) {
		line.init(maxDelay);
		gain = g;
	}

	float process(float x, float delay) {
		float d = line.readLinear(delay);
		float v = x + gain * d;
		line.write(v);
		return d - gain * v;
	}
};

struct OnePoleLP {
	float z = 0.f;
	float a = 1.f;

	void setCutoff(float fc, float sampleRate) {
		a = 1.f - std::exp(-2.f * kPi * fc / sampleRate);
	}

	float process(float x) {
		z += a * (x - z);
		return z;
	}
};

struct OnePoleHP {
	OnePoleLP lp;

	void setCutoff(float fc, float sampleRate) {
		lp.setCutoff(fc, sampleRate);
	}

	float process(float x) {
		return x - lp.process(x);
	}
};

// Delay-line pitch shifter: two read taps sweep through a short window in
// opposite phase and are crossfaded with Hann windows.
struct PitchShifter {
	DelayLine line;
	float window = 2048.f;
	float phase = 0.f;
	float increment = 0.f;

	void init(float sampleRate) {
		window = std::round(0.06f * sampleRate);
		line.init((size_t) window + 8);
		phase = 0.f;
	}

	void clear() {
		line.clear();
	}

	void setSemitones(float semitones) {
		float ratio = std::pow(2.f, semitones / 12.f);
		increment = (1.f - ratio) / window;
	}

	float process(float x) {
		line.write(x);
		phase += increment;
		phase -= std::floor(phase);
		float phaseB = phase + 0.5f;
		if (phaseB >= 1.f)
			phaseB -= 1.f;
		float a = line.readCubic(2.f + phase * window);
		float b = line.readCubic(2.f + phaseB * window);
		float wa = std::sin(kPi * phase);
		float wb = std::sin(kPi * phaseB);
		return a * wa * wa + b * wb * wb;
	}
};

struct SineLFO {
	float phase = 0.f;
	float freq = 0.5f;

	float process(float sampleTime) {
		phase += freq * sampleTime;
		phase -= std::floor(phase);
		return std::sin(2.f * kPi * phase);
	}
};

struct PitchMode {
	const char* name;
	float semitonesA;
	float semitonesB;
	// Share of the second voice (0 = voice A only)
	float voiceB;
};

static const PitchMode kPitchModes[] = {
	{"+1 OCT", 12.f, 0.f, 0.f},
	{"+5TH", 7.f, 0.f, 0.f},
	{"+OCT +5TH", 12.f, 7.f, 0.5f},
	{"+/- 1 OCT", 12.f, -12.f, 0.4f},
	{"+1 +2 OCT", 12.f, 24.f, 0.35f},
	{"-1 OCT", -12.f, 0.f, 0.f},
};
static constexpr int kNumPitchModes = sizeof(kPitchModes) / sizeof(kPitchModes[0]);

struct Params {
	float decay = 0.5f;       // 0..1 -> RT60 0.4 s .. 40 s
	float preDelay = 0.f;     // seconds, 0..0.5
	float tone = 0.6f;        // 0..1 dark .. bright
	float mod = 0.3f;         // 0..1 tank modulation depth
	float amount = 0.5f;      // 0..1 shimmer amount
	int pitchMode = 0;
	bool hold = false;
};

class Reverb {
public:
	void init(float sampleRate) {
		sr = sampleRate;
		scale = sr / kRefRate;

		preDelay.init((size_t) (0.55f * sr));

		const size_t inLen[4] = {142, 107, 379, 277};
		const float inGain[4] = {0.75f, 0.75f, 0.625f, 0.625f};
		for (int i = 0; i < 4; i++) {
			inDelay[i] = std::round(inLen[i] * scale);
			inAp[i].init((size_t) inDelay[i] + 2, inGain[i]);
		}

		const float maxExcursion = kExcursion * scale + 2.f;
		for (int s = 0; s < 2; s++) {
			Half& h = half[s];
			h.ap1Delay = std::round(kAp1[s] * scale);
			h.del1Delay = std::round(kDel1[s] * scale);
			h.ap2Delay = std::round(kAp2[s] * scale);
			h.del2Delay = std::round(kDel2[s] * scale);
			h.ap1.init((size_t) (h.ap1Delay + maxExcursion), -0.7f);
			h.ap2.init((size_t) (h.ap2Delay + maxExcursion), 0.5f);
			// Output taps can reach further back than the nominal delay length
			h.del1.init((size_t) (std::max(h.del1Delay, 3627.f * scale) + 2));
			h.del2.init((size_t) (std::max(h.del2Delay, 2673.f * scale) + 2));
			h.outDC.setCutoff(15.f, sr);
		}

		for (int v = 0; v < 2; v++)
			shifter[v].init(sr);
		shimmerHP.setCutoff(120.f, sr);

		inputLP.setCutoff(16000.f, sr);
		inputHP.setCutoff(20.f, sr);

		const float lfoFreq[4] = {0.31f, 0.37f, 0.53f, 0.61f};
		for (int i = 0; i < 4; i++) {
			lfo[i].freq = lfoFreq[i];
			lfo[i].phase = 0.25f * i;
		}

		smoothCoef = 1.f - std::exp(-1.f / (0.02f * sr));
		currentPitchMode = -1;
		clear();
	}

	void clear() {
		preDelay.clear();
		for (int i = 0; i < 4; i++)
			inAp[i].line.clear();
		for (int s = 0; s < 2; s++) {
			Half& h = half[s];
			h.ap1.line.clear();
			h.ap2.line.clear();
			h.del1.clear();
			h.del2.clear();
			h.damping.z = 0.f;
			h.outDC.lp.z = 0.f;
		}
		for (int v = 0; v < 2; v++)
			shifter[v].clear();
		shimmerLP.z = 0.f;
		shimmerHP.lp.z = 0.f;
		inputLP.z = 0.f;
		inputHP.lp.z = 0.f;
	}

	// Inputs and outputs are nominally +/-1. Produces the 100% wet signal.
	void process(float inL, float inR, const Params& p, float* outL, float* outR) {
		updateParams(p);

		// Mono input, band limited, pre-delayed and diffused
		float x = inputHP.process(inputLP.process(0.5f * (inL + inR)));
		x *= 1.f - holdMix;
		float pre = preDelay.readLinear(std::max(1.f, preDelaySm * sr));
		preDelay.write(x);
		float diffused = pre;
		for (int i = 0; i < 4; i++)
			diffused = inAp[i].process(diffused, inDelay[i]);

		// Tank modulation (faded out during hold: fractional delays slowly lose energy)
		float excursion = modSm * (1.f - holdMix) * kExcursion * scale;
		float m[4];
		for (int i = 0; i < 4; i++)
			m[i] = lfo[i].process(1.f / sr);

		// Figure-eight cross-feed. The pitch shifter sits in the right-to-left
		// path only, so the tail climbs one step per full trip around the tank.
		float fromRight = half[1].del2.read((size_t) half[1].del2Delay);
		float fromLeft = half[0].del2.read((size_t) half[0].del2Delay);
		float feedL = softLimit(decayGain * shimmerBlend(fromRight));
		float feedR = softLimit(decayGain * fromLeft);

		runHalf(half[0], diffused + feedL, excursion * m[0], 0.5f * excursion * m[2]);
		runHalf(half[1], diffused + feedR, excursion * m[1], 0.5f * excursion * m[3]);

		// Output taps from Dattorro's Table 2 (half 0 = left side of Fig. 1)
		const Half& L = half[0];
		const Half& R = half[1];
		float yl = R.del1.read(tap(266)) + R.del1.read(tap(2974)) - R.ap2.line.read(tap(1913))
			+ R.del2.read(tap(1996)) - L.del1.read(tap(1990)) - L.ap2.line.read(tap(187))
			- L.del2.read(tap(1066));
		float yr = L.del1.read(tap(353)) + L.del1.read(tap(3627)) - L.ap2.line.read(tap(1228))
			+ L.del2.read(tap(2673)) - R.del1.read(tap(2111)) - R.ap2.line.read(tap(335))
			- R.del2.read(tap(121));

		*outL = half[0].outDC.process(kOutGain * yl);
		*outR = half[1].outDC.process(kOutGain * yr);
	}

	float getSampleRate() const {
		return sr;
	}

private:
	struct Half {
		Allpass ap1;
		DelayLine del1;
		OnePoleLP damping;
		Allpass ap2;
		DelayLine del2;
		OnePoleHP outDC;
		float ap1Delay = 0.f, del1Delay = 0.f, ap2Delay = 0.f, del2Delay = 0.f;
	};

	static constexpr float kRefRate = 29761.f;
	static constexpr float kExcursion = 16.f;
	static constexpr float kOutGain = 0.6f;
	// Length of one tank half at the reference rate, used for the decay mapping
	static constexpr float kHalfLoopSeconds = (672.f + 4453.f + 1800.f + 3720.f) / kRefRate;
	const float kAp1[2] = {672.f, 908.f};
	const float kDel1[2] = {4453.f, 4217.f};
	const float kAp2[2] = {1800.f, 2656.f};
	const float kDel2[2] = {3720.f, 3163.f};

	float sr = 48000.f;
	float scale = 1.f;
	float smoothCoef = 0.001f;

	DelayLine preDelay;
	Allpass inAp[4];
	float inDelay[4] = {};
	OnePoleLP inputLP;
	OnePoleHP inputHP;
	Half half[2];
	SineLFO lfo[4];
	PitchShifter shifter[2];
	OnePoleLP shimmerLP;
	OnePoleHP shimmerHP;

	// Smoothed parameters
	float decayGain = 0.f;
	float preDelaySm = 0.f;
	float modSm = 0.f;
	float amountSm = 0.f;
	float holdMix = 0.f;
	float voiceBSm = 0.f;
	float toneTarget = -1.f;
	int currentPitchMode = -1;
	int toneCounter = 0;

	size_t tap(float samples) const {
		return std::max<size_t>(1, (size_t) (samples * scale));
	}

	void updateParams(const Params& p) {
		// RT60 -> gain applied twice per tank half
		float rt60 = 0.4f * std::pow(100.f, std::min(std::max(p.decay, 0.f), 1.f));
		float g = std::pow(10.f, -3.f * kHalfLoopSeconds / (2.f * rt60));
		float hold = p.hold ? 1.f : 0.f;

		holdMix += smoothCoef * (hold - holdMix);
		float targetGain = g + (1.f - g) * holdMix;
		decayGain += smoothCoef * (targetGain - decayGain);
		preDelaySm += smoothCoef * (std::min(std::max(p.preDelay, 0.f), 0.5f) - preDelaySm);
		modSm += smoothCoef * (std::min(std::max(p.mod, 0.f), 1.f) - modSm);
		amountSm += smoothCoef * (std::min(std::max(p.amount, 0.f), 1.f) * (1.f - holdMix) - amountSm);

		int mode = std::min(std::max(p.pitchMode, 0), kNumPitchModes - 1);
		if (mode != currentPitchMode) {
			currentPitchMode = mode;
			shifter[0].setSemitones(kPitchModes[mode].semitonesA);
			shifter[1].setSemitones(kPitchModes[mode].semitonesB);
		}
		voiceBSm += smoothCoef * (kPitchModes[mode].voiceB - voiceBSm);

		// Filter coefficients use exp(), so only refresh them every 32 samples
		if (toneCounter-- <= 0 || toneTarget < 0.f) {
			toneCounter = 32;
			float t = std::min(std::max(p.tone, 0.f), 1.f);
			if (t != toneTarget) {
				toneTarget = t;
				float fc = std::min(500.f * std::pow(36.f, t), 0.45f * sr);
				for (int s = 0; s < 2; s++)
					half[s].damping.setCutoff(fc, sr);
				shimmerLP.setCutoff(fc, sr);
			}
		}
	}

	float shimmerBlend(float x) {
		float shifted = 0.f;
		if (voiceBSm < 0.999f)
			shifted += std::sqrt(1.f - voiceBSm) * shifter[0].process(x);
		if (voiceBSm > 0.001f)
			shifted += std::sqrt(voiceBSm) * shifter[1].process(x);
		shifted = shimmerHP.process(shimmerLP.process(shifted));

		// Add the shifted copy and normalise the power, so energy moves into the
		// upper layer without changing the overall decay time
		float k = amountSm;
		return (x + k * shifted) / std::sqrt(1.f + k * k);
	}

	void runHalf(Half& h, float x, float mod1, float mod2) {
		float a = h.ap1.process(x, h.ap1Delay + mod1);
		float d = h.del1.read((size_t) h.del1Delay);
		h.del1.write(a);
		// Hold bypasses the damping so the frozen sound does not fade
		float damped = h.damping.process(d);
		float b = d + (damped - d) * (1.f - holdMix);
		float c = h.ap2.process(b * decayGain, h.ap2Delay + mod2);
		h.del2.write(c);
	}

	static float softLimit(float x) {
		// Exactly linear below 1 (so hold is lossless), saturates smoothly above
		float a = std::fabs(x);
		if (a <= 1.f)
			return x;
		float y = 1.f + std::tanh(a - 1.f);
		return x < 0.f ? -y : y;
	}
};

} // namespace shimmer
