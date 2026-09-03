#include "sound.h"

#include "core.h"

#include <assert.h>
#include <math.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SAMPLE_RATE 44100
#define BUFFER_FRAMES 2048
#define CHANNELS 2
#define MAX_VOICES 12
#define TAU (2.0f * PI)
#define ARRAY_LEN(a) ((int)(sizeof(a) / sizeof((a)[0])))

typedef struct {
  float l, r;
} Stereo;

typedef enum {
  STYLE_SUNNY,
  STYLE_SKIES,
  STYLE_NIGHT,
  STYLE_MAZE,
  STYLE_BLOCKS,
  STYLE_UI,
  STYLE_CREDITS,
  STYLE_RESULTS,
  STYLE_COUNT
} SoundStyle;

static const float STYLE_PITCH[] = {1.0f,  1.12f, 0.88f, 1.02f,
                                    0.96f, 1.0f,  0.94f, 0.88f};
STATIC_ASSERT(ARRAY_LEN(STYLE_PITCH) == STYLE_COUNT, style_pitches_full);

typedef enum {
  INST_MALLET,
  INST_PLUCK,
  INST_AIR,
  INST_PAD,
  INST_BASS,
  INST_KEYS,
  INST_NEON,
  INST_BLOCK
} Instrument;

typedef struct MusicBuild MusicBuild;
typedef void (*ComposeFn)(MusicBuild *);

typedef struct {
  float bpm;
  int bars;
  int beatsPerBar;
  int rootMidi;
  SoundStyle style;
  float echo;
  float room;
  float targetRms;
  ComposeFn compose;
} Track;

struct MusicBuild {
  float *buf; // interleaved stereo
  int frames;
  const Track *track;
};

static int trackBeatFrames(const Track *track) {
  return (int)(SAMPLE_RATE * 60.0f / track->bpm + 0.5f);
}

static float fractf(float x) { return x - floorf(x); }

static unsigned hash32(unsigned x) {
  x ^= x >> 16;
  x *= 0x7feb352du;
  x ^= x >> 15;
  x *= 0x846ca68bu;
  x ^= x >> 16;
  return x;
}

static float noiseAt(unsigned seed, unsigned sample) {
  return (float)(hash32(seed ^ sample) & 0xffffu) / 32767.5f - 1.0f;
}

static float smoothNoise(unsigned seed, unsigned sample, unsigned period) {
  unsigned cell = sample / period;
  unsigned rem = sample % period;
  float f = (float)rem / period;
  f = f * f * (3.0f - 2.0f * f);
  float a = noiseAt(seed, cell);
  float b = noiseAt(seed, cell + 1u);
  return a + (b - a) * f;
}

#define SEMITONES_PER_OCTAVE 12
static const float SEMITONE_RATIO[] = {
    1.0f,       1.0594631f, 1.1224620f, 1.1892071f, 1.2599211f, 1.3348398f,
    1.4142136f, 1.4983071f, 1.5874011f, 1.6817928f, 1.7817974f, 1.8877486f,
};
STATIC_ASSERT(ARRAY_LEN(SEMITONE_RATIO) == SEMITONES_PER_OCTAVE,
              semitone_ratios_full);

static float midiHz(int midi) {
  int delta = midi - 69;
  int octave =
      delta >= 0
          ? delta / SEMITONES_PER_OCTAVE
          : -((-delta + SEMITONES_PER_OCTAVE - 1) / SEMITONES_PER_OCTAVE);
  int note = delta - octave * SEMITONES_PER_OCTAVE;
  return ldexpf(440.0f * SEMITONE_RATIO[note], octave);
}

static float polyBlep(float phase, float step) {
  if (phase < step) {
    float t = phase / step;
    return t + t - t * t - 1.0f;
  }
  if (phase > 1.0f - step) {
    float t = (phase - 1.0f) / step;
    return t * t + t + t + 1.0f;
  }
  return 0.0f;
}

static float sawWave(float cycles, float hz) {
  float p = fractf(cycles);
  return 2.0f * p - 1.0f - polyBlep(p, hz / SAMPLE_RATE);
}

static float pulseWave(float cycles, float hz, float duty) {
  float p = fractf(cycles);
  float out = p < duty ? 1.0f : -1.0f;
  out += polyBlep(p, hz / SAMPLE_RATE);
  float edge = fractf(p - duty);
  out -= polyBlep(edge, hz / SAMPLE_RATE);
  return out;
}

static float triangleWave(float cycles) {
  float p = fractf(cycles);
  return 1.0f - 4.0f * fabsf(p - 0.5f);
}

// Integrate a sinusoidal frequency deviation instead of multiplying f*t by a
// changing frequency. That keeps vibrato centered and free of pitch drift.
static float vibratoCycles(float hz, float t, float rate, float depth) {
  if (rate <= 0.0f || depth <= 0.0f) {
    return hz * t;
  }
  return hz * t + hz * depth * (1.0f - cosf(TAU * rate * t)) / (TAU * rate);
}

static float styleWave(SoundStyle style, float cycles, float hz, int sample,
                       unsigned seed) {
  float sine = sinf(TAU * cycles);
  switch (style) {
  case STYLE_SUNNY:
    return 0.68f * sine + 0.2f * sinf(TAU * cycles * 2.0f) +
           0.12f * triangleWave(cycles);
  case STYLE_SKIES:
    return 0.78f * sine + 0.17f * sinf(TAU * cycles * 2.01f + 0.35f * sine) +
           0.05f * smoothNoise(seed, (unsigned)sample, 37u);
  case STYLE_NIGHT:
    return 0.55f * triangleWave(cycles) + 0.35f * sine +
           0.1f * sinf(TAU * cycles * 3.0f);
  case STYLE_MAZE:
    return 0.5f * pulseWave(cycles, hz, 0.28f) + 0.3f * sawWave(cycles, hz) +
           0.2f * sine;
  case STYLE_BLOCKS: {
    float digital = 0.62f * pulseWave(cycles, hz, 0.5f) + 0.38f * sine;
    return floorf(digital * 18.0f + 0.5f) / 18.0f;
  }
  case STYLE_CREDITS:
    return 0.72f * sine + 0.2f * triangleWave(cycles) +
           0.08f * sinf(TAU * cycles * 2.0f);
  case STYLE_RESULTS:
    return 0.82f * sine + 0.18f * triangleWave(cycles);
  case STYLE_UI:
    return 0.62f * sine + 0.28f * triangleWave(cycles) +
           0.1f * sinf(TAU * cycles * 2.0f);
  case STYLE_COUNT:
    break;
  }
  return 0.0f;
}

static float heldEnvelope(float t, float attack, float decay, float sustain) {
  if (t < attack) {
    return t / attack;
  }
  if (t < attack + decay) {
    float f = (t - attack) / decay;
    return 1.0f + (sustain - 1.0f) * f;
  }
  return sustain;
}

static float adsr(float t, float gate, float attack, float decay, float sustain,
                  float release) {
  float atGate = heldEnvelope(gate, attack, decay, sustain);
  if (t <= gate) {
    return heldEnvelope(t, attack, decay, sustain);
  }
  float f = (t - gate) / release;
  if (f >= 1.0f) {
    return 0.0f;
  }
  float tail = 1.0f - f;
  return atGate * tail * tail;
}

static float instrumentRelease(Instrument inst) {
  switch (inst) {
  case INST_PAD:
    return 0.9f;
  case INST_AIR:
    return 0.55f;
  case INST_KEYS:
    return 0.38f;
  case INST_MALLET:
    return 0.3f;
  case INST_PLUCK:
    return 0.18f;
  default:
    return 0.14f;
  }
}

static float instrumentSample(Instrument inst, float hz, float t, float gate,
                              int sample, unsigned seed) {
  float cycles;
  float env;
  float n = noiseAt(seed, (unsigned)sample);

  switch (inst) {
  case INST_MALLET:
    cycles = vibratoCycles(hz, t, 5.1f, 0.001f);
    env = (1.0f - expf(-t * 300.0f)) * expf(-t * 3.8f) *
          adsr(t, gate, 0.003f, 0.08f, 0.65f, 0.3f);
    return env *
           (0.72f * sinf(TAU * cycles) + 0.2f * sinf(TAU * cycles * 2.01f) +
            0.08f * sinf(TAU * cycles * 3.97f));

  case INST_PLUCK:
    cycles = vibratoCycles(hz, t, 4.8f, 0.0015f);
    env = (1.0f - expf(-t * 500.0f)) * expf(-t * 4.5f) *
          adsr(t, gate, 0.002f, 0.08f, 0.5f, 0.18f);
    return env * (0.48f * triangleWave(cycles) +
                  0.32f * pulseWave(cycles, hz, 0.42f) +
                  0.17f * sinf(TAU * cycles) + 0.03f * n);

  case INST_AIR:
    cycles = vibratoCycles(hz, t, 4.2f, 0.0035f);
    env = adsr(t, gate, 0.075f, 0.18f, 0.78f, 0.55f);
    return env *
           (0.82f * sinf(TAU * cycles) + 0.13f * sinf(TAU * cycles * 2.0f) +
            0.05f * smoothNoise(seed, (unsigned)sample, 53u));

  case INST_PAD:
    cycles = vibratoCycles(hz, t, 0.32f, 0.0015f);
    env = adsr(t, gate, 0.32f, 0.45f, 0.68f, 0.9f);
    return env * (0.44f * sawWave(cycles, hz) +
                  0.32f * sawWave(cycles * 1.003f, hz * 1.003f) +
                  0.24f * sinf(TAU * cycles));

  case INST_BASS:
    cycles = vibratoCycles(hz, t, 4.0f, 0.0007f);
    env = adsr(t, gate, 0.006f, 0.12f, 0.72f, 0.14f);
    return env * (0.58f * sinf(TAU * cycles) + 0.3f * triangleWave(cycles) +
                  0.12f * sawWave(cycles, hz));

  case INST_KEYS: {
    cycles = vibratoCycles(hz, t, 5.0f, 0.001f);
    env = adsr(t, gate, 0.006f, 0.32f, 0.42f, 0.38f);
    float mod = 1.7f * expf(-t * 2.8f) * sinf(TAU * cycles * 2.0f);
    return env * (0.78f * sinf(TAU * cycles + mod) +
                  0.22f * sinf(TAU * cycles * 0.5f));
  }

  case INST_NEON:
    cycles = vibratoCycles(hz, t, 5.5f, 0.002f);
    env = adsr(t, gate, 0.004f, 0.09f, 0.58f, 0.14f);
    return env *
           (0.48f * pulseWave(cycles, hz, 0.25f) + 0.28f * sawWave(cycles, hz) +
            0.24f * sinf(TAU * cycles + 0.8f * sinf(TAU * cycles * 2.0f)));

  case INST_BLOCK:
  default: {
    cycles = vibratoCycles(hz, t, 6.0f, 0.001f);
    env = adsr(t, gate, 0.003f, 0.1f, 0.62f, 0.12f);
    float s = 0.58f * pulseWave(cycles, hz, 0.5f) +
              0.27f * sinf(TAU * cycles + 0.65f * sinf(TAU * cycles * 1.5f)) +
              0.15f * triangleWave(cycles);
    return env * floorf(s * 20.0f + 0.5f) / 20.0f;
  }
  }
}

static int frameAtBeat(const MusicBuild *b, float beat) {
  return (int)(beat * 60.0f / b->track->bpm * SAMPLE_RATE + 0.5f);
}

static int wrapFrame(int frame, int frames) {
  frame %= frames;
  return frame < 0 ? frame + frames : frame;
}

static void addNote(MusicBuild *b, Instrument inst, int midi, float beat,
                    float beats, float gain, float pan, unsigned seed) {
  int start = frameAtBeat(b, beat);
  float gate = beats * 60.0f / b->track->bpm;
  float release = instrumentRelease(inst);
  int count = (int)((gate + release) * SAMPLE_RATE + 0.5f);
  pan = clampf(pan, -1.0f, 1.0f);
  float angle = (pan + 1.0f) * PI * 0.25f;
  float left = cosf(angle), right = sinf(angle);
  float hz = midiHz(midi);
  bool wide = inst == INST_PAD || inst == INST_AIR;
  float hzLeft = wide ? hz * 0.99798f : hz;
  float hzRight = wide ? hz * 1.00202f : hz;

  for (int i = 0; i < count; i++) {
    float t = (float)i / SAMPLE_RATE;
    int frame = wrapFrame(start + i, b->frames);
    float l = instrumentSample(inst, hzLeft, t, gate, i, seed);
    float r =
        wide ? instrumentSample(inst, hzRight, t, gate, i, seed + 0x91u) : l;
    b->buf[frame * 2] += gain * left * l;
    b->buf[frame * 2 + 1] += gain * right * r;
  }
}

#define SCALE_DEGREES 7
static const int SCALE_MAJOR[] = {0, 2, 4, 5, 7, 9, 11};
static const int SCALE_LYDIAN[] = {0, 2, 4, 6, 7, 9, 11};
static const int SCALE_MINOR[] = {0, 2, 3, 5, 7, 8, 10};
static const int SCALE_DORIAN[] = {0, 2, 3, 5, 7, 9, 10};
static const int SCALE_HARMONIC_MINOR[] = {0, 2, 3, 5, 7, 8, 11};
STATIC_ASSERT(ARRAY_LEN(SCALE_MAJOR) == SCALE_DEGREES, major_scale_full);
STATIC_ASSERT(ARRAY_LEN(SCALE_LYDIAN) == SCALE_DEGREES, lydian_scale_full);
STATIC_ASSERT(ARRAY_LEN(SCALE_MINOR) == SCALE_DEGREES, minor_scale_full);
STATIC_ASSERT(ARRAY_LEN(SCALE_DORIAN) == SCALE_DEGREES, dorian_scale_full);
STATIC_ASSERT(ARRAY_LEN(SCALE_HARMONIC_MINOR) == SCALE_DEGREES,
              harmonic_minor_scale_full);

