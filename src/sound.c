#include "sound.h"

#include "raylib.h"

#include <math.h>
#include <stdbool.h>
#include <stdlib.h>

#define BUFFER_SIZE 4096
#define SAMPLE_RATE 44100

// Note frequencies (Hz), scientific pitch notation.
static const float C0 = 16.35f;
static const float CS0 = 17.32f;
static const float D0 = 18.35f;
static const float DS0 = 19.45f;
static const float E0 = 20.60f;
static const float F0 = 21.83f;
static const float FS0 = 23.12f;
static const float G0 = 24.50f;
static const float GS0 = 25.96f;
static const float A0 = 27.50f;
static const float AS0 = 29.14f;
static const float B0 = 30.87f;

static const float C1 = 32.70f;
static const float CS1 = 34.65f;
static const float D1 = 36.71f;
static const float DS1 = 38.89f;
static const float E1 = 41.20f;
static const float F1 = 43.65f;
static const float FS1 = 46.25f;
static const float G1 = 49.00f;
static const float GS1 = 51.91f;
static const float A1 = 55.00f;
static const float AS1 = 58.27f;
static const float B1 = 61.74f;

static const float C2 = 65.41f;
static const float CS2 = 69.30f;
static const float D2 = 73.42f;
static const float DS2 = 77.78f;
static const float E2 = 82.41f;
static const float F2 = 87.31f;
static const float FS2 = 92.50f;
static const float G2 = 98.00f;
static const float GS2 = 103.83f;
static const float A2 = 110.00f;
static const float AS2 = 116.54f;
static const float B2 = 123.47f;

static const float C3 = 130.81f;
static const float CS3 = 138.59f;
static const float D3 = 146.83f;
static const float DS3 = 155.56f;
static const float E3 = 164.81f;
static const float F3 = 174.61f;
static const float FS3 = 185.00f;
static const float G3 = 196.00f;
static const float GS3 = 207.65f;
static const float A3 = 220.00f;
static const float AS3 = 233.08f;
static const float B3 = 246.94f;

static const float C4 = 261.63f;
static const float CS4 = 277.18f;
static const float D4 = 293.66f;
static const float DS4 = 311.13f;
static const float E4 = 329.63f;
static const float F4 = 349.23f;
static const float FS4 = 369.99f;
static const float G4 = 392.00f;
static const float GS4 = 415.30f;
static const float A4 = 440.00f;
static const float AS4 = 466.16f;
static const float B4 = 493.88f;

static const float C5 = 523.25f;
static const float CS5 = 554.37f;
static const float D5 = 587.33f;
static const float DS5 = 622.25f;
static const float E5 = 659.25f;
static const float F5 = 698.46f;
static const float FS5 = 739.99f;
static const float G5 = 783.99f;
static const float GS5 = 830.61f;
static const float A5 = 880.00f;
static const float AS5 = 932.33f;
static const float B5 = 987.77f;

static const float C6 = 1046.50f;
static const float CS6 = 1108.73f;
static const float D6 = 1174.66f;
static const float DS6 = 1244.51f;
static const float E6 = 1318.51f;
static const float F6 = 1396.91f;
static const float FS6 = 1479.98f;
static const float G6 = 1567.98f;
static const float GS6 = 1661.22f;
static const float A6 = 1760.00f;
static const float AS6 = 1864.66f;
static const float B6 = 1975.53f;

static const float C7 = 2093.00f;
static const float CS7 = 2217.46f;
static const float D7 = 2349.32f;
static const float DS7 = 2489.00f;
static const float E7 = 2637.00f;
static const float F7 = 2793.83f;
static const float FS7 = 2959.96f;
static const float G7 = 3135.96f;
static const float GS7 = 3322.44f;
static const float A7 = 3520.00f;
static const float AS7 = 3729.31f;
static const float B7 = 3951.00f;

static const float C8 = 4186.00f;
static const float CS8 = 4434.92f;
static const float D8 = 4698.63f;
static const float DS8 = 4978.00f;
static const float E8 = 5274.00f;
static const float F8 = 5587.65f;
static const float FS8 = 5919.90f;
static const float G8 = 6271.93f;
static const float GS8 = 6644.88f;
static const float A8 = 7040.00f;
static const float AS8 = 7458.62f;
static const float B8 = 7902.13f;

struct Note {
  float frequency;
  float start;    // start time within the loop (seconds)
  float duration; // length of the note (seconds)
};

