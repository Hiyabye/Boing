#include "sound.h"

#include "raylib.h"

#include <math.h>
#include <stdbool.h>
#include <stdlib.h>

#define BUFFER_SIZE 4096
#define SAMPLE_RATE 44100
#define LOOP_DURATION 48.0f
#define LOOP_SAMPLES ((int)(LOOP_DURATION * SAMPLE_RATE))

// Note frequencies (Hz), scientific pitch notation.
#define E2 82.41f
#define GS2 103.83f
#define A2 110.00f
#define B2 123.47f
#define C3 130.81f
#define D3 146.83f
#define E3 164.81f
#define GS3 207.65f
#define A3 220.00f
#define B3 246.94f
#define GS4 415.30f
#define A4 440.00f
#define B4 493.88f
#define C4 261.63f
#define D4 293.66f
#define E4 329.62f
#define G4 392.00f
#define C5 523.25f
#define D5 587.33f
#define E5 659.25f
#define F5 698.46f
#define G5 783.99f
#define GS5 830.61f
#define A5 880.00f
#define C6 1046.5f

struct Note {
  float frequency;
  float start;    // start time within the loop (seconds)
  float duration; // length of the note (seconds)
};

// Track 0 — Nature theme (the original warm arrangement).
static const struct Note NOTES0[] = {
    // RIGHT HAND
    {E5, 0.0f, 0.45f},
    {B4, 0.5f, 0.2f},
    {C5, 0.75f, 0.2f},
    {D5, 1.0f, 0.45f},
    {C5, 1.5f, 0.2f},
    {B4, 1.75f, 0.2f},
    {A4, 2.0f, 0.45f},
    {A4, 2.5f, 0.2f},
    {C5, 2.75f, 0.2f},
    {E5, 3.0f, 0.45f},
    {D5, 3.5f, 0.2f},
    {C5, 3.75f, 0.2f},
    {B4, 4.0f, 0.7f},
    {C5, 4.75f, 0.2f},
    {D5, 5.0f, 0.45f},
    {E5, 5.5f, 0.45f},
    {C5, 6.0f, 0.45f},
    {A4, 6.5f, 0.45f},
    {A4, 7.0f, 0.45f},
    {D5, 8.25f, 0.45f},
    {F5, 8.75f, 0.2f},
    {A5, 9.0f, 0.45f},
    {G5, 9.5f, 0.2f},
    {F5, 9.75f, 0.2f},
    {E5, 10.0f, 0.7f},
    {C5, 10.75f, 0.2f},
    {E5, 11.0f, 0.45f},
    {D5, 11.5f, 0.2f},
    {C5, 11.75f, 0.2f},
    {B4, 12.0f, 0.7f},
    {C5, 12.75f, 0.2f},
    {D5, 13.0f, 0.45f},
    {E5, 13.5f, 0.45f},
    {C5, 14.0f, 0.45f},
    {A4, 14.5f, 0.45f},
    {A4, 15.0f, 0.45f},
    {E5, 16.0f, 0.95f},
    {C5, 17.0f, 0.95f},
    {D5, 18.0f, 0.95f},
    {B4, 19.0f, 0.95f},
    {C5, 20.0f, 0.95f},
    {A4, 21.0f, 0.95f},
    {GS4, 22.0f, 1.95f},
    {E5, 24.0f, 0.95f},
    {C5, 25.0f, 0.95f},
    {D5, 26.0f, 0.95f},
    {B4, 27.0f, 0.95f},
    {C5, 28.0f, 0.45f},
    {E5, 28.5f, 0.45f},
    {A5, 29.0f, 0.45f},
    {A5, 29.5f, 0.45f},
    {GS5, 30.0f, 1.95f},
    {E5, 32.0f, 0.45f},
    {B4, 32.5f, 0.2f},
    {C5, 32.75f, 0.2f},
    {D5, 33.0f, 0.45f},
    {C5, 33.5f, 0.2f},
    {B4, 33.75f, 0.2f},
    {A4, 34.0f, 0.45f},
    {A4, 34.5f, 0.2f},
    {C5, 34.75f, 0.2f},
    {E5, 35.0f, 0.45f},
    {D5, 35.5f, 0.2f},
    {C5, 35.75f, 0.2f},
    {B4, 36.0f, 0.7f},
    {C5, 36.75f, 0.2f},
    {D5, 37.0f, 0.45f},
    {E5, 37.5f, 0.45f},
    {C5, 38.0f, 0.45f},
    {A4, 38.5f, 0.45f},
    {A4, 39.0f, 0.45f},
    {D5, 40.25f, 0.45f},
    {F5, 40.75f, 0.2f},
    {A5, 41.0f, 0.45f},
    {G5, 41.5f, 0.2f},
    {F5, 41.75f, 0.2f},
    {E5, 42.25f, 0.45f},
    {C5, 42.75f, 0.2f},
    {E5, 43.0f, 0.45f},
    {D5, 43.5f, 0.2f},
    {C5, 43.75f, 0.2f},
    {B4, 44.25f, 0.45f},
    {C5, 44.75f, 0.2f},
    {D5, 45.0f, 0.45f},
    {E5, 45.5f, 0.45f},
    {C5, 46.25f, 0.45f},
    {A4, 46.75f, 0.2f},
    {A4, 47.0f, 0.45f},
    // LEFT HAND
    {E2, 0.0f, 0.25f},
    {E3, 0.25f, 0.25f},
    {E2, 0.5f, 0.25f},
    {E3, 0.75f, 0.25f},
    {E2, 1.0f, 0.25f},
    {E3, 1.25f, 0.25f},
    {E2, 1.5f, 0.25f},
    {E3, 1.75f, 0.25f},
    {A2, 2.0f, 0.25f},
    {A3, 2.25f, 0.25f},
    {A2, 2.5f, 0.25f},
    {A3, 2.75f, 0.25f},
    {A2, 3.0f, 0.25f},
    {A3, 3.25f, 0.25f},
    {A2, 3.5f, 0.25f},
    {A3, 3.75f, 0.25f},
    {B2, 4.0f, 0.25f},
    {B3, 4.25f, 0.25f},
    {B2, 4.5f, 0.25f},
    {B3, 4.75f, 0.25f},
    {B2, 5.0f, 0.25f},
    {B3, 5.25f, 0.25f},
    {B2, 5.5f, 0.25f},
    {B3, 5.75f, 0.25f},
    {A2, 6.0f, 0.25f},
    {A3, 6.25f, 0.25f},
    {A2, 6.5f, 0.25f},
    {A3, 6.75f, 0.25f},
    {A2, 7.0f, 0.25f},
    {A3, 7.25f, 0.25f},
    {A2, 7.5f, 0.25f},
    {A3, 7.75f, 0.25f},
    {D3, 8.0f, 0.25f},
    {D4, 8.25f, 0.25f},
    {D3, 8.5f, 0.25f},
    {D4, 8.75f, 0.25f},
    {D3, 9.0f, 0.25f},
    {D4, 9.25f, 0.25f},
    {D3, 9.5f, 0.25f},
    {D4, 9.75f, 0.25f},
    {C3, 10.0f, 0.25f},
    {C4, 10.25f, 0.25f},
    {C3, 10.5f, 0.25f},
    {C4, 10.75f, 0.25f},
    {C3, 11.0f, 0.25f},
    {C4, 11.25f, 0.25f},
    {C3, 11.5f, 0.25f},
    {C4, 11.75f, 0.25f},
    {B2, 12.0f, 0.25f},
    {B3, 12.25f, 0.25f},
    {B2, 12.5f, 0.25f},
    {B3, 12.75f, 0.25f},
    {B2, 13.0f, 0.25f},
    {B3, 13.25f, 0.25f},
    {B2, 13.5f, 0.25f},
    {B3, 13.75f, 0.25f},
    {A2, 14.0f, 0.25f},
    {A3, 14.25f, 0.25f},
    {A2, 14.5f, 0.25f},
    {A3, 14.75f, 0.25f},
    {A2, 15.0f, 0.25f},
    {A3, 15.25f, 0.25f},
    {A2, 15.5f, 0.25f},
    {A3, 15.75f, 0.25f},
    {A2, 16.0f, 0.25f},
    {A3, 16.25f, 0.25f},
    {A2, 16.5f, 0.25f},
    {A3, 16.75f, 0.25f},
    {A2, 17.0f, 0.25f},
    {A3, 17.25f, 0.25f},
    {A2, 17.5f, 0.25f},
    {A3, 17.75f, 0.25f},
    {GS2, 18.0f, 0.25f},
    {GS3, 18.25f, 0.25f},
    {GS2, 18.5f, 0.25f},
    {GS3, 18.75f, 0.25f},
    {GS2, 19.0f, 0.25f},
    {GS3, 19.25f, 0.25f},
    {GS2, 19.5f, 0.25f},
    {GS3, 19.75f, 0.25f},
    {A2, 20.0f, 0.25f},
    {A3, 20.25f, 0.25f},
    {A2, 20.5f, 0.25f},
    {A3, 20.75f, 0.25f},
    {A2, 21.0f, 0.25f},
    {A3, 21.25f, 0.25f},
    {A2, 21.5f, 0.25f},
    {A3, 21.75f, 0.25f},
    {B2, 22.0f, 0.25f},
    {B3, 22.25f, 0.25f},
    {B2, 22.5f, 0.25f},
    {B3, 22.75f, 0.25f},
    {B2, 23.0f, 0.25f},
    {B3, 23.25f, 0.25f},
    {B2, 23.5f, 0.25f},
    {B3, 23.75f, 0.25f},
    {A2, 24.0f, 0.25f},
    {A3, 24.25f, 0.25f},
    {A2, 24.5f, 0.25f},
    {A3, 24.75f, 0.25f},
    {A2, 25.0f, 0.25f},
    {A3, 25.25f, 0.25f},
    {A2, 25.5f, 0.25f},
    {A3, 25.75f, 0.25f},
    {B2, 26.0f, 0.25f},
    {B3, 26.25f, 0.25f},
    {B2, 26.5f, 0.25f},
    {B3, 26.75f, 0.25f},
    {B2, 27.0f, 0.25f},
    {B3, 27.25f, 0.25f},
    {B2, 27.5f, 0.25f},
    {B3, 27.75f, 0.25f},
    {C3, 28.0f, 0.25f},
    {C4, 28.25f, 0.25f},
    {C3, 28.5f, 0.25f},
    {C4, 28.75f, 0.25f},
    {C3, 29.0f, 0.25f},
    {C4, 29.25f, 0.25f},
    {C3, 29.5f, 0.25f},
    {C4, 29.75f, 0.25f},
    {B2, 30.0f, 0.25f},
    {B3, 30.25f, 0.25f},
    {B2, 30.5f, 0.25f},
    {B3, 30.75f, 0.25f},
    {B2, 31.0f, 0.25f},
    {B3, 31.25f, 0.25f},
    {B2, 31.5f, 0.25f},
    {B3, 31.75f, 0.25f},
    {E2, 32.0f, 0.25f},
    {E3, 32.25f, 0.25f},
    {E2, 32.5f, 0.25f},
    {E3, 32.75f, 0.25f},
    {E2, 33.0f, 0.25f},
    {E3, 33.25f, 0.25f},
    {E2, 33.5f, 0.25f},
    {E3, 33.75f, 0.25f},
    {A2, 34.0f, 0.25f},
    {A3, 34.25f, 0.25f},
    {A2, 34.5f, 0.25f},
    {A3, 34.75f, 0.25f},
    {A2, 35.0f, 0.25f},
    {A3, 35.25f, 0.25f},
    {A2, 35.5f, 0.25f},
    {A3, 35.75f, 0.25f},
    {B2, 36.0f, 0.25f},
    {B3, 36.25f, 0.25f},
    {B2, 36.5f, 0.25f},
    {B3, 36.75f, 0.25f},
    {B2, 37.0f, 0.25f},
    {B3, 37.25f, 0.25f},
    {B2, 37.5f, 0.25f},
    {B3, 37.75f, 0.25f},
    {A2, 38.0f, 0.25f},
    {A3, 38.25f, 0.25f},
    {A2, 38.5f, 0.25f},
    {A3, 38.75f, 0.25f},
    {A2, 39.0f, 0.25f},
    {A3, 39.25f, 0.25f},
    {A2, 39.5f, 0.25f},
    {A3, 39.75f, 0.25f},
    {D3, 40.0f, 0.25f},
    {D4, 40.25f, 0.25f},
    {D3, 40.5f, 0.25f},
    {D4, 40.75f, 0.25f},
    {D3, 41.0f, 0.25f},
    {D4, 41.25f, 0.25f},
    {D3, 41.5f, 0.25f},
    {D4, 41.75f, 0.25f},
    {C3, 42.0f, 0.25f},
    {C4, 42.25f, 0.25f},
    {C3, 42.5f, 0.25f},
    {C4, 42.75f, 0.25f},
    {C3, 43.0f, 0.25f},
    {C4, 43.25f, 0.25f},
    {C3, 43.5f, 0.25f},
    {C4, 43.75f, 0.25f},
    {B2, 44.0f, 0.25f},
    {B3, 44.25f, 0.25f},
    {B2, 44.5f, 0.25f},
    {B3, 44.75f, 0.25f},
    {B2, 45.0f, 0.25f},
    {B3, 45.25f, 0.25f},
    {B2, 45.5f, 0.25f},
    {B3, 45.75f, 0.25f},
    {A2, 46.0f, 0.25f},
    {A3, 46.25f, 0.25f},
    {A2, 46.5f, 0.25f},
    {A3, 46.75f, 0.25f},
    {A2, 47.0f, 0.25f},
    {A3, 47.25f, 0.25f},
    {A2, 47.5f, 0.25f},
    {A3, 47.75f, 0.25f},
};