// Harmonic plans are shared config rather than hidden inside each composer.
// TRACKS derives each loop's bar count from these arrays, so score length and
// progression indexing cannot drift apart. MENU and SHOP intentionally reuse
// one plan to keep their synchronized playheads harmonically aligned.
static const signed char PROG_SUNNY[] = {0, 4, 5, 3, 0, 2, 3, 4, 5, 3, 4, 0};
static const signed char PROG_SKIES[] = {0, 1, 3, 0, 5, 1, 4, 0, 3, 1, 4, 0};
static const signed char PROG_NIGHT[] = {0, 3, 6, 4, 0, 5, 3, 4};
static const signed char PROG_MAZE[] = {0, 0, 3, 4, 0, 5, 3, 4, 0, 6, 3, 4};
static const signed char PROG_BLOCKS[] = {0, 5, 3, 4, 0, 6, 3, 4, 5, 3, 4, 0};
static const signed char PROG_MENU[] = {0, 3, 5, 4, 0, 1, 3, 4};
static const signed char PROG_CREDITS[] = {0, 5, 3, 4, 1, 3, 4, 0};
static const signed char PROG_RESULTS[] = {0, 5, 3, 4};

static int degreeMidi(const int *scale, int tonic, int degree) {
  int octave;
  if (degree >= 0) {
    octave = degree / SCALE_DEGREES;
  } else {
    octave = -((-degree + SCALE_DEGREES - 1) / SCALE_DEGREES);
  }
  int index = degree - octave * SCALE_DEGREES;
  return tonic + octave * SEMITONES_PER_OCTAVE + scale[index];
}

static void addChord(MusicBuild *b, const int *scale, int tonic, int degree,
                     float beat, float beats, Instrument inst, float gain,
                     unsigned seed) {
  static const float PAN[] = {-0.42f, 0.05f, 0.42f};
  for (int i = 0; i < 3; i++) {
    addNote(b, inst, degreeMidi(scale, tonic, degree + i * 2), beat, beats,
            gain, PAN[i], seed + (unsigned)i * 17u);
  }
}

typedef struct {
  signed char degree;
  unsigned char step;
  unsigned char length;
} MotifNote;

// Original eight-note "Boing" cell. Every cue reshapes this same upward leap,
// giving the soundtrack one identity without forcing one genre on every era.
static const MotifNote MOTIF[] = {{0, 0, 2},  {2, 2, 2},  {4, 4, 3},
                                  {5, 7, 2},  {4, 10, 2}, {6, 12, 1},
                                  {2, 13, 1}, {1, 14, 2}};

static void addMotif(MusicBuild *b, const int *scale, int tonic, float beat,
                     float stretch, int degreeShift, Instrument inst,
                     float gain, float pan, unsigned seed) {
  for (int i = 0; i < ARRAY_LEN(MOTIF); i++) {
    float start = beat + MOTIF[i].step * 0.25f * stretch;
    float len = MOTIF[i].length * 0.22f * stretch;
    float spread = (i & 1) ? 0.16f : -0.16f;
    addNote(b, inst, degreeMidi(scale, tonic, MOTIF[i].degree + degreeShift),
            start, len, gain * (i == 0 || i == 3 ? 1.08f : 1.0f),
            clampf(pan + spread, -0.8f, 0.8f), seed + (unsigned)i * 31u);
  }
}

typedef enum {
  DRUM_KICK,
  DRUM_SNARE,
  DRUM_HAT,
  DRUM_CLAP,
  DRUM_SHAKER,
  DRUM_TOM
} DrumKind;

static float sweepCycles(float startHz, float endHz, float t, float duration) {
  return startHz * t + 0.5f * (endHz - startHz) * t * t / duration;
}

static float drumSample(DrumKind kind, float t, float duration, int sample,
                        unsigned seed) {
  float x = t / duration;
  float attack = sample < 24 ? (float)sample / 24.0f : 1.0f;
  float n = noiseAt(seed, (unsigned)sample);
  float hp = n - noiseAt(seed, (unsigned)(sample > 0 ? sample - 1 : 0));

  switch (kind) {
  case DRUM_KICK: {
    float env = attack * expf(-x * 7.5f);
    float phase = sweepCycles(165.0f, 48.0f, t, duration);
    return env * (0.9f * sinf(TAU * phase) + 0.1f * n * expf(-x * 25.0f));
  }
  case DRUM_SNARE: {
    float env = attack * expf(-x * 9.0f);
    return env * (0.72f * hp + 0.28f * sinf(TAU * 185.0f * t));
  }
  case DRUM_HAT:
    return attack * expf(-x * 15.0f) *
           (0.72f * hp + 0.28f * pulseWave(4210.0f * t, 4210.0f, 0.37f));
  case DRUM_CLAP: {
    float burst = expf(-fmodf(t, 0.018f) * 85.0f);
    return attack * expf(-x * 8.0f) * burst * hp;
  }
  case DRUM_SHAKER:
    return attack * expf(-x * 12.0f) *
           (0.65f * hp + 0.35f * smoothNoise(seed, (unsigned)sample, 11u));
  case DRUM_TOM:
  default: {
    float env = attack * expf(-x * 6.0f);
    float phase = sweepCycles(225.0f, 105.0f, t, duration);
    return env * (0.84f * sinf(TAU * phase) + 0.16f * n);
  }
  }
}

static float drumDuration(DrumKind kind) {
  switch (kind) {
  case DRUM_KICK:
    return 0.22f;
  case DRUM_SNARE:
  case DRUM_CLAP:
    return 0.18f;
  case DRUM_HAT:
    return 0.07f;
  case DRUM_SHAKER:
    return 0.1f;
  case DRUM_TOM:
  default:
    return 0.25f;
  }
}

static void addDrum(MusicBuild *b, DrumKind kind, float beat, float gain,
                    float pan, unsigned seed) {
  float duration = drumDuration(kind);
  int count = (int)(duration * SAMPLE_RATE + 0.5f);
  int start = frameAtBeat(b, beat);
  pan = clampf(pan, -1.0f, 1.0f);
  float angle = (pan + 1.0f) * PI * 0.25f;
  float left = cosf(angle), right = sinf(angle);
  for (int i = 0; i < count; i++) {
    int frame = wrapFrame(start + i, b->frames);
    float value =
        gain * drumSample(kind, (float)i / SAMPLE_RATE, duration, i, seed);
    b->buf[frame * 2] += left * value;
    b->buf[frame * 2 + 1] += right * value;
  }
}

static void addPopDrums(MusicBuild *b, float beat, float amount,
                        unsigned seed) {
  addDrum(b, DRUM_KICK, beat, 0.5f * amount, 0.0f, seed);
  addDrum(b, DRUM_KICK, beat + 2.0f, 0.42f * amount, 0.0f, seed + 1u);
  addDrum(b, DRUM_SNARE, beat + 1.0f, 0.31f * amount, 0.04f, seed + 2u);
  addDrum(b, DRUM_SNARE, beat + 3.0f, 0.34f * amount, -0.04f, seed + 3u);
  for (int i = 0; i < 8; i++) {
    addDrum(b, DRUM_SHAKER, beat + i * 0.5f, 0.09f * amount,
            (i & 1) ? 0.48f : -0.48f, seed + 10u + (unsigned)i);
  }
}

static void addElectroDrums(MusicBuild *b, float beat, float amount,
                            unsigned seed) {
  for (int i = 0; i < 4; i++) {
    addDrum(b, DRUM_KICK, beat + i, 0.5f * amount, 0.0f, seed + (unsigned)i);
  }
  addDrum(b, DRUM_CLAP, beat + 1.0f, 0.3f * amount, 0.12f, seed + 9u);
  addDrum(b, DRUM_CLAP, beat + 3.0f, 0.34f * amount, -0.12f, seed + 10u);
  for (int i = 0; i < 8; i++) {
    addDrum(b, DRUM_HAT, beat + i * 0.5f + 0.25f, 0.1f * amount,
            (i & 1) ? 0.55f : -0.55f, seed + 20u + (unsigned)i);
  }
}

static void addBreakDrums(MusicBuild *b, float beat, float amount,
                          unsigned seed) {
  static const float KICKS[] = {0.0f, 1.5f, 2.75f};
  static const float SNARES[] = {1.0f, 3.0f};
  for (int i = 0; i < ARRAY_LEN(KICKS); i++) {
    addDrum(b, DRUM_KICK, beat + KICKS[i], 0.56f * amount, 0.0f,
            seed + (unsigned)i);
  }
  for (int i = 0; i < ARRAY_LEN(SNARES); i++) {
    addDrum(b, DRUM_SNARE, beat + SNARES[i], 0.36f * amount, i ? 0.1f : -0.1f,
            seed + 6u + (unsigned)i);
  }
  for (int i = 0; i < 16; i++) {
    addDrum(b, DRUM_HAT, beat + i * 0.25f, 0.075f * amount,
            (i & 1) ? 0.62f : -0.62f, seed + 20u + (unsigned)i);
  }
}

static void composeSunny(MusicBuild *b) {
  int tonic = b->track->rootMidi;
  for (int bar = 0; bar < b->track->bars; bar++) {
    float at = bar * 4.0f;
    int degree = PROG_SUNNY[bar];
    addChord(b, SCALE_MAJOR, tonic, degree, at, 3.72f, INST_PAD, 0.055f,
             100u + (unsigned)bar);
    for (int i = 0; i < 8; i++) {
      int chordDegree = degree + (i % 3) * 2;
      addNote(b, INST_PLUCK, degreeMidi(SCALE_MAJOR, tonic, chordDegree),
              at + i * 0.5f, 0.28f, 0.105f, (i & 1) ? 0.44f : -0.44f,
              300u + (unsigned)(bar * 8 + i));
    }
    addNote(b, INST_BASS, degreeMidi(SCALE_MAJOR, tonic - 24, degree), at,
            1.65f, 0.22f, 0.0f, 500u + (unsigned)bar);
    addNote(b, INST_BASS, degreeMidi(SCALE_MAJOR, tonic - 24, degree + 4),
            at + 2.0f, 1.55f, 0.18f, 0.0f, 520u + (unsigned)bar);
    addPopDrums(b, at, bar < 4 ? 0.78f : 1.0f, 700u + (unsigned)bar * 31u);
  }
  addMotif(b, SCALE_MAJOR, tonic + 12, 0.0f, 1.0f, 0, INST_MALLET, 0.21f,
           -0.12f, 900u);
  addMotif(b, SCALE_MAJOR, tonic + 12, 16.0f, 1.0f, 1, INST_MALLET, 0.2f, 0.12f,
           1000u);
  addMotif(b, SCALE_MAJOR, tonic + 12, 32.0f, 1.0f, 0, INST_MALLET, 0.23f, 0.0f,
           1100u);
  static const int RESPONSE[] = {4, 3, 2, 0};
  for (int phrase = 0; phrase < 3; phrase++) {
    float at = 8.0f + phrase * 16.0f;
    for (int i = 0; i < ARRAY_LEN(RESPONSE); i++) {
      addNote(b, INST_AIR, degreeMidi(SCALE_MAJOR, tonic + 12, RESPONSE[i]),
              at + i * 0.5f, 0.38f, 0.12f, 0.34f,
              1200u + (unsigned)(phrase * 8 + i));
    }
  }
}

static void composeSkies(MusicBuild *b) {
  int tonic = b->track->rootMidi;
  for (int bar = 0; bar < b->track->bars; bar++) {
    float at = bar * 3.0f;
    int degree = PROG_SKIES[bar];
    addChord(b, SCALE_LYDIAN, tonic, degree, at, 2.78f, INST_PAD, 0.065f,
             1300u + (unsigned)bar);
    addNote(b, INST_BASS, degreeMidi(SCALE_LYDIAN, tonic - 24, degree), at,
            2.55f, 0.13f, 0.0f, 1400u + (unsigned)bar);
    for (int i = 0; i < 6; i++) {
      int d = degree + (i % 3) * 2;
      addNote(b, INST_PLUCK, degreeMidi(SCALE_LYDIAN, tonic + 12, d),
              at + i * 0.5f, 0.34f, 0.09f, (i & 1) ? 0.58f : -0.58f,
              1500u + (unsigned)(bar * 6 + i));
      addDrum(b, DRUM_SHAKER, at + i * 0.5f, 0.045f, (i & 1) ? 0.7f : -0.7f,
              1600u + (unsigned)(bar * 6 + i));
    }
    if (bar % 4 == 2) {
      static const int CALL[] = {7, 9, 8};
      for (int i = 0; i < ARRAY_LEN(CALL); i++) {
        addNote(b, INST_AIR, degreeMidi(SCALE_LYDIAN, tonic + 12, CALL[i]),
                at + 1.35f + i * 0.34f, 0.25f, 0.13f, i & 1 ? -0.55f : 0.55f,
                1700u + (unsigned)(bar * 4 + i));
      }
    }
  }
  addMotif(b, SCALE_LYDIAN, tonic + 12, 0.0f, 0.75f, 0, INST_AIR, 0.18f, -0.25f,
           1800u);
  addMotif(b, SCALE_LYDIAN, tonic + 12, 12.0f, 0.75f, 1, INST_AIR, 0.17f, 0.25f,
           1900u);
  addMotif(b, SCALE_LYDIAN, tonic + 12, 24.0f, 0.75f, 0, INST_AIR, 0.2f, 0.0f,
           2000u);
}