// Track 0 — Nature: original upbeat, bouncy melody (A major, 32s loop).
static const struct Note NOTES0[] = {
    {A4, 0.0f, 0.2f},
    {CS5, 0.25f, 0.2f},
    {E5, 0.5f, 0.45f},
    {E5, 1.0f, 0.2f},
    {FS5, 1.25f, 0.2f},
    {E5, 1.5f, 0.45f},
    {GS4, 2.0f, 0.2f},
    {B4, 2.25f, 0.2f},
    {E5, 2.5f, 0.45f},
    {D5, 3.0f, 0.2f},
    {B4, 3.25f, 0.2f},
    {GS4, 3.5f, 0.45f},
    {A4, 4.0f, 0.2f},
    {CS5, 4.25f, 0.2f},
    {FS5, 4.5f, 0.45f},
    {A5, 5.0f, 0.2f},
    {FS5, 5.25f, 0.2f},
    {E5, 5.5f, 0.45f},
    {D5, 6.0f, 0.2f},
    {FS5, 6.25f, 0.2f},
    {A5, 6.5f, 0.45f},
    {FS5, 7.0f, 0.2f},
    {D5, 7.25f, 0.2f},
    {A4, 7.5f, 0.45f},
    {A4, 8.0f, 0.2f},
    {CS5, 8.25f, 0.2f},
    {E5, 8.5f, 0.45f},
    {A5, 9.0f, 0.45f},
    {GS5, 9.5f, 0.2f},
    {FS5, 9.75f, 0.2f},
    {GS4, 10.0f, 0.2f},
    {B4, 10.25f, 0.2f},
    {E5, 10.5f, 0.45f},
    {GS5, 11.0f, 0.45f},
    {B4, 11.5f, 0.2f},
    {E5, 11.75f, 0.2f},
    {FS5, 12.0f, 0.45f},
    {D5, 12.5f, 0.2f},
    {A4, 12.75f, 0.2f},
    {D5, 13.0f, 0.45f},
    {FS5, 13.5f, 0.45f},
    {E5, 14.0f, 0.45f},
    {GS5, 14.5f, 0.2f},
    {B5, 14.75f, 0.2f},
    {E5, 15.0f, 0.95f},
    {A4, 16.0f, 0.2f},
    {CS5, 16.25f, 0.2f},
    {E5, 16.5f, 0.45f},
    {E5, 17.0f, 0.2f},
    {FS5, 17.25f, 0.2f},
    {E5, 17.5f, 0.45f},
    {GS4, 18.0f, 0.2f},
    {B4, 18.25f, 0.2f},
    {E5, 18.5f, 0.45f},
    {D5, 19.0f, 0.2f},
    {B4, 19.25f, 0.2f},
    {GS4, 19.5f, 0.45f},
    {A4, 20.0f, 0.2f},
    {CS5, 20.25f, 0.2f},
    {FS5, 20.5f, 0.45f},
    {A5, 21.0f, 0.2f},
    {FS5, 21.25f, 0.2f},
    {E5, 21.5f, 0.45f},
    {D5, 22.0f, 0.2f},
    {FS5, 22.25f, 0.2f},
    {A5, 22.5f, 0.45f},
    {FS5, 23.0f, 0.2f},
    {D5, 23.25f, 0.2f},
    {A4, 23.5f, 0.45f},
    {A4, 24.0f, 0.2f},
    {CS5, 24.25f, 0.2f},
    {E5, 24.5f, 0.45f},
    {A5, 25.0f, 0.45f},
    {GS5, 25.5f, 0.2f},
    {FS5, 25.75f, 0.2f},
    {GS4, 26.0f, 0.2f},
    {B4, 26.25f, 0.2f},
    {E5, 26.5f, 0.45f},
    {GS5, 27.0f, 0.45f},
    {B4, 27.5f, 0.2f},
    {E5, 27.75f, 0.2f},
    {FS5, 28.0f, 0.45f},
    {D5, 28.5f, 0.2f},
    {A4, 28.75f, 0.2f},
    {D5, 29.0f, 0.45f},
    {FS5, 29.5f, 0.45f},
    {E5, 30.0f, 0.45f},
    {GS5, 30.5f, 0.2f},
    {B5, 30.75f, 0.2f},
    {A5, 31.0f, 0.95f},
    {A2, 0.0f, 0.25f},
    {A3, 0.25f, 0.25f},
    {A2, 0.5f, 0.25f},
    {A3, 0.75f, 0.25f},
    {A2, 1.0f, 0.25f},
    {A3, 1.25f, 0.25f},
    {A2, 1.5f, 0.25f},
    {A3, 1.75f, 0.25f},
    {E2, 2.0f, 0.25f},
    {E3, 2.25f, 0.25f},
    {E2, 2.5f, 0.25f},
    {E3, 2.75f, 0.25f},
    {E2, 3.0f, 0.25f},
    {E3, 3.25f, 0.25f},
    {E2, 3.5f, 0.25f},
    {E3, 3.75f, 0.25f},
    {FS2, 4.0f, 0.25f},
    {FS3, 4.25f, 0.25f},
    {FS2, 4.5f, 0.25f},
    {FS3, 4.75f, 0.25f},
    {FS2, 5.0f, 0.25f},
    {FS3, 5.25f, 0.25f},
    {FS2, 5.5f, 0.25f},
    {FS3, 5.75f, 0.25f},
    {D2, 6.0f, 0.25f},
    {D3, 6.25f, 0.25f},
    {D2, 6.5f, 0.25f},
    {D3, 6.75f, 0.25f},
    {D2, 7.0f, 0.25f},
    {D3, 7.25f, 0.25f},
    {D2, 7.5f, 0.25f},
    {D3, 7.75f, 0.25f},
    {A2, 8.0f, 0.25f},
    {A3, 8.25f, 0.25f},
    {A2, 8.5f, 0.25f},
    {A3, 8.75f, 0.25f},
    {A2, 9.0f, 0.25f},
    {A3, 9.25f, 0.25f},
    {A2, 9.5f, 0.25f},
    {A3, 9.75f, 0.25f},
    {E2, 10.0f, 0.25f},
    {E3, 10.25f, 0.25f},
    {E2, 10.5f, 0.25f},
    {E3, 10.75f, 0.25f},
    {E2, 11.0f, 0.25f},
    {E3, 11.25f, 0.25f},
    {E2, 11.5f, 0.25f},
    {E3, 11.75f, 0.25f},
    {D2, 12.0f, 0.25f},
    {D3, 12.25f, 0.25f},
    {D2, 12.5f, 0.25f},
    {D3, 12.75f, 0.25f},
    {D2, 13.0f, 0.25f},
    {D3, 13.25f, 0.25f},
    {D2, 13.5f, 0.25f},
    {D3, 13.75f, 0.25f},
    {E2, 14.0f, 0.25f},
    {E3, 14.25f, 0.25f},
    {E2, 14.5f, 0.25f},
    {E3, 14.75f, 0.25f},
    {E2, 15.0f, 0.25f},
    {E3, 15.25f, 0.25f},
    {E2, 15.5f, 0.25f},
    {E3, 15.75f, 0.25f},
    {A2, 16.0f, 0.25f},
    {A3, 16.25f, 0.25f},
    {A2, 16.5f, 0.25f},
    {A3, 16.75f, 0.25f},
    {A2, 17.0f, 0.25f},
    {A3, 17.25f, 0.25f},
    {A2, 17.5f, 0.25f},
    {A3, 17.75f, 0.25f},
    {E2, 18.0f, 0.25f},
    {E3, 18.25f, 0.25f},
    {E2, 18.5f, 0.25f},
    {E3, 18.75f, 0.25f},
    {E2, 19.0f, 0.25f},
    {E3, 19.25f, 0.25f},
    {E2, 19.5f, 0.25f},
    {E3, 19.75f, 0.25f},
    {FS2, 20.0f, 0.25f},
    {FS3, 20.25f, 0.25f},
    {FS2, 20.5f, 0.25f},
    {FS3, 20.75f, 0.25f},
    {FS2, 21.0f, 0.25f},
    {FS3, 21.25f, 0.25f},
    {FS2, 21.5f, 0.25f},
    {FS3, 21.75f, 0.25f},
    {D2, 22.0f, 0.25f},
    {D3, 22.25f, 0.25f},
    {D2, 22.5f, 0.25f},
    {D3, 22.75f, 0.25f},
    {D2, 23.0f, 0.25f},
    {D3, 23.25f, 0.25f},
    {D2, 23.5f, 0.25f},
    {D3, 23.75f, 0.25f},
    {A2, 24.0f, 0.25f},
    {A3, 24.25f, 0.25f},
    {A2, 24.5f, 0.25f},
    {A3, 24.75f, 0.25f},
    {A2, 25.0f, 0.25f},
    {A3, 25.25f, 0.25f},
    {A2, 25.5f, 0.25f},
    {A3, 25.75f, 0.25f},
    {E2, 26.0f, 0.25f},
    {E3, 26.25f, 0.25f},
    {E2, 26.5f, 0.25f},
    {E3, 26.75f, 0.25f},
    {E2, 27.0f, 0.25f},
    {E3, 27.25f, 0.25f},
    {E2, 27.5f, 0.25f},
    {E3, 27.75f, 0.25f},
    {D2, 28.0f, 0.25f},
    {D3, 28.25f, 0.25f},
    {D2, 28.5f, 0.25f},
    {D3, 28.75f, 0.25f},
    {D2, 29.0f, 0.25f},
    {D3, 29.25f, 0.25f},
    {D2, 29.5f, 0.25f},
    {D3, 29.75f, 0.25f},
    {A2, 30.0f, 0.25f},
    {A3, 30.25f, 0.25f},
    {A2, 30.5f, 0.25f},
    {A3, 30.75f, 0.25f},
    {A2, 31.0f, 0.25f},
    {A3, 31.25f, 0.25f},
    {A2, 31.5f, 0.25f},
    {A3, 31.75f, 0.25f},
};