// Track 1 — Arcade theme. PLACEHOLDER: add the melody here, same
// {frequency, start, duration} form as NOTES0 above (start/duration in seconds
// within the 48s loop). The A2 tonic-pedal pad plays under every track, so this
// is not silent while empty. Keep the loop length at LOOP_DURATION (48s) so the
// shared pedal stays seamless.
static const struct Note NOTES1[] = {
    // RIGHT HAND
    {E5, 0.0f, 0.45f},
    {B4, 0.5f, 0.2f},
    {C5, 0.75f, 0.2f},
    {D5, 1.0f, 0.45f},
    {C5, 1.5f, 0.2f},
    {B4, 1.75f, 0.2f},
    {A4, 2.0f, 0.45f},
    {A4, 2.5f, 0.2f},
    {C5, 2.75f, 0.2f},
    {E5, 3.0f, 0.45f},
    {D5, 3.5f, 0.2f},
    {C5, 3.75f, 0.2f},
    {B4, 4.0f, 0.7f},
    {C5, 4.75f, 0.2f},
    {D5, 5.0f, 0.45f},
    {E5, 5.5f, 0.45f},
    {C5, 6.0f, 0.45f},
    {A4, 6.5f, 0.45f},
    {A4, 7.0f, 0.45f},
    {D5, 8.25f, 0.45f},
    {F5, 8.75f, 0.2f},
    {A5, 9.0f, 0.45f},
    {G5, 9.5f, 0.2f},
    {F5, 9.75f, 0.2f},
    {E5, 10.0f, 0.7f},
    {C5, 10.75f, 0.2f},
    {E5, 11.0f, 0.45f},
    {D5, 11.5f, 0.2f},
    {C5, 11.75f, 0.2f},
    {B4, 12.0f, 0.7f},
    {C5, 12.75f, 0.2f},
    {D5, 13.0f, 0.45f},
    {E5, 13.5f, 0.45f},
    {C5, 14.0f, 0.45f},
    {A4, 14.5f, 0.45f},
    {A4, 15.0f, 0.45f},
    {E5, 16.0f, 0.95f},
    {C5, 17.0f, 0.95f},
    {D5, 18.0f, 0.95f},
    {B4, 19.0f, 0.95f},
    {C5, 20.0f, 0.95f},
    {A4, 21.0f, 0.95f},
    {GS4, 22.0f, 1.95f},
    {E5, 24.0f, 0.95f},
    {C5, 25.0f, 0.95f},
    {D5, 26.0f, 0.95f},
    {B4, 27.0f, 0.95f},
    {C5, 28.0f, 0.45f},
    {E5, 28.5f, 0.45f},
    {A5, 29.0f, 0.45f},
    {A5, 29.5f, 0.45f},
    {GS5, 30.0f, 1.95f},
    {E5, 32.0f, 0.45f},
    {B4, 32.5f, 0.2f},
    {C5, 32.75f, 0.2f},
    {D5, 33.0f, 0.45f},
    {C5, 33.5f, 0.2f},
    {B4, 33.75f, 0.2f},
    {A4, 34.0f, 0.45f},
    {A4, 34.5f, 0.2f},
    {C5, 34.75f, 0.2f},
    {E5, 35.0f, 0.45f},
    {D5, 35.5f, 0.2f},
    {C5, 35.75f, 0.2f},
    {B4, 36.0f, 0.7f},
    {C5, 36.75f, 0.2f},
    {D5, 37.0f, 0.45f},
    {E5, 37.5f, 0.45f},
    {C5, 38.0f, 0.45f},
    {A4, 38.5f, 0.45f},
    {A4, 39.0f, 0.45f},
    {D5, 40.25f, 0.45f},
    {F5, 40.75f, 0.2f},
    {A5, 41.0f, 0.45f},
    {G5, 41.5f, 0.2f},
    {F5, 41.75f, 0.2f},
    {E5, 42.25f, 0.45f},
    {C5, 42.75f, 0.2f},
    {E5, 43.0f, 0.45f},
    {D5, 43.5f, 0.2f},
    {C5, 43.75f, 0.2f},
    {B4, 44.25f, 0.45f},
    {C5, 44.75f, 0.2f},
    {D5, 45.0f, 0.45f},
    {E5, 45.5f, 0.45f},
    {C5, 46.25f, 0.45f},
    {A4, 46.75f, 0.2f},
    {A4, 47.0f, 0.45f},
    // LEFT HAND
    {E2, 0.0f, 0.25f},
    {E3, 0.25f, 0.25f},
    {E2, 0.5f, 0.25f},
    {E3, 0.75f, 0.25f},
    {E2, 1.0f, 0.25f},
    {E3, 1.25f, 0.25f},
    {E2, 1.5f, 0.25f},
    {E3, 1.75f, 0.25f},
    {A2, 2.0f, 0.25f},
    {A3, 2.25f, 0.25f},
    {A2, 2.5f, 0.25f},
    {A3, 2.75f, 0.25f},
    {A2, 3.0f, 0.25f},
    {A3, 3.25f, 0.25f},
    {A2, 3.5f, 0.25f},
    {A3, 3.75f, 0.25f},
    {B2, 4.0f, 0.25f},
    {B3, 4.25f, 0.25f},
    {B2, 4.5f, 0.25f},
    {B3, 4.75f, 0.25f},
    {B2, 5.0f, 0.25f},
    {B3, 5.25f, 0.25f},
    {B2, 5.5f, 0.25f},
    {B3, 5.75f, 0.25f},
    {A2, 6.0f, 0.25f},
    {A3, 6.25f, 0.25f},
    {A2, 6.5f, 0.25f},
    {A3, 6.75f, 0.25f},
    {A2, 7.0f, 0.25f},
    {A3, 7.25f, 0.25f},
    {A2, 7.5f, 0.25f},
    {A3, 7.75f, 0.25f},
    {D3, 8.0f, 0.25f},
    {D4, 8.25f, 0.25f},
    {D3, 8.5f, 0.25f},
    {D4, 8.75f, 0.25f},
    {D3, 9.0f, 0.25f},
    {D4, 9.25f, 0.25f},
    {D3, 9.5f, 0.25f},
    {D4, 9.75f, 0.25f},
    {C3, 10.0f, 0.25f},
    {C4, 10.25f, 0.25f},
    {C3, 10.5f, 0.25f},
    {C4, 10.75f, 0.25f},
    {C3, 11.0f, 0.25f},
    {C4, 11.25f, 0.25f},
    {C3, 11.5f, 0.25f},
    {C4, 11.75f, 0.25f},
    {B2, 12.0f, 0.25f},
    {B3, 12.25f, 0.25f},
    {B2, 12.5f, 0.25f},
    {B3, 12.75f, 0.25f},
    {B2, 13.0f, 0.25f},
    {B3, 13.25f, 0.25f},
    {B2, 13.5f, 0.25f},
    {B3, 13.75f, 0.25f},
    {A2, 14.0f, 0.25f},
    {A3, 14.25f, 0.25f},
    {A2, 14.5f, 0.25f},
    {A3, 14.75f, 0.25f},
    {A2, 15.0f, 0.25f},
    {A3, 15.25f, 0.25f},
    {A2, 15.5f, 0.25f},
    {A3, 15.75f, 0.25f},
    {A2, 16.0f, 0.25f},
    {A3, 16.25f, 0.25f},
    {A2, 16.5f, 0.25f},
    {A3, 16.75f, 0.25f},
    {A2, 17.0f, 0.25f},
    {A3, 17.25f, 0.25f},
    {A2, 17.5f, 0.25f},
    {A3, 17.75f, 0.25f},
    {GS2, 18.0f, 0.25f},
    {GS3, 18.25f, 0.25f},
    {GS2, 18.5f, 0.25f},
    {GS3, 18.75f, 0.25f},
    {GS2, 19.0f, 0.25f},
    {GS3, 19.25f, 0.25f},
    {GS2, 19.5f, 0.25f},
    {GS3, 19.75f, 0.25f},
    {A2, 20.0f, 0.25f},
    {A3, 20.25f, 0.25f},
    {A2, 20.5f, 0.25f},
    {A3, 20.75f, 0.25f},
    {A2, 21.0f, 0.25f},
    {A3, 21.25f, 0.25f},
    {A2, 21.5f, 0.25f},
    {A3, 21.75f, 0.25f},
    {B2, 22.0f, 0.25f},
    {B3, 22.25f, 0.25f},
    {B2, 22.5f, 0.25f},
    {B3, 22.75f, 0.25f},
    {B2, 23.0f, 0.25f},
    {B3, 23.25f, 0.25f},
    {B2, 23.5f, 0.25f},
    {B3, 23.75f, 0.25f},
    {A2, 24.0f, 0.25f},
    {A3, 24.25f, 0.25f},
    {A2, 24.5f, 0.25f},
    {A3, 24.75f, 0.25f},
    {A2, 25.0f, 0.25f},
    {A3, 25.25f, 0.25f},
    {A2, 25.5f, 0.25f},
    {A3, 25.75f, 0.25f},
    {B2, 26.0f, 0.25f},
    {B3, 26.25f, 0.25f},
    {B2, 26.5f, 0.25f},
    {B3, 26.75f, 0.25f},
    {B2, 27.0f, 0.25f},
    {B3, 27.25f, 0.25f},
    {B2, 27.5f, 0.25f},
    {B3, 27.75f, 0.25f},
    {C3, 28.0f, 0.25f},
    {C4, 28.25f, 0.25f},
    {C3, 28.5f, 0.25f},
    {C4, 28.75f, 0.25f},
    {C3, 29.0f, 0.25f},
    {C4, 29.25f, 0.25f},
    {C3, 29.5f, 0.25f},
    {C4, 29.75f, 0.25f},
    {B2, 30.0f, 0.25f},
    {B3, 30.25f, 0.25f},
    {B2, 30.5f, 0.25f},
    {B3, 30.75f, 0.25f},
    {B2, 31.0f, 0.25f},
    {B3, 31.25f, 0.25f},
    {B2, 31.5f, 0.25f},
    {B3, 31.75f, 0.25f},
    {E2, 32.0f, 0.25f},
    {E3, 32.25f, 0.25f},
    {E2, 32.5f, 0.25f},
    {E3, 32.75f, 0.25f},
    {E2, 33.0f, 0.25f},
    {E3, 33.25f, 0.25f},
    {E2, 33.5f, 0.25f},
    {E3, 33.75f, 0.25f},
    {A2, 34.0f, 0.25f},
    {A3, 34.25f, 0.25f},
    {A2, 34.5f, 0.25f},
    {A3, 34.75f, 0.25f},
    {A2, 35.0f, 0.25f},
    {A3, 35.25f, 0.25f},
    {A2, 35.5f, 0.25f},
    {A3, 35.75f, 0.25f},
    {B2, 36.0f, 0.25f},
    {B3, 36.25f, 0.25f},
    {B2, 36.5f, 0.25f},
    {B3, 36.75f, 0.25f},
    {B2, 37.0f, 0.25f},
    {B3, 37.25f, 0.25f},
    {B2, 37.5f, 0.25f},
    {B3, 37.75f, 0.25f},
    {A2, 38.0f, 0.25f},
    {A3, 38.25f, 0.25f},
    {A2, 38.5f, 0.25f},
    {A3, 38.75f, 0.25f},
    {A2, 39.0f, 0.25f},
    {A3, 39.25f, 0.25f},
    {A2, 39.5f, 0.25f},
    {A3, 39.75f, 0.25f},
    {D3, 40.0f, 0.25f},
    {D4, 40.25f, 0.25f},
    {D3, 40.5f, 0.25f},
    {D4, 40.75f, 0.25f},
    {D3, 41.0f, 0.25f},
    {D4, 41.25f, 0.25f},
    {D3, 41.5f, 0.25f},
    {D4, 41.75f, 0.25f},
    {C3, 42.0f, 0.25f},
    {C4, 42.25f, 0.25f},
    {C3, 42.5f, 0.25f},
    {C4, 42.75f, 0.25f},
    {C3, 43.0f, 0.25f},
    {C4, 43.25f, 0.25f},
    {C3, 43.5f, 0.25f},
    {C4, 43.75f, 0.25f},
    {B2, 44.0f, 0.25f},
    {B3, 44.25f, 0.25f},
    {B2, 44.5f, 0.25f},
    {B3, 44.75f, 0.25f},
    {B2, 45.0f, 0.25f},
    {B3, 45.25f, 0.25f},
    {B2, 45.5f, 0.25f},
    {B3, 45.75f, 0.25f},
    {A2, 46.0f, 0.25f},
    {A3, 46.25f, 0.25f},
    {A2, 46.5f, 0.25f},
    {A3, 46.75f, 0.25f},
    {A2, 47.0f, 0.25f},
    {A3, 47.25f, 0.25f},
    {A2, 47.5f, 0.25f},
    {A3, 47.75f, 0.25f},
};