static void composeNight(MusicBuild *b) {
  int tonic = b->track->rootMidi;
  for (int bar = 0; bar < b->track->bars; bar++) {
    float at = bar * 4.0f;
    int degree = PROG_NIGHT[bar];
    addChord(b, SCALE_DORIAN, tonic, degree, at, 3.82f, INST_PAD, 0.075f,
             2100u + (unsigned)bar);
    addNote(b, INST_BASS, degreeMidi(SCALE_DORIAN, tonic - 24, degree), at,
            3.45f, 0.15f, 0.0f, 2200u + (unsigned)bar);
    addNote(b, INST_KEYS, degreeMidi(SCALE_DORIAN, tonic + 12, degree + 4),
            at + 2.5f, 0.8f, 0.075f, (bar & 1) ? 0.38f : -0.38f,
            2300u + (unsigned)bar);
    if ((bar & 1) == 0) {
      addDrum(b, DRUM_TOM, at, 0.12f, -0.08f, 2400u + (unsigned)bar);
    }
    addDrum(b, DRUM_SHAKER, at + 3.5f, 0.035f, (bar & 1) ? 0.62f : -0.62f,
            2500u + (unsigned)bar);
  }
  addMotif(b, SCALE_DORIAN, tonic + 12, 0.0f, 2.0f, 0, INST_KEYS, 0.15f, -0.18f,
           2600u);
  addMotif(b, SCALE_DORIAN, tonic + 12, 16.0f, 2.0f, -1, INST_KEYS, 0.16f,
           0.18f, 2700u);
  for (int bar = 1; bar < b->track->bars; bar += 2) {
    float at = bar * 4.0f;
    addNote(b, INST_MALLET, degreeMidi(SCALE_DORIAN, tonic + 24, bar % 7),
            at + 1.5f, 0.55f, 0.09f, (bar & 2) ? 0.58f : -0.58f,
            2800u + (unsigned)bar);
  }
}

static void composeMaze(MusicBuild *b) {
  static const float BASS_AT[] = {0.0f, 0.75f, 1.5f, 2.0f, 2.75f, 3.5f};
  int tonic = b->track->rootMidi;
  for (int bar = 0; bar < b->track->bars; bar++) {
    float at = bar * 4.0f;
    int degree = PROG_MAZE[bar];
    for (int i = 0; i < ARRAY_LEN(BASS_AT); i++) {
      int d = degree + ((i == 2 || i == 5) ? 4 : 0);
      addNote(b, INST_BASS, degreeMidi(SCALE_MINOR, tonic - 24, d),
              at + BASS_AT[i], 0.42f, 0.2f, 0.0f,
              2900u + (unsigned)(bar * 8 + i));
    }
    for (int i = 0; i < 4; i++) {
      addChord(b, SCALE_MINOR, tonic, degree, at + 0.5f + i, 0.24f, INST_NEON,
               0.065f, 3100u + (unsigned)(bar * 4 + i));
    }
    addElectroDrums(b, at, bar < 2 ? 0.78f : 1.0f, 3300u + (unsigned)bar * 29u);
  }
  addMotif(b, SCALE_MINOR, tonic + 12, 0.0f, 1.0f, 0, INST_NEON, 0.17f, -0.18f,
           3500u);
  addMotif(b, SCALE_MINOR, tonic + 12, 16.0f, 1.0f, 2, INST_NEON, 0.17f, 0.18f,
           3600u);
  addMotif(b, SCALE_MINOR, tonic + 12, 32.0f, 1.0f, 0, INST_NEON, 0.19f, 0.0f,
           3700u);
  for (int bar = 2; bar < b->track->bars; bar += 4) {
    float at = bar * 4.0f;
    for (int i = 0; i < 6; i++) {
      addNote(b, INST_NEON,
              degreeMidi(SCALE_MINOR, tonic + 24, (i * 2 + bar) % 7),
              at + i * 0.25f, 0.16f, 0.08f, (i & 1) ? 0.6f : -0.6f,
              3800u + (unsigned)(bar * 8 + i));
    }
  }
}

static void composeBlocks(MusicBuild *b) {
  int tonic = b->track->rootMidi;
  for (int bar = 0; bar < b->track->bars; bar++) {
    float at = bar * 4.0f;
    int degree = PROG_BLOCKS[bar];
    addChord(b, SCALE_HARMONIC_MINOR, tonic, degree, at + 0.25f, 0.45f,
             INST_BLOCK, 0.075f, 3900u + (unsigned)bar);
    addChord(b, SCALE_HARMONIC_MINOR, tonic, degree, at + 2.25f, 0.45f,
             INST_BLOCK, 0.065f, 4000u + (unsigned)bar);
    addNote(b, INST_BASS, degreeMidi(SCALE_HARMONIC_MINOR, tonic - 24, degree),
            at, 0.7f, 0.22f, 0.0f, 4100u + (unsigned)bar);
    addNote(b, INST_BASS,
            degreeMidi(SCALE_HARMONIC_MINOR, tonic - 24, degree + 4), at + 2.0f,
            0.7f, 0.2f, 0.0f, 4200u + (unsigned)bar);
    for (int i = 0; i < 16; i++) {
      int d = degree + ((i % 4) == 3 ? 6 : (i % 3) * 2);
      addNote(b, INST_BLOCK, degreeMidi(SCALE_HARMONIC_MINOR, tonic + 12, d),
              at + i * 0.25f, 0.14f, 0.042f, (i & 1) ? 0.52f : -0.52f,
              4300u + (unsigned)(bar * 16 + i));
    }
    addBreakDrums(b, at, bar < 2 ? 0.8f : 1.0f, 4600u + (unsigned)bar * 37u);
  }
  addMotif(b, SCALE_HARMONIC_MINOR, tonic + 12, 0.0f, 0.75f, 0, INST_BLOCK,
           0.15f, -0.16f, 4800u);
  addMotif(b, SCALE_HARMONIC_MINOR, tonic + 12, 16.0f, 0.75f, 1, INST_BLOCK,
           0.15f, 0.16f, 4900u);
  addMotif(b, SCALE_HARMONIC_MINOR, tonic + 12, 32.0f, 0.75f, 0, INST_BLOCK,
           0.17f, 0.0f, 5000u);
}

static void composeMenu(MusicBuild *b) {
  int tonic = b->track->rootMidi;
  for (int bar = 0; bar < b->track->bars; bar++) {
    float at = bar * 4.0f;
    int degree = PROG_MENU[bar];
    addChord(b, SCALE_MAJOR, tonic, degree, at, 3.8f, INST_PAD, 0.065f,
             5100u + (unsigned)bar);
    addNote(b, INST_BASS, degreeMidi(SCALE_MAJOR, tonic - 24, degree), at,
            1.75f, 0.17f, 0.0f, 5200u + (unsigned)bar);
    addNote(b, INST_BASS, degreeMidi(SCALE_MAJOR, tonic - 24, degree + 4),
            at + 2.0f, 1.7f, 0.14f, 0.0f, 5300u + (unsigned)bar);
    addDrum(b, DRUM_KICK, at, 0.22f, 0.0f, 5400u + (unsigned)bar);
    addDrum(b, DRUM_CLAP, at + 2.0f, 0.1f, 0.15f, 5500u + (unsigned)bar);
    for (int i = 0; i < 4; i++) {
      addDrum(b, DRUM_SHAKER, at + i + 0.5f, 0.045f, (i & 1) ? 0.5f : -0.5f,
              5600u + (unsigned)(bar * 4 + i));
    }
  }
  addMotif(b, SCALE_MAJOR, tonic, 0.0f, 1.0f, 0, INST_KEYS, 0.2f, -0.1f, 5700u);
  addMotif(b, SCALE_MAJOR, tonic, 16.0f, 1.0f, 1, INST_KEYS, 0.19f, 0.1f,
           5800u);
  for (int bar = 2; bar < b->track->bars; bar += 4) {
    float at = bar * 4.0f;
    static const int ANSWER[] = {4, 3, 1, 0};
    for (int i = 0; i < ARRAY_LEN(ANSWER); i++) {
      addNote(b, INST_MALLET, degreeMidi(SCALE_MAJOR, tonic + 12, ANSWER[i]),
              at + i * 0.75f, 0.48f, 0.09f, (i & 1) ? 0.42f : -0.42f,
              5900u + (unsigned)(bar * 4 + i));
    }
  }
}

static void composeShop(MusicBuild *b) {
  int tonic = b->track->rootMidi;
  for (int bar = 0; bar < b->track->bars; bar++) {
    float at = bar * 4.0f;
    int degree = PROG_MENU[bar];
    static const float STABS[] = {0.0f, 0.75f, 1.5f, 2.5f, 3.25f};
    for (int i = 0; i < ARRAY_LEN(STABS); i++) {
      addChord(b, SCALE_MAJOR, tonic, degree, at + STABS[i], 0.22f, INST_PLUCK,
               0.07f, 6100u + (unsigned)(bar * 8 + i));
    }
    for (int i = 0; i < 4; i++) {
      int d = degree + (i == 3 ? 6 : i * 2);
      addNote(b, INST_BASS, degreeMidi(SCALE_MAJOR, tonic - 24, d), at + i,
              0.62f, 0.15f, 0.0f, 6300u + (unsigned)(bar * 4 + i));
      addDrum(b, DRUM_SHAKER, at + i + 0.5f, 0.06f, (i & 1) ? 0.58f : -0.58f,
              6400u + (unsigned)(bar * 4 + i));
    }
    addDrum(b, DRUM_KICK, at, 0.2f, 0.0f, 6500u + (unsigned)bar);
    addDrum(b, DRUM_CLAP, at + 2.0f, 0.13f, -0.12f, 6600u + (unsigned)bar);
  }
  addMotif(b, SCALE_MAJOR, tonic + 12, 0.0f, 1.0f, 0, INST_MALLET, 0.16f,
           -0.16f, 6700u);
  addMotif(b, SCALE_MAJOR, tonic + 12, 16.0f, 1.0f, 1, INST_MALLET, 0.16f,
           0.16f, 6800u);
  for (int bar = 2; bar < b->track->bars; bar += 2) {
    float at = bar * 4.0f;
    addNote(b, INST_MALLET, degreeMidi(SCALE_MAJOR, tonic + 24, bar % 7),
            at + 3.5f, 0.22f, 0.075f, (bar & 2) ? 0.62f : -0.62f,
            6900u + (unsigned)bar);
  }
}

static void composeCredits(MusicBuild *b) {
  int tonic = b->track->rootMidi;
  for (int bar = 0; bar < b->track->bars; bar++) {
    float at = bar * 4.0f;
    int degree = PROG_CREDITS[bar];
    addChord(b, SCALE_MAJOR, tonic, degree, at, 3.86f, INST_PAD, 0.082f,
             7000u + (unsigned)bar);
    addNote(b, INST_BASS, degreeMidi(SCALE_MAJOR, tonic - 24, degree), at, 3.5f,
            0.12f, 0.0f, 7100u + (unsigned)bar);
    addNote(b, INST_KEYS, degreeMidi(SCALE_MAJOR, tonic, degree), at, 1.6f,
            0.105f, -0.25f, 7200u + (unsigned)bar);
    addNote(b, INST_KEYS, degreeMidi(SCALE_MAJOR, tonic, degree + 4), at + 2.0f,
            1.55f, 0.09f, 0.25f, 7300u + (unsigned)bar);
    if (bar >= 4) {
      addDrum(b, DRUM_SHAKER, at + 1.5f, 0.035f, -0.52f, 7400u + (unsigned)bar);
      addDrum(b, DRUM_SHAKER, at + 3.5f, 0.035f, 0.52f, 7500u + (unsigned)bar);
    }
  }
  addMotif(b, SCALE_MAJOR, tonic + 12, 0.0f, 1.5f, 0, INST_AIR, 0.15f, -0.18f,
           7600u);
  addMotif(b, SCALE_MAJOR, tonic + 12, 16.0f, 1.5f, -1, INST_AIR, 0.16f, 0.18f,
           7700u);
  addNote(b, INST_MALLET, degreeMidi(SCALE_MAJOR, tonic + 24, 0), 30.0f, 1.5f,
          0.1f, 0.0f, 7800u);
}

static void composeResults(MusicBuild *b) {
  int tonic = b->track->rootMidi;
  for (int bar = 0; bar < b->track->bars; bar++) {
    float at = bar * 4.0f;
    int degree = PROG_RESULTS[bar];
    addChord(b, SCALE_MINOR, tonic, degree, at, 3.86f, INST_PAD, 0.07f,
             7900u + (unsigned)bar);
    addNote(b, INST_BASS, degreeMidi(SCALE_MINOR, tonic - 24, degree), at,
            3.45f, 0.1f, 0.0f, 8000u + (unsigned)bar);
    static const int FALL[] = {4, 2, 1, 0};
    for (int i = 0; i < ARRAY_LEN(FALL); i++) {
      addNote(b, INST_KEYS,
              degreeMidi(SCALE_MINOR, tonic + 12, FALL[i] + degree),
              at + i * 0.75f, 0.48f, 0.085f, (i & 1) ? 0.25f : -0.25f,
              8100u + (unsigned)(bar * 4 + i));
    }
  }
}