// Track 1 — Arcade theme. Same {frequency, start, duration} form as NOTES0
// (start/duration in seconds within this track's loop). The shared A2
// tonic-pedal pad plays under every track; keep each track's loop length a
// whole number of seconds so that pedal stays seamless.
static const struct Note NOTES1[] = {
    // RIGHT HAND (melody + one harmony tone per chord beat)
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
    {GS4, 0.0f, 0.45f},
    {E4, 2.0f, 0.45f},
    {GS4, 4.0f, 0.7f},
    {A4, 6.0f, 0.45f},
    {F4, 8.25f, 0.45f},
    {C5, 10.0f, 0.7f},
    {GS4, 12.0f, 0.7f},
    {A4, 14.0f, 0.45f},
    {E5, 16.0f, 0.45f},
    {B4, 16.5f, 0.2f},
    {C5, 16.75f, 0.2f},
    {D5, 17.0f, 0.45f},
    {C5, 17.5f, 0.2f},
    {B4, 17.75f, 0.2f},
    {A4, 18.0f, 0.45f},
    {A4, 18.5f, 0.2f},
    {C5, 18.75f, 0.2f},
    {E5, 19.0f, 0.45f},
    {D5, 19.5f, 0.2f},
    {C5, 19.75f, 0.2f},
    {B4, 20.0f, 0.7f},
    {C5, 20.75f, 0.2f},
    {D5, 21.0f, 0.45f},
    {E5, 21.5f, 0.45f},
    {C5, 22.0f, 0.45f},
    {A4, 22.5f, 0.45f},
    {A4, 23.0f, 0.45f},
    {D5, 24.25f, 0.45f},
    {F5, 24.75f, 0.2f},
    {A5, 25.0f, 0.45f},
    {G5, 25.5f, 0.2f},
    {F5, 25.75f, 0.2f},
    {E5, 26.0f, 0.7f},
    {C5, 26.75f, 0.2f},
    {E5, 27.0f, 0.45f},
    {D5, 27.5f, 0.2f},
    {C5, 27.75f, 0.2f},
    {B4, 28.0f, 0.7f},
    {C5, 28.75f, 0.2f},
    {D5, 29.0f, 0.45f},
    {E5, 29.5f, 0.45f},
    {C5, 30.0f, 0.45f},
    {A4, 30.5f, 0.45f},
    {A4, 31.0f, 0.45f},
    {GS4, 16.0f, 0.45f},
    {E4, 18.0f, 0.45f},
    {GS4, 20.0f, 0.7f},
    {A4, 22.0f, 0.45f},
    {F4, 24.25f, 0.45f},
    {C5, 26.0f, 0.7f},
    {GS4, 28.0f, 0.7f},
    {A4, 30.0f, 0.45f},
    {E5, 32.0f, 0.95f},
    {C5, 33.0f, 0.95f},
    {D5, 34.0f, 0.95f},
    {B4, 35.0f, 0.95f},
    {C5, 36.0f, 0.95f},
    {A4, 37.0f, 0.95f},
    {GS4, 38.0f, 1.95f},
    {E5, 40.0f, 0.95f},
    {C5, 41.0f, 0.95f},
    {D5, 42.0f, 0.95f},
    {B4, 43.0f, 0.95f},
    {C5, 44.0f, 0.45f},
    {E5, 44.5f, 0.45f},
    {A5, 45.0f, 0.45f},
    {A5, 45.5f, 0.45f},
    {GS5, 46.0f, 1.95f},
    {GS4, 32.0f, 0.95f},
    {A4, 33.0f, 0.95f},
    {B4, 34.0f, 0.95f},
    {GS4, 35.0f, 0.95f},
    {A4, 36.0f, 0.95f},
    {E4, 37.0f, 0.95f},
    {E4, 38.0f, 1.95f},
    {GS4, 40.0f, 0.95f},
    {A4, 41.0f, 0.95f},
    {B4, 42.0f, 0.95f},
    {GS4, 43.0f, 0.95f},
    {E5, 46.0f, 1.95f},
    {E5, 48.0f, 0.45f},
    {B4, 48.5f, 0.2f},
    {C5, 48.75f, 0.2f},
    {D5, 49.0f, 0.45f},
    {C5, 49.5f, 0.2f},
    {B4, 49.75f, 0.2f},
    {A4, 50.0f, 0.45f},
    {A4, 50.5f, 0.2f},
    {C5, 50.75f, 0.2f},
    {E5, 51.0f, 0.45f},
    {D5, 51.5f, 0.2f},
    {C5, 51.75f, 0.2f},
    {B4, 52.0f, 0.7f},
    {C5, 52.75f, 0.2f},
    {D5, 53.0f, 0.45f},
    {E5, 53.5f, 0.45f},
    {C5, 54.0f, 0.45f},
    {A4, 54.5f, 0.45f},
    {A4, 55.0f, 0.45f},
    {D5, 56.25f, 0.45f},
    {F5, 56.75f, 0.2f},
    {A5, 57.0f, 0.45f},
    {G5, 57.5f, 0.2f},
    {F5, 57.75f, 0.2f},
    {E5, 58.0f, 0.7f},
    {C5, 58.75f, 0.2f},
    {E5, 59.0f, 0.45f},
    {D5, 59.5f, 0.2f},
    {C5, 59.75f, 0.2f},
    {B4, 60.0f, 0.7f},
    {C5, 60.75f, 0.2f},
    {D5, 61.0f, 0.45f},
    {E5, 61.5f, 0.45f},
    {C5, 62.0f, 0.45f},
    {A4, 62.5f, 0.45f},
    {A4, 63.0f, 0.45f},
    {GS4, 48.0f, 0.45f},
    {E4, 50.0f, 0.45f},
    {GS4, 52.0f, 0.7f},
    {A4, 54.0f, 0.45f},
    {F4, 56.25f, 0.45f},
    {C5, 58.0f, 0.7f},
    {GS4, 60.0f, 0.7f},
    {A4, 62.0f, 0.45f},
    // LEFT HAND (oom-pah: low/high octave eighths on the chord bass)
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
    {E2, 16.0f, 0.25f},
    {E3, 16.25f, 0.25f},
    {E2, 16.5f, 0.25f},
    {E3, 16.75f, 0.25f},
    {E2, 17.0f, 0.25f},
    {E3, 17.25f, 0.25f},
    {E2, 17.5f, 0.25f},
    {E3, 17.75f, 0.25f},
    {A2, 18.0f, 0.25f},
    {A3, 18.25f, 0.25f},
    {A2, 18.5f, 0.25f},
    {A3, 18.75f, 0.25f},
    {A2, 19.0f, 0.25f},
    {A3, 19.25f, 0.25f},
    {A2, 19.5f, 0.25f},
    {A3, 19.75f, 0.25f},
    {B2, 20.0f, 0.25f},
    {B3, 20.25f, 0.25f},
    {B2, 20.5f, 0.25f},
    {B3, 20.75f, 0.25f},
    {B2, 21.0f, 0.25f},
    {B3, 21.25f, 0.25f},
    {B2, 21.5f, 0.25f},
    {B3, 21.75f, 0.25f},
    {A2, 22.0f, 0.25f},
    {A3, 22.25f, 0.25f},
    {A2, 22.5f, 0.25f},
    {A3, 22.75f, 0.25f},
    {A2, 23.0f, 0.25f},
    {A3, 23.25f, 0.25f},
    {A2, 23.5f, 0.25f},
    {A3, 23.75f, 0.25f},
    {D3, 24.0f, 0.25f},
    {D4, 24.25f, 0.25f},
    {D3, 24.5f, 0.25f},
    {D4, 24.75f, 0.25f},
    {D3, 25.0f, 0.25f},
    {D4, 25.25f, 0.25f},
    {D3, 25.5f, 0.25f},
    {D4, 25.75f, 0.25f},
    {C3, 26.0f, 0.25f},
    {C4, 26.25f, 0.25f},
    {C3, 26.5f, 0.25f},
    {C4, 26.75f, 0.25f},
    {C3, 27.0f, 0.25f},
    {C4, 27.25f, 0.25f},
    {C3, 27.5f, 0.25f},
    {C4, 27.75f, 0.25f},
    {B2, 28.0f, 0.25f},
    {B3, 28.25f, 0.25f},
    {B2, 28.5f, 0.25f},
    {B3, 28.75f, 0.25f},
    {B2, 29.0f, 0.25f},
    {B3, 29.25f, 0.25f},
    {B2, 29.5f, 0.25f},
    {B3, 29.75f, 0.25f},
    {A2, 30.0f, 0.25f},
    {A3, 30.25f, 0.25f},
    {A2, 30.5f, 0.25f},
    {A3, 30.75f, 0.25f},
    {A2, 31.0f, 0.25f},
    {A3, 31.25f, 0.25f},
    {A2, 31.5f, 0.25f},
    {A3, 31.75f, 0.25f},
    {A2, 32.0f, 0.25f},
    {A3, 32.25f, 0.25f},
    {A2, 32.5f, 0.25f},
    {A3, 32.75f, 0.25f},
    {A2, 33.0f, 0.25f},
    {A3, 33.25f, 0.25f},
    {A2, 33.5f, 0.25f},
    {A3, 33.75f, 0.25f},
    {GS2, 34.0f, 0.25f},
    {GS3, 34.25f, 0.25f},
    {GS2, 34.5f, 0.25f},
    {GS3, 34.75f, 0.25f},
    {GS2, 35.0f, 0.25f},
    {GS3, 35.25f, 0.25f},
    {GS2, 35.5f, 0.25f},
    {GS3, 35.75f, 0.25f},
    {A2, 36.0f, 0.25f},
    {A3, 36.25f, 0.25f},
    {A2, 36.5f, 0.25f},
    {A3, 36.75f, 0.25f},
    {A2, 37.0f, 0.25f},
    {A3, 37.25f, 0.25f},
    {A2, 37.5f, 0.25f},
    {A3, 37.75f, 0.25f},
    {B2, 38.0f, 0.25f},
    {B3, 38.25f, 0.25f},
    {B2, 38.5f, 0.25f},
    {B3, 38.75f, 0.25f},
    {B2, 39.0f, 0.25f},
    {B3, 39.25f, 0.25f},
    {B2, 39.5f, 0.25f},
    {B3, 39.75f, 0.25f},
    {A2, 40.0f, 0.25f},
    {A3, 40.25f, 0.25f},
    {A2, 40.5f, 0.25f},
    {A3, 40.75f, 0.25f},
    {A2, 41.0f, 0.25f},
    {A3, 41.25f, 0.25f},
    {A2, 41.5f, 0.25f},
    {A3, 41.75f, 0.25f},
    {B2, 42.0f, 0.25f},
    {B3, 42.25f, 0.25f},
    {B2, 42.5f, 0.25f},
    {B3, 42.75f, 0.25f},
    {B2, 43.0f, 0.25f},
    {B3, 43.25f, 0.25f},
    {B2, 43.5f, 0.25f},
    {B3, 43.75f, 0.25f},
    {C3, 44.0f, 0.25f},
    {C4, 44.25f, 0.25f},
    {C3, 44.5f, 0.25f},
    {C4, 44.75f, 0.25f},
    {C3, 45.0f, 0.25f},
    {C4, 45.25f, 0.25f},
    {C3, 45.5f, 0.25f},
    {C4, 45.75f, 0.25f},
    {B2, 46.0f, 0.25f},
    {B3, 46.25f, 0.25f},
    {B2, 46.5f, 0.25f},
    {B3, 46.75f, 0.25f},
    {B2, 47.0f, 0.25f},
    {B3, 47.25f, 0.25f},
    {B2, 47.5f, 0.25f},
    {B3, 47.75f, 0.25f},
    {E2, 48.0f, 0.25f},
    {E3, 48.25f, 0.25f},
    {E2, 48.5f, 0.25f},
    {E3, 48.75f, 0.25f},
    {E2, 49.0f, 0.25f},
    {E3, 49.25f, 0.25f},
    {E2, 49.5f, 0.25f},
    {E3, 49.75f, 0.25f},
    {A2, 50.0f, 0.25f},
    {A3, 50.25f, 0.25f},
    {A2, 50.5f, 0.25f},
    {A3, 50.75f, 0.25f},
    {A2, 51.0f, 0.25f},
    {A3, 51.25f, 0.25f},
    {A2, 51.5f, 0.25f},
    {A3, 51.75f, 0.25f},
    {B2, 52.0f, 0.25f},
    {B3, 52.25f, 0.25f},
    {B2, 52.5f, 0.25f},
    {B3, 52.75f, 0.25f},
    {B2, 53.0f, 0.25f},
    {B3, 53.25f, 0.25f},
    {B2, 53.5f, 0.25f},
    {B3, 53.75f, 0.25f},
    {A2, 54.0f, 0.25f},
    {A3, 54.25f, 0.25f},
    {A2, 54.5f, 0.25f},
    {A3, 54.75f, 0.25f},
    {A2, 55.0f, 0.25f},
    {A3, 55.25f, 0.25f},
    {A2, 55.5f, 0.25f},
    {A3, 55.75f, 0.25f},
    {D3, 56.0f, 0.25f},
    {D4, 56.25f, 0.25f},
    {D3, 56.5f, 0.25f},
    {D4, 56.75f, 0.25f},
    {D3, 57.0f, 0.25f},
    {D4, 57.25f, 0.25f},
    {D3, 57.5f, 0.25f},
    {D4, 57.75f, 0.25f},
    {C3, 58.0f, 0.25f},
    {C4, 58.25f, 0.25f},
    {C3, 58.5f, 0.25f},
    {C4, 58.75f, 0.25f},
    {C3, 59.0f, 0.25f},
    {C4, 59.25f, 0.25f},
    {C3, 59.5f, 0.25f},
    {C4, 59.75f, 0.25f},
    {B2, 60.0f, 0.25f},
    {B3, 60.25f, 0.25f},
    {B2, 60.5f, 0.25f},
    {B3, 60.75f, 0.25f},
    {B2, 61.0f, 0.25f},
    {B3, 61.25f, 0.25f},
    {B2, 61.5f, 0.25f},
    {B3, 61.75f, 0.25f},
    {A2, 62.0f, 0.25f},
    {A3, 62.25f, 0.25f},
    {A2, 62.5f, 0.25f},
    {A3, 62.75f, 0.25f},
    {A2, 63.0f, 0.25f},
    {A3, 63.25f, 0.25f},
    {A2, 63.5f, 0.25f},
    {A3, 63.75f, 0.25f},
};