// A track is just its note list; all tracks share LOOP_DURATION and the pedal.
struct Track {
  const struct Note *notes;
  int count;
};
#define TRACK(a) {(a), (int)(sizeof(a) / sizeof((a)[0]))}
static const struct Track TRACKS[] = {
    TRACK(NOTES0),
    TRACK(NOTES1),
};
#define TRACK_COUNT ((int)(sizeof(TRACKS) / sizeof(TRACKS[0])))

// One precomputed loop buffer per track. Playback crossfades between the
// currently-selected track and the one we're fading out of.
static float *loopBuffer[TRACK_COUNT];
static int loopPos = 0;
static int curTrack = 0;  // track fading in / currently playing
static int prevTrack = 0; // track fading out
static float fade = 1.0f; // 0..1 weight of curTrack (1 = crossfade complete)
// Crossfade duration in samples (~2s).
#define FADE_SAMPLES (2 * SAMPLE_RATE)

// Short attack / long release so notes don't click and have a soft tail.
static float adsr(float local, float dur) {
  float a = 0.008f, r = 0.08f;
  if (local < a) {
    return local / a;
  }
  if (local > dur - r) {
    float e = (dur - local) / r;
    return e < 0.0f ? 0.0f : e;
  }
  return 1.0f;
}

// Warmer timbre: fundamental + two quieter harmonics, with a gentle vibrato.
static float tone(float freq, float local) {
  float vib = 1.0f + 0.004f * sinf(2.0f * PI * 5.5f * local);
  float ph = 2.0f * PI * freq * vib * local;
  return sinf(ph) + 0.45f * sinf(2.0f * ph) + 0.2f * sinf(3.0f * ph);
}