// Positional in MusicTrack order. MENU and SHOP deliberately share timing and
// harmony so setTrack() can preserve their exact playhead during navigation.
static const Track TRACKS[] = {
    {116.0f, ARRAY_LEN(PROG_SUNNY), 4, 62, STYLE_SUNNY, 0.1f, 0.1f, 0.125f,
     composeSunny},
    {96.0f, ARRAY_LEN(PROG_SKIES), 3, 64, STYLE_SKIES, 0.18f, 0.2f, 0.105f,
     composeSkies},
    {84.0f, ARRAY_LEN(PROG_NIGHT), 4, 59, STYLE_NIGHT, 0.22f, 0.28f, 0.1f,
     composeNight},
    {126.0f, ARRAY_LEN(PROG_MAZE), 4, 60, STYLE_MAZE, 0.08f, 0.08f, 0.13f,
     composeMaze},
    {142.0f, ARRAY_LEN(PROG_BLOCKS), 4, 64, STYLE_BLOCKS, 0.06f, 0.06f, 0.135f,
     composeBlocks},
    {104.0f, ARRAY_LEN(PROG_MENU), 4, 67, STYLE_UI, 0.14f, 0.16f, 0.11f,
     composeMenu},
    {104.0f, ARRAY_LEN(PROG_MENU), 4, 67, STYLE_UI, 0.12f, 0.12f, 0.115f,
     composeShop},
    {88.0f, ARRAY_LEN(PROG_CREDITS), 4, 62, STYLE_CREDITS, 0.2f, 0.28f, 0.1f,
     composeCredits},
    {72.0f, ARRAY_LEN(PROG_RESULTS), 4, 57, STYLE_RESULTS, 0.2f, 0.24f, 0.09f,
     composeResults},
};
STATIC_ASSERT(ARRAY_LEN(TRACKS) == TRACK_COUNT, tracks_full);

// Every adjacent gameplay pair gets one compact handoff recipe. All switch on
// an outgoing bar line; the beat counts only shape the fade on either side.
typedef enum {
  HANDOFF_AIR,
  HANDOFF_TWILIGHT,
  HANDOFF_SIGNAL,
  HANDOFF_FRAGMENT,
  HANDOFF_REBOOT,
  HANDOFF_COUNT
} HandoffColor;

typedef struct {
  MusicTrack from;
  MusicTrack to;
  float outBeats;
  float inBeats;
  HandoffColor color;
} Handoff;

static const Handoff HANDOFFS[] = {
    {TRACK_HILLS, TRACK_SKIES, 1.0f, 1.0f, HANDOFF_AIR},
    {TRACK_SKIES, TRACK_NIGHT, 1.0f, 1.0f, HANDOFF_TWILIGHT},
    {TRACK_NIGHT, TRACK_MAZE, 1.0f, 1.0f, HANDOFF_SIGNAL},
    {TRACK_MAZE, TRACK_BLOCKS, 1.0f, 1.0f, HANDOFF_FRAGMENT},
    {TRACK_BLOCKS, TRACK_HILLS, 2.0f, 1.0f, HANDOFF_REBOOT},
};
STATIC_ASSERT(ARRAY_LEN(HANDOFFS) == TRACK_GAMEPLAY_COUNT,
              gameplay_handoffs_full);
STATIC_ASSERT((int)HANDOFF_COUNT == (int)TRACK_GAMEPLAY_COUNT,
              handoff_colors_full);

static const int *const GAMEPLAY_SCALES[] = {
    SCALE_MAJOR, SCALE_LYDIAN, SCALE_DORIAN, SCALE_MINOR, SCALE_HARMONIC_MINOR,
};
STATIC_ASSERT(ARRAY_LEN(GAMEPLAY_SCALES) == TRACK_GAMEPLAY_COUNT,
              gameplay_scales_full);

static int handoffFor(int from, int to) {
  for (int i = 0; i < ARRAY_LEN(HANDOFFS); i++) {
    if ((int)HANDOFFS[i].from == from && (int)HANDOFFS[i].to == to) {
      return i;
    }
  }
  return -1;
}

static const MusicTrack PRELOAD_ORDER[] = {
    TRACK_MENU,   TRACK_HILLS, TRACK_SKIES,   TRACK_NIGHT,   TRACK_MAZE,
    TRACK_BLOCKS, TRACK_SHOP,  TRACK_CREDITS, TRACK_RESULTS,
};
STATIC_ASSERT(ARRAY_LEN(PRELOAD_ORDER) == TRACK_COUNT,
              audio_preload_order_full);

static int16_t *loopBuffer[TRACK_COUNT];
static int loopFrames[TRACK_COUNT];
static int loopPos[TRACK_COUNT];
static pthread_t preloadThread;
static pthread_mutex_t preloadLock = PTHREAD_MUTEX_INITIALIZER;
static bool preloadStarted;
static bool preloadFinished;
static bool preloadJoined;

static float dryAt(const float *buf, int frames, int frame, int channel) {
  frame = wrapFrame(frame, frames);
  return buf[frame * 2 + channel];
}

static const float ROOM_SEC[] = {0.043f, 0.071f, 0.113f, 0.173f};
static const float ROOM_GAIN[] = {0.2f, 0.15f, 0.11f, 0.08f};
STATIC_ASSERT(ARRAY_LEN(ROOM_SEC) == ARRAY_LEN(ROOM_GAIN), room_taps_aligned);

static void applySpace(float *buf, int frames, const Track *track) {
  if (track->echo <= 0.0f && track->room <= 0.0f) {
    return;
  }
  size_t bytes = (size_t)frames * CHANNELS * sizeof(float);
  float *dry = malloc(bytes);
  if (dry == NULL) {
    return;
  }
  memcpy(dry, buf, bytes);
  int beat = trackBeatFrames(track);
  int echo1 = beat * 3 / 4;
  int echo2 = beat * 3 / 2;

  for (int i = 0; i < frames; i++) {
    float l = dry[i * 2], r = dry[i * 2 + 1];
    l += track->echo * (0.42f * dryAt(dry, frames, i - echo1, 1) +
                        0.22f * dryAt(dry, frames, i - echo2, 0));
    r += track->echo * (0.42f * dryAt(dry, frames, i - echo1, 0) +
                        0.22f * dryAt(dry, frames, i - echo2, 1));
    for (int tap = 0; tap < ARRAY_LEN(ROOM_SEC); tap++) {
      int delay = (int)(ROOM_SEC[tap] * SAMPLE_RATE);
      float gl = dryAt(dry, frames, i - delay, tap & 1);
      float gr = dryAt(dry, frames, i - delay, (tap & 1) ^ 1);
      l += track->room * ROOM_GAIN[tap] * gl;
      r += track->room * ROOM_GAIN[tap] * gr;
    }
    buf[i * 2] = l;
    buf[i * 2 + 1] = r;
  }
  free(dry);
}

static void masterTrack(float *buf, int frames, const Track *track) {
  float prevIn[2] = {0.0f, 0.0f};
  float prevOut[2] = {0.0f, 0.0f};
  double sum = 0.0;
  const float driveScale = 1.0f / tanhf(1.15f);

  for (int i = 0; i < frames; i++) {
    for (int ch = 0; ch < 2; ch++) {
      int at = i * 2 + ch;
      float saturated = tanhf(buf[at] * 1.15f) * driveScale;
      float highpassed = saturated - prevIn[ch] + 0.995f * prevOut[ch];
      prevIn[ch] = saturated;
      prevOut[ch] = highpassed;
      buf[at] = highpassed;
      sum += (double)highpassed * highpassed;
    }
  }

  float rms = sqrtf((float)(sum / (frames * 2)));
  if (rms < 0.00001f) {
    return;
  }
  float gain = track->targetRms / rms;
  // A soft-knee master catches isolated synth transients without turning one
  // tall peak into a quieter whole song. Pulse-heavy eras need extra true-peak
  // margin because reconstruction can overshoot their individual samples.
  float ceiling = (track->style == STYLE_MAZE || track->style == STYLE_BLOCKS)
                      ? 0.47f
                      : (track->style == STYLE_SUNNY ? 0.56f : 0.64f);
  float knee = ceiling - 0.16f;
  for (int i = 0; i < frames * 2; i++) {
    float sample = buf[i] * gain;
    float mag = fabsf(sample);
    if (mag > knee) {
      mag = knee + (ceiling - knee) * tanhf((mag - knee) / (ceiling - knee));
      sample = copysignf(mag, sample);
    }
    buf[i] = sample;
  }
}

static void buildLoop(int track) {
  if (track < 0 || track >= TRACK_COUNT || loopBuffer[track] != NULL ||
      loopFrames[track] != 0) {
    return;
  }
  const Track *def = &TRACKS[track];
  float seconds = def->bars * def->beatsPerBar * 60.0f / def->bpm;
  int frames = (int)(seconds * SAMPLE_RATE + 0.5f);
  size_t count = (size_t)frames * CHANNELS;
  float *mix = calloc(count, sizeof(float));
  if (mix == NULL) {
    loopFrames[track] = -1;
    return;
  }

  MusicBuild build = {.buf = mix, .frames = frames, .track = def};
  def->compose(&build);
  applySpace(mix, frames, def);
  masterTrack(mix, frames, def);

  int16_t *out = malloc(count * sizeof(int16_t));
  if (out == NULL) {
    free(mix);
    loopFrames[track] = -1;
    return;
  }
  for (size_t i = 0; i < count; i++) {
    float dither =
        (noiseAt(0xD17E4u, (unsigned)i) - noiseAt(0xA11CEu, (unsigned)i)) /
        65536.0f;
    float sample = clampf(mix[i] + dither, -1.0f, 1.0f);
    out[i] = (int16_t)lrintf(sample * 32767.0f);
  }
  free(mix);
  loopBuffer[track] = out;
  loopFrames[track] = frames;
}

static bool validTrack(int track) { return track >= 0 && track < TRACK_COUNT; }
static bool gameplayTrack(int track) {
  return track >= 0 && track < TRACK_GAMEPLAY_COUNT;
}
static bool menuPair(int a, int b) {
  return (a == TRACK_MENU || a == TRACK_SHOP) &&
         (b == TRACK_MENU || b == TRACK_SHOP);
}

static int curTrack = TRACK_NONE;
static int prevTrack = TRACK_NONE;
static int pendingTrack = TRACK_NONE;
static int pendingHandoff = -1;
static int pendingFrames;
static int pendingFadeFrames;
static bool pendingCueStarted;
static float crossfade = 1.0f;
static int crossfadeFrames = 1;
static float pauseTarget;
static float pauseMix;
static float musicVol = 1.0f;
static float sfxVol = 1.0f;

static int framesForBeats(int track, float beats) {
  return (int)(beats * 60.0f / TRACKS[track].bpm * SAMPLE_RATE + 0.5f);
}

static int barBoundaryFrame(int track, int bar) {
  int bars = TRACKS[track].bars;
  return (int)(((int64_t)loopFrames[track] * bar + bars / 2) / bars);
}

// Return the first outgoing bar with enough room for the authored lead-out.
// Deriving boundaries from the complete loop length avoids accumulated beat
// rounding error at the loop seam.
static int framesToNextBar(int track, int minLead) {
  int bar = 1;
  while (barBoundaryFrame(track, bar) <= loopPos[track]) {
    bar++;
  }
  int wait = barBoundaryFrame(track, bar) - loopPos[track];
  if (wait >= minLead) {
    return wait;
  }
  bar++;
  if (bar <= TRACKS[track].bars) {
    return barBoundaryFrame(track, bar) - loopPos[track];
  }
  return loopFrames[track] - loopPos[track] + barBoundaryFrame(track, 1);
}

static void clearPendingTrack(void) {
  pendingTrack = TRACK_NONE;
  pendingHandoff = -1;
  pendingFrames = 0;
  pendingFadeFrames = 0;
  pendingCueStarted = false;
}

static void switchTrackNow(int track) {
  int outgoing = curTrack;
  prevTrack = outgoing;
  curTrack = track;
  crossfade = 0.0f;

  if (validTrack(track)) {
    if (menuPair(outgoing, track) && loopFrames[outgoing] > 0 &&
        loopFrames[track] > 0) {
      loopPos[track] = loopPos[outgoing] % loopFrames[track];
    } else {
      loopPos[track] = 0;
    }
    float seconds =
        menuPair(outgoing, track) ? 2.0f * 60.0f / TRACKS[track].bpm : 0.65f;
    crossfadeFrames = (int)(seconds * SAMPLE_RATE + 0.5f);
  } else {
    crossfadeFrames = (int)(0.35f * SAMPLE_RATE);
  }
  if (crossfadeFrames < 1) {
    crossfadeFrames = 1;
  }
}

void setTrack(int track) {
  if (track != TRACK_NONE && !validTrack(track)) {
    return;
  }
  if (track == pendingTrack) {
    return; // frame-by-frame routing repeats the same request
  }
  if (track == curTrack) {
    // A reversal can cancel a silent request, but once its bridge is audible we
    // finish that bar. A repeated later request can schedule the next handoff.
    if (!pendingCueStarted) {
      clearPendingTrack();
    }
    return;
  }

  if (gameplayTrack(curTrack) && gameplayTrack(track) &&
      loopFrames[curTrack] > 0) {
    // The first request owns this bar. Frame-by-frame routing repeats a rapid
    // later destination after the current handoff, preserving musical order.
    if (pendingTrack != TRACK_NONE) {
      return;
    }
    pendingTrack = track;
    pendingHandoff = handoffFor(curTrack, track);
    float outBeats =
        pendingHandoff >= 0 ? HANDOFFS[pendingHandoff].outBeats : 1.0f;
    pendingFadeFrames = framesForBeats(curTrack, outBeats);
    pendingFrames = framesToNextBar(curTrack, pendingFadeFrames);
    pendingCueStarted = false;
    return;
  }

  // Durable non-gameplay contexts remain immediate and cancel any request.
  clearPendingTrack();
  switchTrackNow(track);
}

void setMusicPaused(bool paused) { pauseTarget = paused ? 1.0f : 0.0f; }