// Track 2 — Menu/Credits: warm, inviting sustained theme (A mixolydian, 32s).
static const struct Note NOTES_MENU[] = {
    {E5, 0.0f, 0.95f},
    {A5, 1.0f, 0.95f},
    {E5, 2.0f, 0.95f},
    {CS5, 3.0f, 0.95f},
    {FS5, 4.0f, 0.95f},
    {D5, 5.0f, 0.95f},
    {A5, 6.0f, 0.95f},
    {FS5, 7.0f, 0.95f},
    {G5, 8.0f, 0.95f},
    {B4, 9.0f, 0.95f},
    {D5, 10.0f, 0.95f},
    {G5, 11.0f, 0.95f},
    {FS5, 12.0f, 0.95f},
    {A5, 13.0f, 0.95f},
    {GS5, 14.0f, 0.95f},
    {E5, 15.0f, 0.95f},
    {A5, 16.0f, 0.95f},
    {E5, 17.0f, 0.95f},
    {CS5, 18.0f, 0.95f},
    {E5, 19.0f, 0.95f},
    {D5, 20.0f, 0.95f},
    {FS5, 21.0f, 0.95f},
    {A5, 22.0f, 0.95f},
    {FS5, 23.0f, 0.95f},
    {G5, 24.0f, 0.95f},
    {D5, 25.0f, 0.95f},
    {FS5, 26.0f, 0.95f},
    {A5, 27.0f, 0.95f},
    {E5, 28.0f, 0.95f},
    {GS5, 29.0f, 0.95f},
    {A5, 30.0f, 1.95f},
    {A2, 0.0f, 0.5f},
    {A3, 0.5f, 0.5f},
    {A2, 1.0f, 0.5f},
    {A3, 1.5f, 0.5f},
    {A2, 2.0f, 0.5f},
    {A3, 2.5f, 0.5f},
    {A2, 3.0f, 0.5f},
    {A3, 3.5f, 0.5f},
    {D2, 4.0f, 0.5f},
    {D3, 4.5f, 0.5f},
    {D2, 5.0f, 0.5f},
    {D3, 5.5f, 0.5f},
    {D2, 6.0f, 0.5f},
    {D3, 6.5f, 0.5f},
    {D2, 7.0f, 0.5f},
    {D3, 7.5f, 0.5f},
    {G2, 8.0f, 0.5f},
    {G3, 8.5f, 0.5f},
    {G2, 9.0f, 0.5f},
    {G3, 9.5f, 0.5f},
    {G2, 10.0f, 0.5f},
    {G3, 10.5f, 0.5f},
    {G2, 11.0f, 0.5f},
    {G3, 11.5f, 0.5f},
    {D2, 12.0f, 0.5f},
    {D3, 12.5f, 0.5f},
    {D2, 13.0f, 0.5f},
    {D3, 13.5f, 0.5f},
    {E2, 14.0f, 0.5f},
    {E3, 14.5f, 0.5f},
    {E2, 15.0f, 0.5f},
    {E3, 15.5f, 0.5f},
    {A2, 16.0f, 0.5f},
    {A3, 16.5f, 0.5f},
    {A2, 17.0f, 0.5f},
    {A3, 17.5f, 0.5f},
    {A2, 18.0f, 0.5f},
    {A3, 18.5f, 0.5f},
    {A2, 19.0f, 0.5f},
    {A3, 19.5f, 0.5f},
    {D2, 20.0f, 0.5f},
    {D3, 20.5f, 0.5f},
    {D2, 21.0f, 0.5f},
    {D3, 21.5f, 0.5f},
    {D2, 22.0f, 0.5f},
    {D3, 22.5f, 0.5f},
    {D2, 23.0f, 0.5f},
    {D3, 23.5f, 0.5f},
    {G2, 24.0f, 0.5f},
    {G3, 24.5f, 0.5f},
    {G2, 25.0f, 0.5f},
    {G3, 25.5f, 0.5f},
    {D2, 26.0f, 0.5f},
    {D3, 26.5f, 0.5f},
    {D2, 27.0f, 0.5f},
    {D3, 27.5f, 0.5f},
    {E2, 28.0f, 0.5f},
    {E3, 28.5f, 0.5f},
    {E2, 29.0f, 0.5f},
    {E3, 29.5f, 0.5f},
    {A2, 30.0f, 0.5f},
    {A3, 30.5f, 0.5f},
    {A2, 31.0f, 0.5f},
    {A3, 31.5f, 0.5f},
};