// Precompute one full loop of a track's arrangement. Called once per track at
// startup. ponytail: all CPU cost is here at startup, one buffer per track.
static void buildLoop(int track) {
  if (loopBuffer[track] != NULL) {
    return;
  }
  const struct Note *notes = TRACKS[track].notes;
  int noteCount = TRACKS[track].count;

  loopBuffer[track] = malloc(LOOP_SAMPLES * sizeof(float));
  float *buf = loopBuffer[track];
  float peak = 0.0001f;
  for (int i = 0; i < LOOP_SAMPLES; i++) {
    float t = (float)i / SAMPLE_RATE;
    float sample = 0.0f;
    for (int n = 0; n < noteCount; n++) {
      float local = t - notes[n].start;
      if (local >= 0.0f && local < notes[n].duration) {
        sample += 0.5f * adsr(local, notes[n].duration) *
                  tone(notes[n].frequency, local);
      }
    }
    // Soft tonic (A2) pedal pad under everything for warmth, with slow tremolo.
    // A2=110Hz over 48s is 5280 whole cycles, so the loop seam stays seamless.
    float trem = 0.85f + 0.15f * sinf(2.0f * PI * 0.2f * t);
    sample +=
        0.07f * trem *
        (sinf(2.0f * PI * A2 * t) + 0.5f * sinf(2.0f * PI * A2 * 2.0f * t));

    buf[i] = sample;
    float mag = fabsf(sample);
    if (mag > peak) {
      peak = mag;
    }
  }
  // Normalize to a safe headroom peak; the tanh limiter catches SFX on top.
  float gain = 0.9f / peak;
  for (int i = 0; i < LOOP_SAMPLES; i++) {
    buf[i] *= gain;
  }
}