void setVolumes(float music, float sfxVolume) {
  musicVol = clampf(music, 0.0f, 1.0f);
  sfxVol = clampf(sfxVolume, 0.0f, 1.0f);
}

static Stereo trackSample(int track) {
  if (!validTrack(track) || loopBuffer[track] == NULL ||
      loopFrames[track] <= 0) {
    return (Stereo){0.0f, 0.0f};
  }
  int at = loopPos[track] * 2;
  return (Stereo){loopBuffer[track][at] / 32768.0f,
                  loopBuffer[track][at + 1] / 32768.0f};
}

static void advanceTrack(int track) {
  if (validTrack(track) && loopFrames[track] > 0) {
    loopPos[track] = (loopPos[track] + 1) % loopFrames[track];
  }
}

typedef struct {
  float seconds;
  float gain;
  float duck;
  unsigned char priority;
} CueDef;

static const CueDef CUES[] = {
    {0.16f, 0.32f, 0.04f, 2}, // COIN
    {0.22f, 0.38f, 0.08f, 3}, // BOUNCE
    {0.48f, 0.42f, 0.2f, 5},  // CRUMBLE
    {0.48f, 0.32f, 0.12f, 4}, // MAGNET
    {0.58f, 0.34f, 0.16f, 5}, // ROCKET
    {0.52f, 0.3f, 0.1f, 4},   // BALLOON
    {0.62f, 0.34f, 0.14f, 5}, // SHIELD
    {0.42f, 0.34f, 0.12f, 5}, // PELLET
    {0.48f, 0.38f, 0.2f, 6},  // DEBUFF
    {0.5f, 0.46f, 0.42f, 8},  // HIT
    {0.3f, 0.4f, 0.2f, 6},    // SPLAT
    {0.09f, 0.24f, 0.0f, 2},  // SELECT
    {0.72f, 0.34f, 0.12f, 5}, // PURCHASE
    {0.32f, 0.38f, 0.16f, 6}, // DENIED
    {0.95f, 0.34f, 0.18f, 7}, // HANDOFF
    {0.9f, 0.38f, 0.35f, 8},  // GAMEOVER
    {1.35f, 0.38f, 0.3f, 9},  // NEWBEST
    {0.5f, 0.23f, 0.14f, 7},  // STATIC
    {1.0f, 0.3f, 0.0f, 9},    // LOGO
};
STATIC_ASSERT(ARRAY_LEN(CUES) == SFX_COUNT, cue_defs_full);

typedef struct {
  bool active;
  SfxId type;
  int pos;
  int length;
  float pitch;
  float left;
  float right;
  float gain;
  SoundStyle style;
  int rootMidi;
  unsigned seed;
  bool musicBus;
  int handoff;
  int handoffSeam;
} Voice;

static Voice voices[MAX_VOICES];
static unsigned voiceSerial;
static bool logoPlayed;

static SoundStyle currentStyle(void) {
  return validTrack(curTrack) ? TRACKS[curTrack].style : STYLE_UI;
}

static int currentRoot(void) {
  return validTrack(curTrack) ? TRACKS[curTrack].rootMidi : 67;
}

static float stylePitch(SoundStyle style) { return STYLE_PITCH[style]; }

static int findVoice(unsigned priority) {
  int candidate = -1;
  float oldest = -1.0f;
  for (int i = 0; i < MAX_VOICES; i++) {
    if (!voices[i].active) {
      return i;
    }
    unsigned livePriority = CUES[voices[i].type].priority;
    float age = (float)voices[i].pos / voices[i].length;
    if (livePriority <= priority && age > oldest) {
      candidate = i;
      oldest = age;
    }
  }
  return candidate;
}

static int triggerCue(SfxId id, float pitch, float pan, bool musicBus,
                      SoundStyle style, int rootMidi) {
  if (id < 0 || id >= SFX_COUNT || pitch <= 0.0f || !isfinite(pitch) ||
      !isfinite(pan)) {
    return -1;
  }
  int slot = findVoice(CUES[id].priority);
  if (slot < 0) {
    return -1;
  }
  unsigned seed = hash32(++voiceSerial * 0x9e3779b9u + (unsigned)id * 101u);
  float randomPitch = id == SFX_COIN ? 0.004f : 0.012f;
  pitch *= 1.0f + noiseAt(seed, 1u) * randomPitch;
  pan =
      clampf(pan + noiseAt(seed, 2u) * (musicBus ? 0.0f : 0.035f), -1.0f, 1.0f);
  float angle = (pan + 1.0f) * PI * 0.25f;
  voices[slot] = (Voice){
      .active = true,
      .type = id,
      .pos = 0,
      .length = (int)(CUES[id].seconds * SAMPLE_RATE + 0.5f),
      .pitch = pitch,
      .left = cosf(angle),
      .right = sinf(angle),
      .gain = CUES[id].gain * (0.96f + 0.08f * noiseAt(seed, 3u)),
      .style = style,
      .rootMidi = rootMidi,
      .seed = seed,
      .musicBus = musicBus,
      .handoff = -1,
      .handoffSeam = 0,
  };
  return slot;
}

void sfxAt(SfxId id, float pitch, float pan) {
  triggerCue(id, pitch, pan, false, currentStyle(), currentRoot());
}

void sfxPitched(SfxId id, float pitch) { sfxAt(id, pitch, 0.0f); }
void sfx(SfxId id) { sfxAt(id, 1.0f, 0.0f); }

void musicLogoSting(void) {
  if (!logoPlayed) {
    logoPlayed = true;
    triggerCue(SFX_LOGO, 1.0f, 0.0f, true, STYLE_UI,
               TRACKS[TRACK_MENU].rootMidi);
  }
}

static void musicEraTransition(int handoff, int seamFrames) {
  if (handoff < 0 || handoff >= ARRAY_LEN(HANDOFFS)) {
    return;
  }
  const Handoff *def = &HANDOFFS[handoff];
  int track = def->to;
  int slot = triggerCue(SFX_HANDOFF, 1.0f, 0.0f, true, TRACKS[track].style,
                        TRACKS[track].rootMidi);
  if (slot >= 0) {
    voices[slot].pitch = 1.0f; // soundtrack bridges stay exactly in tune
    voices[slot].handoff = handoff;
    voices[slot].handoffSeam = seamFrames;
    voices[slot].length = seamFrames + framesForBeats(track, def->inBeats);
  }
}

static float cueEnv(const Voice *v, float attackSeconds, float curve) {
  float attack = attackSeconds * SAMPLE_RATE;
  float in = v->pos < attack ? v->pos / attack : 1.0f;
  float out = 1.0f - (float)v->pos / v->length;
  return in * powf(fmaxf(out, 0.0f), curve);
}

static float steppedTone(const Voice *v, const int *semitones, int count,
                         float x, int octave) {
  int note = (int)(x * count);
  if (note >= count) {
    note = count - 1;
  }
  int start = v->length * note / count;
  int end = v->length * (note + 1) / count;
  int local = v->pos - start;
  int attack = (int)(0.003f * SAMPLE_RATE);
  int release = (int)(0.008f * SAMPLE_RATE);
  float noteEnv = fminf((float)local / attack, 1.0f) *
                  fminf((float)(end - v->pos) / release, 1.0f);
  float hz = midiHz(v->rootMidi + octave + semitones[note]) * v->pitch *
             stylePitch(v->style);
  float t = (float)local / SAMPLE_RATE;
  float cycles = vibratoCycles(hz, t, 5.0f, 0.0015f);
  return noteEnv * styleWave(v->style, cycles, hz, local, v->seed);
}

static float chirpWave(const Voice *v, float startHz, float endHz) {
  float t = (float)v->pos / SAMPLE_RATE;
  float duration = (float)v->length / SAMPLE_RATE;
  startHz *= v->pitch * stylePitch(v->style);
  endHz *= v->pitch * stylePitch(v->style);
  float cycles = sweepCycles(startHz, endHz, t, duration);
  float hz = startHz + (endHz - startHz) * t / duration;
  return styleWave(v->style, cycles, fabsf(hz), v->pos, v->seed);
}

static float renderHandoffCue(const Voice *v, unsigned seedOffset) {
  if (v->handoff < 0 || v->handoff >= ARRAY_LEN(HANDOFFS)) {
    return 0.0f;
  }
  const Handoff *handoff = &HANDOFFS[v->handoff];
  const Track *from = &TRACKS[handoff->from], *to = &TRACKS[handoff->to];
  float x = (float)v->pos / v->length;
  float blendStart = v->handoffSeam * 0.5f;
  float blendEnd = v->handoffSeam + (v->length - v->handoffSeam) * 0.5f;
  float blend =
      clampf((v->pos - blendStart) / (blendEnd - blendStart), 0.0f, 1.0f);
  blend = blend * blend * (3.0f - 2.0f * blend);
  float t = (float)v->pos / SAMPLE_RATE;
  float fromHz = midiHz(from->rootMidi + 12);
  float toHz = midiHz(to->rootMidi + 12);
  float source =
      styleWave(from->style, fromHz * t, fromHz, v->pos, v->seed + seedOffset);
  float destination =
      styleWave(to->style, toHz * t, toHz, v->pos, v->seed + seedOffset + 17u);
  bool reboot = handoff->color == HANDOFF_REBOOT;
  float pivot = source + (destination - source) * blend;
  if (reboot) {
    // A is common to E harmonic minor and D major. Hold it in a plain voice
    // while the busy digital arrangement falls away and the new mode settles.
    float common = midiHz(69);
    pivot =
        0.8f * sinf(TAU * common * t) + 0.2f * sinf(TAU * common * 2.0f * t);
  }

  // Revoice the opening rise of the shared Boing motif: the source plays the
  // first half and the destination answers, avoiding an early harmonic clash.
  const int noteCount = 5;
  int note = v->pos * noteCount / v->length;
  if (note >= noteCount) {
    note = noteCount - 1;
  }
  int start = v->length * note / noteCount;
  int end = v->length * (note + 1) / noteCount;
  int local = v->pos - start;
  int attack = (int)(0.004f * SAMPLE_RATE);
  int release = (int)(0.012f * SAMPLE_RATE);
  float noteEnv = fminf((float)local / attack, 1.0f) *
                  fminf((float)(end - v->pos) / release, 1.0f);
  int motifTrack = start < v->handoffSeam ? handoff->from : handoff->to;
  const Track *motifVoice = &TRACKS[motifTrack];
  int degree = MOTIF[note].degree;
  int midi = degreeMidi(GAMEPLAY_SCALES[motifTrack], motifVoice->rootMidi + 12,
                        degree);
  float hz = midiHz(midi);
  float motifWave = styleWave(motifVoice->style, hz * local / SAMPLE_RATE, hz,
                              local, v->seed + seedOffset + 31u);
  if (reboot) {
    float localT = (float)local / SAMPLE_RATE;
    float gate = (float)(end - start) / SAMPLE_RATE * 0.72f;
    float digital = styleWave(STYLE_BLOCKS, hz * localT, hz, local,
                              v->seed + seedOffset + 31u);
    float mallet = instrumentSample(INST_MALLET, hz, localT, gate, local,
                                    v->seed + seedOffset + 47u);
    motifWave = digital + (mallet - digital) * blend;
  }
  float motif = noteEnv * motifWave;

  float arch = sinf(PI * x);
  float color = 0.0f;
  switch (handoff->color) {
  case HANDOFF_AIR:
    color = 0.13f * smoothNoise(v->seed + seedOffset, (unsigned)v->pos, 53u);
    break;
  case HANDOFF_TWILIGHT:
    color = 0.12f * sinf(TAU * midiHz(to->rootMidi + 19) * t);
    break;
  case HANDOFF_SIGNAL: {
    float duration = (float)v->length / SAMPLE_RATE;
    float pulse = 3.0f * t + 4.0f * t * t / duration;
    color = 0.12f * destination * (0.5f + 0.5f * sinf(TAU * pulse));
    break;
  }
  case HANDOFF_FRAGMENT: {
    int tick = (int)(0.12f * SAMPLE_RATE);
    int at = v->pos % tick;
    float burst = expf(-(float)at / (0.025f * SAMPLE_RATE));
    float noise =
        noiseAt(v->seed + seedOffset, (unsigned)v->pos) -
        noiseAt(v->seed + seedOffset, (unsigned)(v->pos > 0 ? v->pos - 1 : 0));
    color = 0.1f * burst * noise;
    break;
  }
  case HANDOFF_REBOOT: {
    float digital = floorf(source * 8.0f + 0.5f) / 8.0f;
    float soft = 0.72f * sinf(TAU * toHz * t) + 0.28f * triangleWave(toHz * t);
    color = 0.1f * (digital + (soft - digital) * blend);
    break;
  }
  case HANDOFF_COUNT:
    break;
  }

  return cueEnv(v, 0.008f, 1.15f) *
         (0.34f * pivot + 0.58f * motif + arch * color);
}