// Track 3 — Game over: somber, sparse A-minor loop (8s).
static const struct Note NOTES_GAMEOVER[] = {
    {E5, 0.0f, 0.95f},
    {D5, 1.0f, 0.95f},
    {C5, 2.0f, 1.4f},
    {A4, 3.5f, 0.45f},
    {E5, 4.0f, 0.95f},
    {C5, 5.0f, 0.95f},
    {GS4, 6.0f, 1.95f},
    {A3, 0.0f, 1.9f},
    {C4, 0.0f, 1.9f},
    {F3, 2.0f, 1.9f},
    {A3, 2.0f, 1.9f},
    {C3, 4.0f, 1.9f},
    {E4, 4.0f, 1.9f},
    {E3, 6.0f, 1.9f},
    {GS3, 6.0f, 1.9f},
};

// A track is its note list plus its own loop length; the A2 pedal underlies
// every track and stays seamless for any whole-second `duration`.
struct Track {
  const struct Note *notes;
  int count;
  float duration; // loop length (s); per-track so a longer arrangement fits
};
#define TRACK(a, dur) {(a), (int)(sizeof(a) / sizeof((a)[0])), (dur)}
// TRACK_NATURE/ARCADE/MENU/GAMEOVER live in sound.h (main.c selects by state).
// Each track's duration must give the A2 pedal whole cycles (110Hz * seconds
// integer) for a seamless seam — 48/64/32/8s all qualify.
static const struct Track TRACKS[] = {
    [TRACK_NATURE] = TRACK(NOTES0, 32.0f),
    [TRACK_ARCADE] = TRACK(NOTES1, 64.0f),
    [TRACK_MENU] = TRACK(NOTES_MENU, 32.0f),
    [TRACK_GAMEOVER] = TRACK(NOTES_GAMEOVER, 8.0f),
};
#define TRACK_COUNT ((int)(sizeof(TRACKS) / sizeof(TRACKS[0])))