// Start a crossfade to `track` (no-op if already selected). SFX switch variant
// immediately; the music crossfades over FADE_SAMPLES in updateAudio.
void setTrack(int track) {
  if (track < 0 || track >= TRACK_COUNT || track == curTrack) {
    return;
  }
  // ponytail: single 2-track crossfade — a rapid re-switch mid-fade snaps the
  // outgoing track; add a proper fade stack only if that's ever audible.
  prevTrack = curTrack;
  curTrack = track;
  fade = 0.0f;
}

// --- Procedural sound effects ------------------------------------------------
// A small pool of voices synthesized on the fly and summed into the music
// stream. No sample files: every effect is math over its sample index.

enum { SFX_COIN, SFX_BOUNCE, SFX_POWERUP, SFX_HIT, SFX_SELECT, SFX_LEVELUP };

// Effect lengths in samples.
static const int sfxLen[] = {
    [SFX_COIN] = (int)(0.12f * SAMPLE_RATE),
    [SFX_BOUNCE] = (int)(0.15f * SAMPLE_RATE),
    [SFX_POWERUP] = (int)(0.35f * SAMPLE_RATE),
    [SFX_HIT] = (int)(0.40f * SAMPLE_RATE),
    [SFX_SELECT] = (int)(0.08f * SAMPLE_RATE),
    [SFX_LEVELUP] = (int)(0.70f * SAMPLE_RATE),
};