static float renderCueMono(const Voice *v, unsigned seedOffset) {
  float x = (float)v->pos / v->length;
  float t = (float)v->pos / SAMPLE_RATE;
  float env = cueEnv(v, 0.003f, 1.8f);
  float n = noiseAt(v->seed + seedOffset, (unsigned)v->pos);
  float smooth = smoothNoise(v->seed + seedOffset, (unsigned)v->pos, 43u);

  switch (v->type) {
  case SFX_COIN: {
    static const int NOTES[] = {0, 4, 9};
    return cueEnv(v, 0.002f, 2.4f) *
           (0.82f * steppedTone(v, NOTES, ARRAY_LEN(NOTES), x, 24) +
            0.18f * sinf(TAU * midiHz(v->rootMidi + 36) * v->pitch * t));
  }
  case SFX_BOUNCE:
    return env * (0.82f * chirpWave(v, 360.0f, 92.0f) +
                  0.18f * smooth * expf(-x * 8.0f));
  case SFX_CRUMBLE: {
    float chunks = sinf(
        TAU * sweepCycles(145.0f, 48.0f, t, (float)v->length / SAMPLE_RATE));
    float crack = (n - noiseAt(v->seed + seedOffset,
                               (unsigned)(v->pos > 0 ? v->pos - 1 : 0)));
    float pulse = 0.55f + 0.45f * fabsf(sinf(TAU * 13.0f * t));
    return env * (0.48f * chunks + 0.52f * crack * pulse);
  }
  case SFX_MAGNET: {
    static const int NOTES[] = {0, 7, 12, 16};
    float hum = sinf(TAU * (90.0f * t + 5.0f * sinf(TAU * 7.0f * t)));
    return env *
           (0.8f * steppedTone(v, NOTES, ARRAY_LEN(NOTES), x, 12) + 0.2f * hum);
  }
  case SFX_ROCKET: {
    float rise = chirpWave(v, 110.0f, 760.0f);
    float whoosh = 0.55f * n + 0.45f * smooth;
    return cueEnv(v, 0.006f, 1.25f) *
           (0.42f * rise + 0.58f * whoosh * (0.4f + 0.6f * x));
  }
  case SFX_BALLOON: {
    float wobble = 1.0f + 0.035f * sinf(TAU * 6.0f * t);
    float hz = (310.0f + 390.0f * x) * v->pitch * stylePitch(v->style) * wobble;
    float cycles =
        sweepCycles(310.0f, 700.0f, t, (float)v->length / SAMPLE_RATE) *
        v->pitch * stylePitch(v->style);
    return cueEnv(v, 0.025f, 1.5f) *
           (0.85f * styleWave(v->style, cycles, hz, v->pos, v->seed) +
            0.15f * smooth);
  }
  case SFX_SHIELD: {
    float root = midiHz(v->rootMidi + 12) * v->pitch * stylePitch(v->style);
    float shimmer = sinf(TAU * root * t) + 0.7f * sinf(TAU * root * 1.5f * t) +
                    0.5f * sinf(TAU * root * 2.0f * t);
    return cueEnv(v, 0.008f, 1.35f) * shimmer * 0.48f;
  }
  case SFX_PELLET: {
    float pulse = chirpWave(v, 95.0f, 280.0f);
    float ring = sinf(TAU * midiHz(v->rootMidi + 24) * v->pitch * t);
    return env * (0.72f * pulse + 0.28f * ring);
  }
  case SFX_DEBUFF: {
    static const int NOTES[] = {6, 3, 0, -6};
    return env *
           (0.72f * steppedTone(v, NOTES, ARRAY_LEN(NOTES), x, 12) + 0.28f * n);
  }
  case SFX_HIT: {
    float body = chirpWave(v, 210.0f, 46.0f);
    float crack = n - noiseAt(v->seed + seedOffset,
                              (unsigned)(v->pos > 0 ? v->pos - 1 : 0));
    return cueEnv(v, 0.0015f, 2.0f) * (0.58f * body + 0.42f * crack);
  }
  case SFX_SPLAT: {
    float body = chirpWave(v, 175.0f, 55.0f);
    float wet = smoothNoise(v->seed + seedOffset, (unsigned)v->pos, 19u);
    return env * (0.58f * body + 0.42f * wet);
  }
  case SFX_SELECT: {
    static const int NOTES[] = {0, 7};
    return cueEnv(v, 0.0015f, 2.8f) *
           steppedTone(v, NOTES, ARRAY_LEN(NOTES), x, 24);
  }
  case SFX_PURCHASE: {
    static const int NOTES[] = {0, 2, 4, 7, 9};
    return cueEnv(v, 0.002f, 1.35f) *
           steppedTone(v, NOTES, ARRAY_LEN(NOTES), x, 24);
  }
  case SFX_DENIED: {
    static const int NOTES[] = {0, -6};
    return env * steppedTone(v, NOTES, ARRAY_LEN(NOTES), x, 0);
  }
  case SFX_HANDOFF: {
    if (v->handoff >= 0) {
      return renderHandoffCue(v, seedOffset);
    }
    static const int NOTES[] = {0, 4, 7, 12, 16};
    return cueEnv(v, 0.004f, 1.2f) *
           steppedTone(v, NOTES, ARRAY_LEN(NOTES), x, 12);
  }
  case SFX_GAMEOVER: {
    static const int NOTES[] = {7, 3, 0, -5};
    return cueEnv(v, 0.006f, 1.15f) *
           steppedTone(v, NOTES, ARRAY_LEN(NOTES), x, 12);
  }
  case SFX_NEWBEST: {
    static const int NOTES[] = {0, 4, 7, 11, 12, 16, 19};
    float lead = steppedTone(v, NOTES, ARRAY_LEN(NOTES), x, 12);
    float fifth = steppedTone(v, NOTES, ARRAY_LEN(NOTES), x, 19);
    return cueEnv(v, 0.004f, 1.05f) * (0.68f * lead + 0.32f * fifth);
  }
  case SFX_STATIC: {
    float fade = sinf(PI * clampf(x, 0.0f, 1.0f));
    float hum = sinf(TAU * 60.0f * t) + 0.45f * sinf(TAU * 120.0f * t);
    return fade * (0.78f * n + 0.14f * smooth + 0.08f * hum);
  }
  case SFX_LOGO: {
    static const int NOTES[] = {0, 4, 7, 14};
    float lead = steppedTone(v, NOTES, ARRAY_LEN(NOTES), x, 12);
    float low = sinf(TAU * midiHz(v->rootMidi - 12) * t);
    return cueEnv(v, 0.012f, 1.1f) * (0.78f * lead + 0.22f * low);
  }
  case SFX_COUNT:
    break;
  }
  return 0.0f;
}

static Stereo renderVoice(const Voice *v) {
  float mono = v->gain * renderCueMono(v, 0u);
  float right =
      v->type == SFX_STATIC ? v->gain * renderCueMono(v, 0x51A71Cu) : mono;
  return (Stereo){mono * v->left, right * v->right};
}

static float voiceActivity(const Voice *v) {
  float x = 1.0f - (float)v->pos / v->length;
  return fmaxf(x, 0.0f);
}

#define HIT_HOLD_FRAMES (SAMPLE_RATE * 2 / 5)
#define HIT_RECOVER_FRAMES (SAMPLE_RATE * 4 / 5)
static int hitFrames;
static int musicCombo;
static float dangerTarget, dangerMix;
static float finaleTarget, finaleMix;
static Stereo simpleMusic;
static Stereo pauseFilter;
static float duckMix;
static unsigned dynamicsPos;

void setMusicDynamics(int combo, float danger, float finale) {
  musicCombo = combo > 0 ? combo : 0;
  dangerTarget = clampf(danger, 0.0f, 1.0f);
  finaleTarget = clampf(finale, 0.0f, 1.0f);
}

void musicMonsterHit(void) { hitFrames = HIT_HOLD_FRAMES + HIT_RECOVER_FRAMES; }

static float smoothToward(float current, float target, float rate) {
  return current + (target - current) * rate;
}

static float hitRestore(void) {
  if (hitFrames <= 0) {
    return 1.0f;
  }
  int left = hitFrames--;
  if (left > HIT_RECOVER_FRAMES) {
    return 0.0f;
  }
  return 1.0f - (float)left / HIT_RECOVER_FRAMES;
}

static float liveHitEnv(int phase, int length) {
  if (phase < 0 || phase >= length) {
    return 0.0f;
  }
  float attack = phase < 24 ? (float)phase / 24.0f : 1.0f;
  float out = 1.0f - (float)phase / length;
  return attack * out * out;
}

static float comboStage(int combo, int startsAt) {
  return clampf((combo - startsAt) / 5.0f, 0.0f, 1.0f);
}

static Stereo renderComboLayer(unsigned pos, int combo) {
  if (combo <= 2 || !validTrack(curTrack)) {
    return (Stereo){0.0f, 0.0f};
  }
  const Track *track = &TRACKS[curTrack];
  int beat = trackBeatFrames(track);
  int half = beat / 2;
  int hatPhase = (int)(pos % (unsigned)half);
  int hatLen = (int)(0.055f * SAMPLE_RATE);
  float hatEnv = liveHitEnv(hatPhase, hatLen);
  float early = comboStage(combo, 2);
  unsigned step = pos / (unsigned)half;
  float panL = (step & 1u) ? 0.48f : 0.88f;
  float panR = (step & 1u) ? 0.88f : 0.48f;

  if (track->style == STYLE_SKIES) {
    static const int NOTES[] = {0, 4, 7, 11, 7};
    float t = (float)hatPhase / SAMPLE_RATE;
    float chime =
        hatEnv *
        sinf(TAU *
             midiHz(track->rootMidi + 24 + NOTES[step % ARRAY_LEN(NOTES)]) * t);
    return (Stereo){0.075f * early * chime * panL,
                    0.075f * early * chime * panR};
  }

  float raw = noiseAt(0xC0B0u, pos);
  float hp = raw - noiseAt(0xC0B0u, pos > 0 ? pos - 1u : 0u);
  float hat = raw;
  float hatGain = 0.06f;
  switch (track->style) {
  case STYLE_SUNNY:
    hat = 0.6f * hp + 0.4f * smoothNoise(0x51A4Eu, pos, 13u);
    hatGain = 0.052f;
    break;
  case STYLE_NIGHT: {
    static const int NOTES[] = {0, 4, 2, 6};
    float hz = midiHz(track->rootMidi + 24 + NOTES[step % ARRAY_LEN(NOTES)]);
    hat = styleWave(STYLE_NIGHT, hz * hatPhase / SAMPLE_RATE, hz, hatPhase,
                    0xA1677u);
    hatGain = 0.036f;
    break;
  }
  case STYLE_MAZE:
    hat = hp;
    hatGain = 0.058f;
    break;
  case STYLE_BLOCKS:
    hat = floorf(hp * 8.0f + 0.5f) / 8.0f;
    hatGain = 0.05f;
    break;
  default:
    break;
  }
  hat *= hatEnv;
  Stereo out = {hatGain * early * hat * panL, hatGain * early * hat * panR};

  int kickPhase = (int)(pos % (unsigned)beat);
  int kickLen = (int)(0.16f * SAMPLE_RATE);
  float kickEnv = liveHitEnv(kickPhase, kickLen);
  if (kickEnv > 0.0f) {
    float start = 145.0f, end = 52.0f, gain = 0.095f;
    if (track->style == STYLE_NIGHT) {
      start = 112.0f;
      end = 44.0f;
      gain = 0.058f;
    } else if (track->style == STYLE_MAZE) {
      start = 158.0f;
      end = 55.0f;
      gain = 0.1f;
    } else if (track->style == STYLE_BLOCKS) {
      start = 176.0f;
      end = 58.0f;
      gain = 0.108f;
    }
    float t = (float)kickPhase / SAMPLE_RATE;
    float phase = sweepCycles(start, end, t, (float)kickLen / SAMPLE_RATE);
    float kick = kickEnv * sinf(TAU * phase) * gain * comboStage(combo, 6);
    out.l += kick;
    out.r += kick;
  }

  int snarePhase = (int)(pos % (unsigned)(beat * 2)) - beat;
  int snareLen = (int)(0.12f * SAMPLE_RATE);
  float snareEnv = liveHitEnv(snarePhase, snareLen);
  if (snareEnv > 0.0f) {
    float texture = noiseAt(0x5A4E3u, pos);
    float gain = 0.065f;
    if (track->style == STYLE_NIGHT) {
      texture = 0.55f * texture +
                0.45f * sinf(TAU * 175.0f * snarePhase / SAMPLE_RATE);
      gain = 0.038f;
    } else if (track->style == STYLE_MAZE) {
      texture -= noiseAt(0x5A4E3u, pos > 0 ? pos - 1u : 0u);
    } else if (track->style == STYLE_BLOCKS) {
      texture = floorf(texture * 10.0f + 0.5f) / 10.0f;
      gain = 0.072f;
    }
    float snare = gain * comboStage(combo, 10) * snareEnv * texture;
    out.l += snare * 0.8f;
    out.r += snare;
  }
  return out;
}

static Stereo renderDanger(unsigned pos, float amount) {
  if (amount <= 0.0f || !validTrack(curTrack)) {
    return (Stereo){0.0f, 0.0f};
  }
  const Track *track = &TRACKS[curTrack];
  int beat = trackBeatFrames(track);
  int period = beat * 2;
  int phase = (int)(pos % (unsigned)period);
  int second = phase - (int)(0.28f * SAMPLE_RATE);
  int len = (int)(0.17f * SAMPLE_RATE);
  float e1 = liveHitEnv(phase, len);
  float e2 = liveHitEnv(second, len) * 0.72f;
  float t1 = (float)phase / SAMPLE_RATE;
  float t2 = (float)(second > 0 ? second : 0) / SAMPLE_RATE;
  float root = midiHz(track->rootMidi - 36);
  float value =
      0.12f * amount *
      (e1 * sinf(TAU * root * t1) + e2 * sinf(TAU * root * 1.5f * t2));
  return (Stereo){value, value};
}