// One precomputed loop buffer per track. Playback crossfades between the
// currently-selected track and the one we're fading out of.
static float *loopBuffer[TRACK_COUNT];
static int loopSamples[TRACK_COUNT]; // per-track loop length in samples
static int loopPos[TRACK_COUNT];     // per-track playback position
static int curTrack = 0;             // track fading in / currently playing
static int prevTrack = 0;            // track fading out
static float fade = 1.0f; // 0..1 weight of curTrack (1 = crossfade complete)
// Crossfade duration in samples (~2s).
#define FADE_SAMPLES (2 * SAMPLE_RATE)

// Independent channel levels (0..1); settings.c pushes these via setVolumes.
static float musicVol = 1.0f, sfxVol = 1.0f;
void setVolumes(float music, float sfx) {
  musicVol = music;
  sfxVol = sfx;
}

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
  int samples = (int)(TRACKS[track].duration * SAMPLE_RATE);
  loopSamples[track] = samples;

  loopBuffer[track] = malloc(samples * sizeof(float));
  float *buf = loopBuffer[track];
  if (buf == NULL) {
    return; // out of memory: leave the slot NULL; updateAudio plays it silent
  }
  float peak = 0.0001f;
  for (int i = 0; i < samples; i++) {
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
    // A2=110Hz over an integer number of seconds is whole cycles, so the loop
    // seam stays seamless for any track (48s -> 5280, 64s -> 7040).
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
  for (int i = 0; i < samples; i++) {
    buf[i] *= gain;
  }
}