// ponytail: fixed 8 voices; overlapping triggers past 8 drop silently — grow
// the pool only if that's ever audible.
#define MAX_VOICES 8
struct Voice {
  bool active;
  int type;
  int pos; // samples elapsed since trigger
};
static struct Voice voices[MAX_VOICES];

static void trigger(int type) {
  for (int i = 0; i < MAX_VOICES; i++) {
    if (!voices[i].active) {
      voices[i] = (struct Voice){true, type, 0};
      return;
    }
  }
}

void sfxCoin(void) { trigger(SFX_COIN); }
void sfxBounce(void) { trigger(SFX_BOUNCE); }
void sfxPowerup(void) { trigger(SFX_POWERUP); }
void sfxHit(void) { trigger(SFX_HIT); }
void sfxSelect(void) { trigger(SFX_SELECT); }
void sfxLevelUp(void) { trigger(SFX_LEVELUP); }

static float square(float freq, float t) {
  return fmodf(freq * t, 1.0f) < 0.5f ? 1.0f : -1.0f;
}

// One sample for a voice, amplitude-scaled with a linear decay envelope.
static float renderVoice(int type, int pos) {
  int len = sfxLen[type];
  int atk = (int)(0.004f * SAMPLE_RATE); // ~4ms attack removes the click
  float env = pos < atk ? (float)pos / atk : 1.0f - (float)pos / len;
  if (env < 0.0f) {
    env = 0.0f;
  }
  float t = (float)pos / SAMPLE_RATE;
  float s = 0.0f;
  // ponytail: arcade (track 1) gets a harsher, brighter 8-bit variant per
  // effect — hand-tuned, not a data table like the music.
  bool arc = (curTrack == 1);
  switch (type) {
  case SFX_COIN: // rising two-note blip (brighter on arcade)
    s = square(pos < len / 2 ? (arc ? 1174.66f : 987.77f)
                             : (arc ? 1567.98f : 1318.51f),
               t);
    break;
  case SFX_BOUNCE: { // downward pitch sweep "boing"
    float f = 400.0f - 280.0f * (float)pos / len;
    s = arc ? square(f, t) : sinf(2.0f * PI * f * t); // square = crunchier
    break;
  }
  case SFX_POWERUP: { // rising arpeggio
    static const float natNotes[3] = {523.25f, 659.25f, 783.99f};  // C5 E5 G5
    static const float arcNotes[3] = {659.25f, 987.77f, 1318.51f}; // E5 B5 E6
    const float *notes = arc ? arcNotes : natNotes;
    int k = pos * 3 / len;
    s = square(notes[k > 2 ? 2 : k], t);
    break;
  }
  case SFX_HIT: { // descending saw + noise (noisier on arcade)
    float f = 300.0f - 240.0f * (float)pos / len;
    float saw = 2.0f * (f * t - floorf(0.5f + f * t));
    float noise = (float)rand() / (float)RAND_MAX * 2.0f - 1.0f;
    s = arc ? 0.4f * saw + 0.6f * noise : 0.7f * saw + 0.3f * noise;
    break;
  }
  case SFX_SELECT:
    s = square(arc ? 880.0f : 660.0f, t);
    break;
  case SFX_LEVELUP: { // triumphant rising fanfare (C5 E5 G5 C6), full major
    static const float notes[4] = {523.25f, 659.25f, 783.99f, 1046.50f};
    int k = pos * 4 / len;
    s = square(notes[k > 3 ? 3 : k], t);
    break;
  }
  }
  return 0.35f * env * s;
}