static Stereo renderFinale(unsigned pos, float amount) {
  if (amount <= 0.0f || curTrack != TRACK_BLOCKS) {
    return (Stereo){0.0f, 0.0f};
  }
  int beat = (int)(SAMPLE_RATE * 60.0f / TRACKS[TRACK_BLOCKS].bpm + 0.5f);
  int half = beat / 2;
  int phase = (int)(pos % (unsigned)half);
  int len = (int)(0.32f * SAMPLE_RATE);
  float env = liveHitEnv(phase, len);
  static const int NOTES[] = {0, 3, 7, 11, 12, 8, 7, 3};
  int note = (int)(pos / (unsigned)half) % ARRAY_LEN(NOTES);
  float hz = midiHz(TRACKS[TRACK_BLOCKS].rootMidi + 12 + NOTES[note]);
  float t = (float)phase / SAMPLE_RATE;
  float value = 0.085f * amount * env *
                styleWave(STYLE_BLOCKS, hz * t, hz, phase, 0xF1A1Eu);
  float pan = note & 1 ? 0.68f : 0.32f;
  return (Stereo){value * pan, value * (1.0f - pan)};
}

void initAudio(AudioStream *stream) {
  *stream = (AudioStream){0};
  InitAudioDevice();
  if (!IsAudioDeviceReady()) {
    return;
  }
  SetAudioStreamBufferSizeDefault(BUFFER_FRAMES);
  *stream = LoadAudioStream(SAMPLE_RATE, 32, CHANNELS);
  if (IsAudioStreamValid(*stream)) {
    PlayAudioStream(*stream);
  }
}

static void *preloadWorker(void *unused) {
  (void)unused;
  for (int i = 0; i < TRACK_COUNT; i++) {
    buildLoop(PRELOAD_ORDER[i]);
  }
  pthread_mutex_lock(&preloadLock);
  preloadFinished = true;
  pthread_mutex_unlock(&preloadLock);
  return NULL;
}

bool audioPreloadStep(void) {
  if (!preloadStarted) {
    preloadStarted = true;
    if (pthread_create(&preloadThread, NULL, preloadWorker, NULL) != 0) {
      // Thread creation is a rare platform failure. Finish synchronously so a
      // missing worker cannot leave the splash waiting forever.
      preloadWorker(NULL);
      preloadJoined = true;
      return true;
    }
  }

  pthread_mutex_lock(&preloadLock);
  bool finished = preloadFinished;
  pthread_mutex_unlock(&preloadLock);
  if (finished && !preloadJoined) {
    pthread_join(preloadThread, NULL);
    preloadJoined = true;
  }
  return finished;
}

static void beginPendingTrack(void) {
  int track = pendingTrack;
  int handoff = pendingHandoff;
  prevTrack = TRACK_NONE; // the outgoing full mix ended before this downbeat
  curTrack = track;
  loopPos[track] = 0;
  crossfade = 0.0f;
  float inBeats = handoff >= 0 ? HANDOFFS[handoff].inBeats : 1.0f;
  crossfadeFrames = framesForBeats(track, inBeats);
  if (crossfadeFrames < 1) {
    crossfadeFrames = 1;
  }
  clearPendingTrack();
}

static float eased01(float x) {
  x = clampf(x, 0.0f, 1.0f);
  return x * x * (3.0f - 2.0f * x);
}

static void handoffMixShape(int handoff, float progress, float *filter,
                            float *fade) {
  *filter = *fade = eased01(progress);
  if (handoff >= 0 && handoff < ARRAY_LEN(HANDOFFS) &&
      HANDOFFS[handoff].color == HANDOFF_REBOOT) {
    // Two source beats: remove hats/snare into the low-passed center first,
    // then take out the remaining kick/bass under the neutral motif bridge.
    *filter = eased01(progress * 2.0f);
    *fade = eased01(progress * 2.0f - 1.0f);
  }
}

static void pendingMix(float *filter, float *fade) {
  *filter = *fade = 0.0f;
  if (pendingTrack == TRACK_NONE) {
    return;
  }
  if (!pendingCueStarted && pendingFrames <= pendingFadeFrames) {
    musicEraTransition(pendingHandoff, pendingFadeFrames);
    pendingCueStarted = true;
  }
  if (pendingFrames < pendingFadeFrames) {
    float progress = 1.0f - (float)pendingFrames / pendingFadeFrames;
    handoffMixShape(pendingHandoff, progress, filter, fade);
  }
}

static Stereo renderAudioFrame(void) {
  if (pendingTrack != TRACK_NONE && pendingFrames <= 0) {
    beginPendingTrack();
  }
  float filter, fade;
  pendingMix(&filter, &fade);
  Stereo incoming = trackSample(curTrack);
  Stereo outgoing = trackSample(prevTrack);
  float inGain = sinf(crossfade * PI * 0.5f);
  float outGain = cosf(crossfade * PI * 0.5f);
  Stereo music = {incoming.l * inGain + outgoing.l * outGain,
                  incoming.r * inGain + outgoing.r * outGain};
  unsigned layerPos = validTrack(curTrack) && loopFrames[curTrack] > 0
                          ? (unsigned)loopPos[curTrack]
                          : dynamicsPos;
  advanceTrack(curTrack);
  if (prevTrack != curTrack) {
    advanceTrack(prevTrack);
  }
  if (crossfade < 1.0f) {
    crossfade += 1.0f / crossfadeFrames;
    if (crossfade >= 1.0f) {
      crossfade = 1.0f;
      prevTrack = curTrack;
    }
  }

  dangerMix = smoothToward(dangerMix, dangerTarget, 0.0007f);
  finaleMix = smoothToward(finaleMix, finaleTarget, 0.0007f);
  pauseMix = smoothToward(pauseMix, pauseTarget, 0.0009f);
  Stereo combo = renderComboLayer(layerPos, musicCombo);
  Stereo danger = renderDanger(layerPos, dangerMix);
  Stereo finale = renderFinale(layerPos, finaleMix);
  music.l += combo.l + danger.l + finale.l;
  music.r += combo.r + danger.r + finale.r;

  simpleMusic.l += 0.025f * (music.l - simpleMusic.l);
  simpleMusic.r += 0.025f * (music.r - simpleMusic.r);
  float restore = hitRestore();
  music.l = 0.65f * simpleMusic.l + (music.l - 0.65f * simpleMusic.l) * restore;
  music.r = 0.65f * simpleMusic.r + (music.r - 0.65f * simpleMusic.r) * restore;

  pauseFilter.l += 0.075f * (music.l - pauseFilter.l);
  pauseFilter.r += 0.075f * (music.r - pauseFilter.r);
  music.l += (pauseFilter.l * 0.52f - music.l) * pauseMix;
  music.r += (pauseFilter.r * 0.52f - music.r) * pauseMix;

  // Remove the outgoing rhythm section before the new downbeat. Its filtered
  // center fades first, while the short motif bridge carries across the seam.
  float stripGain = cosf(fade * PI * 0.5f);
  music.l = (music.l + (simpleMusic.l * 0.55f - music.l) * filter) * stripGain;
  music.r = (music.r + (simpleMusic.r * 0.55f - music.r) * filter) * stripGain;

  Stereo musicCue = {0.0f, 0.0f};
  Stereo effects = {0.0f, 0.0f};
  float duckTarget = 0.0f;
  for (int v = 0; v < MAX_VOICES; v++) {
    if (!voices[v].active) {
      continue;
    }
    Stereo voice = renderVoice(&voices[v]);
    if (voices[v].musicBus) {
      musicCue.l += voice.l;
      musicCue.r += voice.r;
    } else {
      effects.l += voice.l;
      effects.r += voice.r;
      duckTarget = fmaxf(duckTarget,
                         CUES[voices[v].type].duck * voiceActivity(&voices[v]));
    }
    if (++voices[v].pos >= voices[v].length) {
      voices[v].active = false;
    }
  }
  float duckRate = duckTarget > duckMix ? 0.06f : 0.0012f;
  duckMix = smoothToward(duckMix, duckTarget, duckRate);
  float musicGain = musicVol * (1.0f - duckMix);
  float left = (music.l + musicCue.l) * musicGain + effects.l * sfxVol;
  float right = (music.r + musicCue.r) * musicGain + effects.r * sfxVol;
  if (pendingTrack != TRACK_NONE && pendingFrames > 0) {
    pendingFrames--;
  }
  dynamicsPos++;
  return (Stereo){tanhf(left), tanhf(right)};
}

void updateAudio(AudioStream *stream) {
  if (!IsAudioStreamValid(*stream) || !IsAudioStreamProcessed(*stream)) {
    return;
  }

  float output[BUFFER_FRAMES * CHANNELS];
  for (int i = 0; i < BUFFER_FRAMES; i++) {
    Stereo frame = renderAudioFrame();
    output[i * 2] = frame.l;
    output[i * 2 + 1] = frame.r;
  }
  UpdateAudioStream(*stream, output, BUFFER_FRAMES);
}

void deinitAudio(AudioStream *stream) {
  if (preloadStarted && !preloadJoined) {
    pthread_join(preloadThread, NULL);
    preloadJoined = true;
  }
  if (IsAudioStreamValid(*stream)) {
    UnloadAudioStream(*stream);
  }
  if (IsAudioDeviceReady()) {
    CloseAudioDevice();
  }
  *stream = (AudioStream){0};

  for (int i = 0; i < TRACK_COUNT; i++) {
    free(loopBuffer[i]);
    loopBuffer[i] = NULL;
    loopFrames[i] = 0;
    loopPos[i] = 0;
  }
  memset(voices, 0, sizeof(voices));
  preloadStarted = false;
  preloadFinished = false;
  preloadJoined = false;
  logoPlayed = false;
  curTrack = prevTrack = TRACK_NONE;
  clearPendingTrack();
  crossfade = 1.0f;
  crossfadeFrames = 1;
}

#ifdef SOUND_SELFTEST
static void resetPlaybackTest(void) {
  memset(loopPos, 0, sizeof(loopPos));
  memset(voices, 0, sizeof(voices));
  curTrack = prevTrack = TRACK_NONE;
  clearPendingTrack();
  crossfade = 1.0f;
  crossfadeFrames = 1;
  pauseTarget = pauseMix = 0.0f;
  musicVol = sfxVol = 1.0f;
  hitFrames = musicCombo = 0;
  dangerTarget = dangerMix = 0.0f;
  finaleTarget = finaleMix = 0.0f;
  simpleMusic = pauseFilter = (Stereo){0.0f, 0.0f};
  duckMix = 0.0f;
  dynamicsPos = 0;
  voiceSerial = 0;
}

static void checkDefinitions(void) {
  for (int i = 0; i < TRACK_COUNT; i++) {
    const Track *track = &TRACKS[i];
    assert(track->bpm >= 60.0f && track->bpm <= 180.0f);
    assert(track->bars > 0 && track->beatsPerBar >= 3 &&
           track->beatsPerBar <= 4);
    assert(track->rootMidi >= 36 && track->rootMidi <= 84);
    assert(track->style >= 0 && track->style < STYLE_COUNT);
    assert(track->echo >= 0.0f && track->echo <= 0.5f);
    assert(track->room >= 0.0f && track->room <= 0.5f);
    assert(track->targetRms > 0.05f && track->targetRms < 0.2f);
    assert(track->compose != NULL);
  }
  assert(TRACKS[TRACK_MENU].bpm == TRACKS[TRACK_SHOP].bpm);
  assert(TRACKS[TRACK_MENU].bars == TRACKS[TRACK_SHOP].bars);
  for (int i = 0; i < ARRAY_LEN(HANDOFFS); i++) {
    assert(HANDOFFS[i].from == (MusicTrack)i);
    assert(HANDOFFS[i].to == (MusicTrack)((i + 1) % TRACK_GAMEPLAY_COUNT));
    assert(HANDOFFS[i].outBeats > 0.0f &&
           HANDOFFS[i].outBeats <= TRACKS[i].beatsPerBar);
    assert(HANDOFFS[i].inBeats > 0.0f &&
           HANDOFFS[i].inBeats <= TRACKS[HANDOFFS[i].to].beatsPerBar);
    assert(HANDOFFS[i].color == (HandoffColor)i);
    assert(GAMEPLAY_SCALES[i] != NULL);
  }
  bool seen[TRACK_COUNT] = {false};
  for (int i = 0; i < TRACK_COUNT; i++) {
    int track = PRELOAD_ORDER[i];
    assert(validTrack(track) && !seen[track]);
    seen[track] = true;
  }
  for (int i = 0; i < SFX_COUNT; i++) {
    assert(CUES[i].seconds > 0.0f && CUES[i].gain > 0.0f);
    assert(CUES[i].duck >= 0.0f && CUES[i].duck < 1.0f);
    assert(CUES[i].priority > 0);
  }
}

static void checkTracks(void) {
  for (int track = 0; track < TRACK_COUNT; track++) {
    assert(loopBuffer[track] != NULL && loopFrames[track] > 0);
    double sum = 0.0, side = 0.0;
    float peak = 0.0f;
    for (int i = 0; i < loopFrames[track]; i++) {
      float l = loopBuffer[track][i * 2] / 32768.0f;
      float r = loopBuffer[track][i * 2 + 1] / 32768.0f;
      sum += ((double)l * l + (double)r * r) * 0.5;
      side += fabs(l - r);
      peak = fmaxf(peak, fmaxf(fabsf(l), fabsf(r)));
    }
    float rms = sqrtf((float)(sum / loopFrames[track]));
    assert(rms > 0.07f && rms < 0.16f);
    assert(peak > 0.3f && peak <= 0.645f);
    if (TRACKS[track].style == STYLE_MAZE ||
        TRACKS[track].style == STYLE_BLOCKS) {
      assert(peak <= 0.475f);
    }
    assert(side > loopFrames[track] * 0.005 && "track collapsed to mono");
    float seamL = fabsf((loopBuffer[track][0] -
                         loopBuffer[track][(loopFrames[track] - 1) * 2]) /
                        32768.0f);
    float seamR = fabsf((loopBuffer[track][1] -
                         loopBuffer[track][(loopFrames[track] - 1) * 2 + 1]) /
                        32768.0f);
    assert(seamL < 0.2f && seamR < 0.2f);
  }
}