// Level tracks (nature/arcade) crossfade into each other as the theme morphs
// mid-climb; menu and game over are separate screens, so switching to/from them
// is a hard cut — no music bleeds across a UI boundary.
static bool isLevelTrack(int t) {
  return t == TRACK_NATURE || t == TRACK_ARCADE;
}

// Switch to `track` (no-op if already selected). Crossfades over FADE_SAMPLES
// only between two level tracks; any switch involving menu/game over cuts
// instantly. SFX pick up the new track's variant immediately.
void setTrack(int track) {
  if (track < 0 || track >= TRACK_COUNT || track == curTrack) {
    return;
  }
  // ponytail: single 2-track crossfade — a rapid re-switch mid-fade snaps the
  // outgoing track; add a proper fade stack only if that's ever audible.
  prevTrack = curTrack;
  curTrack = track;
  fade = (isLevelTrack(track) && isLevelTrack(prevTrack)) ? 0.0f : 1.0f;
}

// --- Procedural sound effects ------------------------------------------------
// A small pool of voices synthesized on the fly and summed into the music
// stream. No sample files: every effect is math over its sample index.

// Effect lengths in samples (SfxId lives in sound.h).
static const int sfxLen[] = {
    [SFX_COIN] = (int)(0.12f * SAMPLE_RATE),
    [SFX_BOUNCE] = (int)(0.15f * SAMPLE_RATE),
    [SFX_POWERUP] = (int)(0.35f * SAMPLE_RATE),
    [SFX_HIT] = (int)(0.40f * SAMPLE_RATE),
    [SFX_SELECT] = (int)(0.08f * SAMPLE_RATE),
    [SFX_LEVELUP] = (int)(0.70f * SAMPLE_RATE),
    [SFX_GAMEOVER] = (int)(0.50f * SAMPLE_RATE),
    [SFX_SPLAT] = (int)(0.18f * SAMPLE_RATE),
    [SFX_PICKUP] = (int)(0.22f * SAMPLE_RATE),
    [SFX_NEWBEST] = (int)(1.10f * SAMPLE_RATE),
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

void sfx(SfxId id) {
  for (int i = 0; i < MAX_VOICES; i++) {
    if (!voices[i].active) {
      voices[i] = (struct Voice){true, id, 0};
      return;
    }
  }
}

static float square(float freq, float t) {
  return fmodf(freq * t, 1.0f) < 0.5f ? 1.0f : -1.0f;
}

// Linear frequency sweep: base at pos=0, falling by `depth` over the effect.
static float sweep(float base, float depth, int pos, int len) {
  return base - depth * (float)pos / len;
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
  bool arc = (curTrack == TRACK_ARCADE);
  switch (type) {
  case SFX_COIN: // rising two-note blip (brighter on arcade)
    s = square(pos < len / 2 ? (arc ? D6 : B5) : (arc ? G6 : E6), t);
    break;
  case SFX_BOUNCE: { // downward pitch sweep "boing"
    float f = sweep(400.0f, 280.0f, pos, len);
    s = arc ? square(f, t) : sinf(2.0f * PI * f * t); // square = crunchier
    break;
  }
  case SFX_POWERUP: { // rising arpeggio
    static const float natNotes[3] = {C5, E5, G5};
    static const float arcNotes[3] = {E5, B5, E6};
    const float *notes = arc ? arcNotes : natNotes;
    int k = pos * 3 / len;
    s = square(notes[k > 2 ? 2 : k], t);
    break;
  }
  case SFX_HIT: { // descending saw + noise (noisier on arcade)
    float f = sweep(300.0f, 240.0f, pos, len);
    float saw = 2.0f * (f * t - floorf(0.5f + f * t));
    float noise = (float)rand() / (float)RAND_MAX * 2.0f - 1.0f;
    s = arc ? 0.4f * saw + 0.6f * noise : 0.7f * saw + 0.3f * noise;
    break;
  }
  case SFX_SELECT:
    s = square(arc ? 880.0f : 660.0f, t);
    break;
  case SFX_LEVELUP: { // triumphant rising fanfare, full major
    static const float notes[4] = {C5, E5, G5, C6};
    int k = pos * 4 / len;
    s = square(notes[k > 3 ? 3 : k], t);
    break;
  }
  case SFX_GAMEOVER: { // sad descending sweep
    float f = sweep(400.0f, 320.0f, pos, len);
    s = arc ? square(f, t) : sinf(2.0f * PI * f * t);
    break;
  }
  case SFX_SPLAT: { // short wet squish: low saw + noise
    float f = sweep(180.0f, 120.0f, pos, len);
    float saw = 2.0f * (f * t - floorf(0.5f + f * t));
    float noise = (float)rand() / (float)RAND_MAX * 2.0f - 1.0f;
    s = 0.5f * saw + 0.5f * noise;
    break;
  }
  case SFX_PICKUP: { // brighter/higher arpeggio (Balloon/Shield)
    static const float natNotes[3] = {G5, C6, E6};
    static const float arcNotes[3] = {B5, E6, B6};
    const float *notes = arc ? arcNotes : natNotes;
    int k = pos * 3 / len;
    s = square(notes[k > 2 ? 2 : k], t);
    break;
  }
  case SFX_NEWBEST: { // grand ascending run holding on the top note
    static const float natNotes[6] = {C5, E5, G5, C6, E6, G6};
    static const float arcNotes[6] = {E5, G5, B5, E6, G6, B6};
    const float *notes = arc ? arcNotes : natNotes;
    int k = pos * 6 / len;      // step up over the first 5/6...
    if (k > 5) {
      k = 5; // ...then hold the top note for the last sixth (the fanfare peak)
    }
    // Layer a fifth above for a fuller, brighter chord than the plain level-up.
    s = 0.6f * square(notes[k], t) + 0.4f * square(notes[k] * 1.5f, t);
    break;
  }
  }
  return 0.28f * env * s; // master SFX gain (music is unaffected)
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
    // A track whose loop failed to allocate is NULL — treat it as silence.
    float cur =
        loopBuffer[curTrack] ? loopBuffer[curTrack][loopPos[curTrack]] : 0.0f;
    float prev = loopBuffer[prevTrack]
                     ? loopBuffer[prevTrack][loopPos[prevTrack]]
                     : 0.0f;
    float music = fade * cur + (1.0f - fade) * prev;
    loopPos[curTrack] = (loopPos[curTrack] + 1) % loopSamples[curTrack];
    if (prevTrack != curTrack) {
      loopPos[prevTrack] = (loopPos[prevTrack] + 1) % loopSamples[prevTrack];
    }
    if (fade < 1.0f) {
      fade += 1.0f / FADE_SAMPLES;
      if (fade > 1.0f) {
        fade = 1.0f;
      }
    }
    float s = 0.85f * music * musicVol; // headroom for SFX on top

    float e = 0.0f; // SFX summed separately so its level scales independently
    for (int v = 0; v < MAX_VOICES; v++) {
      if (!voices[v].active) {
        continue;
      }
      e += renderVoice(voices[v].type, voices[v].pos);
      if (++voices[v].pos >= sfxLen[voices[v].type]) {
        voices[v].active = false;
      }
    }

    buffer[i] = tanhf(s + e * sfxVol); // soft limiter — warmer than a hard clamp
  }
  UpdateAudioStream(*stream, buffer, BUFFER_SIZE);
}

void deinitAudio(AudioStream *stream) {
  UnloadAudioStream(*stream);
  CloseAudioDevice();

  for (int t = 0; t < TRACK_COUNT; t++) {
    free(loopBuffer[t]);
    loopBuffer[t] = NULL;
    loopPos[t] = 0;
  }
}