void initAudio(AudioStream *stream) {
  InitAudioDevice();
  SetAudioStreamBufferSizeDefault(BUFFER_SIZE);

  for (int t = 0; t < TRACK_COUNT; t++) {
    buildLoop(t);
  }

  *stream = LoadAudioStream(SAMPLE_RATE, 32, 1);
  PlayAudioStream(*stream);
}

void updateAudio(AudioStream *stream) {
  if (!IsAudioStreamProcessed(*stream)) {
    return;
  }

  float buffer[BUFFER_SIZE];
  for (int i = 0; i < BUFFER_SIZE; i++) {
    // Crossfade the outgoing and incoming tracks (both share loopPos/length).
    float music = fade * loopBuffer[curTrack][loopPos] +
                  (1.0f - fade) * loopBuffer[prevTrack][loopPos];
    loopPos = (loopPos + 1) % LOOP_SAMPLES;
    if (fade < 1.0f) {
      fade += 1.0f / FADE_SAMPLES;
      if (fade > 1.0f) {
        fade = 1.0f;
      }
    }
    float s = 0.85f * music; // headroom for SFX on top

    for (int v = 0; v < MAX_VOICES; v++) {
      if (!voices[v].active) {
        continue;
      }
      s += renderVoice(voices[v].type, voices[v].pos);
      if (++voices[v].pos >= sfxLen[voices[v].type]) {
        voices[v].active = false;
      }
    }

    buffer[i] = tanhf(s); // soft limiter — warmer than a hard clamp
  }
  UpdateAudioStream(*stream, buffer, BUFFER_SIZE);
}

void deinitAudio(AudioStream *stream) {
  UnloadAudioStream(*stream);
  CloseAudioDevice();

  for (int t = 0; t < TRACK_COUNT; t++) {
    free(loopBuffer[t]);
    loopBuffer[t] = NULL;
  }
  loopPos = 0;
}