static void checkCues(void) {
  for (int style = 0; style < STYLE_COUNT; style++) {
    for (int cue = 0; cue < SFX_COUNT; cue++) {
      Voice v = {.active = true,
                 .type = (SfxId)cue,
                 .length = (int)(CUES[cue].seconds * SAMPLE_RATE),
                 .pitch = 1.0f,
                 .left = 0.707f,
                 .right = 0.707f,
                 .gain = CUES[cue].gain,
                 .style = (SoundStyle)style,
                 .rootMidi = 60,
                 .seed = 1234u + (unsigned)(style * SFX_COUNT + cue),
                 .handoff = -1};
      double energy = 0.0;
      for (v.pos = 0; v.pos < v.length; v.pos++) {
        float value = renderCueMono(&v, 0u);
        assert(isfinite(value) && fabsf(value) < 4.0f);
        energy += fabs(value);
      }
      assert(energy > 1.0 && "silent cue/style variant");
    }
  }

  memset(voices, 0, sizeof(voices));
  sfxAt((SfxId)-1, 1.0f, 0.0f);
  sfxAt(SFX_COIN, 0.0f, 0.0f);
  sfxAt(SFX_COIN, NAN, 0.0f);
  sfxAt(SFX_COIN, 1.0f, NAN);
  for (int i = 0; i < MAX_VOICES; i++) {
    assert(!voices[i].active);
  }
  for (int i = 0; i < MAX_VOICES; i++) {
    sfx(SFX_COIN);
  }
  sfx(SFX_NEWBEST);
  bool foundPriority = false;
  for (int i = 0; i < MAX_VOICES; i++) {
    foundPriority |= voices[i].type == SFX_NEWBEST;
  }
  assert(foundPriority && "high-priority cue was not voice-stolen in");

  memset(voices, 0, sizeof(voices));
  musicEraTransition(-1, 1);
  musicEraTransition(ARRAY_LEN(HANDOFFS), 1);
  for (int i = 0; i < MAX_VOICES; i++) {
    assert(!voices[i].active);
  }
  for (int handoff = 0; handoff < ARRAY_LEN(HANDOFFS); handoff++) {
    memset(voices, 0, sizeof(voices));
    int seam =
        framesForBeats(HANDOFFS[handoff].from, HANDOFFS[handoff].outBeats);
    musicEraTransition(handoff, seam);
    int to = HANDOFFS[handoff].to;
    assert(voices[0].active && voices[0].type == SFX_HANDOFF);
    assert(voices[0].musicBus && voices[0].handoff == handoff);
    assert(voices[0].handoffSeam == seam && voices[0].length > seam);
    assert(voices[0].style == TRACKS[to].style);
    assert(voices[0].rootMidi == TRACKS[to].rootMidi);
  }
}

static void checkDynamics(void) {
  resetPlaybackTest();
  curTrack = prevTrack = TRACK_MENU;
  loopPos[TRACK_MENU] = 1234;
  setTrack(TRACK_SHOP);
  assert(curTrack == TRACK_SHOP && loopPos[TRACK_SHOP] == 1234 &&
         crossfade == 0.0f);
  assert(crossfadeFrames == framesForBeats(TRACK_SHOP, 2.0f));
  setTrack(TRACK_HILLS);
  assert(curTrack == TRACK_HILLS && loopPos[TRACK_HILLS] == 0 &&
         pendingTrack == TRACK_NONE);

  curTrack = prevTrack = TRACK_HILLS;
  Stereo noFinale = renderFinale(0, 1.0f);
  assert(noFinale.l == 0.0f && noFinale.r == 0.0f);
  curTrack = prevTrack = TRACK_BLOCKS;
  crossfade = 1.0f;
  float low = 0.0f, high = 0.0f, mono = 0.0f, danger = 0.0f;
  float finale = 0.0f;
  for (unsigned i = 0; i < SAMPLE_RATE * 2u; i++) {
    Stereo silent = renderComboLayer(i, 0);
    assert(silent.l == 0.0f && silent.r == 0.0f);
    Stereo a = renderComboLayer(i, 4);
    Stereo b = renderComboLayer(i, 18);
    Stereo d = renderDanger(i, 1.0f);
    Stereo f = renderFinale(i, 1.0f);
    low += fabsf(a.l) + fabsf(a.r);
    high += fabsf(b.l) + fabsf(b.r);
    mono += fabsf(b.l + b.r);
    danger += fabsf(d.l) + fabsf(d.r);
    finale += fabsf(f.l) + fabsf(f.r);
  }
  assert(low > 1.0f && high > low * 1.5f);
  assert(mono > high * 0.4f && "combo layer cancels when folded to mono");
  assert(danger > 1.0f && finale > 1.0f);

  setMusicDynamics(-1, 2.0f, -1.0f);
  assert(musicCombo == 0 && dangerTarget == 1.0f && finaleTarget == 0.0f);
  setMusicPaused(true);
  assert(pauseTarget == 1.0f);
  musicMonsterHit();
  assert(hitFrames == HIT_HOLD_FRAMES + HIT_RECOVER_FRAMES);
  assert(hitRestore() == 0.0f);
}

static int prepareHandoffTest(int handoff) {
  resetPlaybackTest();
  const Handoff *def = &HANDOFFS[handoff];
  curTrack = prevTrack = def->from;
  loopPos[def->from] = framesForBeats(def->from, 0.4f);
  int expected = barBoundaryFrame(def->from, 1) - loopPos[def->from];
  setTrack(def->to);
  assert(curTrack == (int)def->from && pendingTrack == (int)def->to);
  assert(pendingHandoff == handoff && pendingFrames == expected);
  assert(pendingFadeFrames == framesForBeats(def->from, def->outBeats));
  setTrack(def->to);
  assert(pendingFrames == expected); // repeated requests are idempotent
  return expected;
}

static void checkHandoffs(void) {
  int reboot = handoffFor(TRACK_BLOCKS, TRACK_HILLS);
  float filter, fade;
  handoffMixShape(reboot, 0.25f, &filter, &fade);
  assert(filter > 0.0f && fade == 0.0f);
  handoffMixShape(reboot, 0.5f, &filter, &fade);
  assert(filter == 1.0f && fade == 0.0f);
  handoffMixShape(reboot, 0.75f, &filter, &fade);
  assert(filter == 1.0f && fabsf(fade - 0.5f) < 0.0001f);
  handoffMixShape(0, 0.25f, &filter, &fade);
  assert(filter == fade && filter > 0.0f);

  for (int handoff = 0; handoff < ARRAY_LEN(HANDOFFS); handoff++) {
    const Handoff *def = &HANDOFFS[handoff];
    int seam = prepareHandoffTest(handoff);
    int total = seam + SAMPLE_RATE * 3;
    double sum = 0.0, side = 0.0;
    float peak = 0.0f;
    Stereo before = {0.0f, 0.0f};
    float seamJump = 0.0f;
    for (int i = 0; i < total; i++) {
      if (i < seam) {
        assert(curTrack == (int)def->from && loopPos[def->to] == 0);
      }
      Stereo frame = renderAudioFrame();
      assert(isfinite(frame.l) && isfinite(frame.r));
      sum += ((double)frame.l * frame.l + (double)frame.r * frame.r) * 0.5;
      side += fabsf(frame.l - frame.r);
      peak = fmaxf(peak, fmaxf(fabsf(frame.l), fabsf(frame.r)));
      if (i == seam - 1) {
        before = frame;
        assert(pendingFrames == 0 && curTrack == (int)def->from);
      } else if (i == seam) {
        seamJump = fmaxf(fabsf(frame.l - before.l), fabsf(frame.r - before.r));
        assert(curTrack == (int)def->to && prevTrack == TRACK_NONE);
        assert(loopPos[def->to] == 1); // destination began at frame zero
      }
    }
    float rms = sqrtf((float)(sum / total));
    assert(curTrack == (int)def->to && pendingTrack == TRACK_NONE);
    assert(rms > 0.035f && rms < 0.22f);
    assert(peak > 0.12f && peak < 0.95f); // final tanh limiter stayed clean
    assert(side > total * 0.002 && "handoff collapsed to mono");
    assert(seamJump < 0.2f && "handoff made a discontinuous sample jump");
    printf("handoff %d->%d: wait %.2fs rms %.3f peak %.3f seam %.3f\n",
           def->from, def->to, (float)seam / SAMPLE_RATE, rms, peak, seamJump);
  }

  int due = prepareHandoffTest(0);
  setTrack(TRACK_NIGHT);
  assert(pendingTrack == TRACK_SKIES && pendingFrames == due);
  setTrack(TRACK_HILLS);
  assert(pendingTrack == TRACK_NONE); // returning to the source cancels

  prepareHandoffTest(0);
  while (!pendingCueStarted) {
    renderAudioFrame();
  }
  setTrack(TRACK_NIGHT);
  assert(pendingTrack == TRACK_SKIES); // an audible bridge finishes its bar
  int held = pendingFrames;
  setTrack(TRACK_COUNT + 3);
  assert(pendingTrack == TRACK_SKIES && pendingFrames == held);

  resetPlaybackTest();
  curTrack = prevTrack = TRACK_HILLS;
  int lead = framesForBeats(TRACK_HILLS, HANDOFFS[0].outBeats);
  loopPos[TRACK_HILLS] = loopFrames[TRACK_HILLS] - lead / 2;
  setTrack(TRACK_SKIES);
  int acrossLoop = lead / 2 + barBoundaryFrame(TRACK_HILLS, 1);
  assert(pendingFrames == acrossLoop); // too little lead defers past loop zero

  int savedFrames = loopFrames[TRACK_HILLS];
  loopFrames[TRACK_HILLS] = -1;
  resetPlaybackTest();
  curTrack = prevTrack = TRACK_HILLS;
  setTrack(TRACK_SKIES);
  assert(curTrack == TRACK_SKIES && pendingTrack == TRACK_NONE);
  Stereo degraded = renderAudioFrame();
  assert(isfinite(degraded.l) && isfinite(degraded.r));
  loopFrames[TRACK_HILLS] = savedFrames;
}

static void put16(FILE *file, unsigned value) {
  fputc(value & 0xffu, file);
  fputc((value >> 8) & 0xffu, file);
}

static void put32(FILE *file, unsigned value) {
  put16(file, value & 0xffffu);
  put16(file, value >> 16);
}

static void writeWavHeader(FILE *file, int frames) {
  unsigned dataBytes = (unsigned)frames * CHANNELS * 2u;
  fwrite("RIFF", 1, 4, file);
  put32(file, 36u + dataBytes);
  fwrite("WAVEfmt ", 1, 8, file);
  put32(file, 16);
  put16(file, 1);
  put16(file, CHANNELS);
  put32(file, SAMPLE_RATE);
  put32(file, SAMPLE_RATE * CHANNELS * 2);
  put16(file, CHANNELS * 2);
  put16(file, 16);
  fwrite("data", 1, 4, file);
  put32(file, dataBytes);
}

static const char *const TRACK_NAMES[] = {
    "sunny", "skies", "night",   "maze",    "blocks",
    "menu",  "shop",  "credits", "results",
};
STATIC_ASSERT(ARRAY_LEN(TRACK_NAMES) == TRACK_COUNT, track_names_full);

static void writeTrack(const char *prefix, int track, const char *name) {
  char path[1024];
  snprintf(path, sizeof(path), "%s-%s.wav", prefix, name);
  FILE *file = fopen(path, "wb");
  assert(file != NULL);
  writeWavHeader(file, loopFrames[track]);
  for (int i = 0; i < loopFrames[track] * 2; i++) {
    put16(file, (unsigned short)loopBuffer[track][i]);
  }
  fclose(file);
}

static void writeHandoff(const char *prefix, int handoff) {
  const Handoff *def = &HANDOFFS[handoff];
  int frames = prepareHandoffTest(handoff) + SAMPLE_RATE * 3;
  char path[1024];
  snprintf(path, sizeof(path), "%s-%s-to-%s.wav", prefix,
           TRACK_NAMES[def->from], TRACK_NAMES[def->to]);
  FILE *file = fopen(path, "wb");
  assert(file != NULL);
  writeWavHeader(file, frames);
  for (int i = 0; i < frames; i++) {
    Stereo frame = renderAudioFrame();
    put16(file, (unsigned short)lrintf(frame.l * 32767.0f));
    put16(file, (unsigned short)lrintf(frame.r * 32767.0f));
  }
  fclose(file);
}

static void exportAudio(const char *prefix) {
  for (int i = 0; i < TRACK_COUNT; i++) {
    writeTrack(prefix, i, TRACK_NAMES[i]);
  }
  for (int i = 0; i < ARRAY_LEN(HANDOFFS); i++) {
    writeHandoff(prefix, i);
  }
}

int main(int argc, char **argv) {
  checkDefinitions();
  assert(PRELOAD_ORDER[0] == TRACK_MENU);
  while (!audioPreloadStep()) {
  }
  assert(loopBuffer[TRACK_MENU] != NULL);
  checkTracks();
  checkCues();
  checkDynamics();
  checkHandoffs();
  if (argc > 1) {
    exportAudio(argv[1]);
  }
  AudioStream stream = {0};
  deinitAudio(&stream);
  printf("ok - %d stereo cues, %d effects, %d musical handoffs\n", TRACK_COUNT,
         SFX_COUNT, ARRAY_LEN(HANDOFFS));
  return 0;
}
#endif
