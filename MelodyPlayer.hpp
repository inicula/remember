#pragma once
#include "notes.hpp"
#include <Arduino.h>

struct Note {
    u16 freq;
    u32 slice;
};

using Melody = const Note*;

template <typename T> static u32 getTotalSlices(const T& notes)
{
    u32 sum = 0;
    for (const auto& note : notes)
        sum += note.slice;
    return sum;
}

class MelodyPlayer {
public:
    template <typename T>
    constexpr MelodyPlayer(const T& notes, u16 totalDuration)
        : mel(&notes[0])
        , numNotes(sizeof(notes) / sizeof(notes[0]))
        , msPerSlice(totalDuration / getTotalSlices(notes))
        , i(numNotes)
        , past(0)
    {
    }

    void init() { pinMode(BUZZER_PIN, OUTPUT); }

    void play()
    {
        const auto currentTs = millis();

        if (i == numNotes) {
            i = 0;
            past = currentTs;
        }

        const auto note = mel[i].freq;
        if (note)
            tone(BUZZER_PIN, note);
        else
            noTone(BUZZER_PIN);

        if (currentTs - past > mel[i].slice * msPerSlice) {
            past = currentTs;
            ++i;
        }
    }

    void stop() { noTone(BUZZER_PIN); }

    static constexpr u8 BUZZER_PIN = 3;

private:
    Melody mel;
    u16 numNotes;
    u32 msPerSlice;
    u16 i;
    u32 past;
};

static constexpr Note CONTRAPUNCTUS_1[] = {
    { NOTE_D5, 1 },
    { 0, 3 },
    { NOTE_A5, 1 },
    { 0, 3 },
    { NOTE_F5, 1 },
    { 0, 3 },
    { NOTE_D5, 1 },
    { 0, 3 },
    { NOTE_CS5, 1 },
    { 0, 3 },
    { NOTE_D5, 1 },
    { 0, 1 },
    { NOTE_E5, 1 },
    { 0, 1 },
    { NOTE_F5, 1 },
    { 0, 4 },
    { NOTE_G5, 1 },
    { NOTE_F5, 1 },
    { NOTE_E5, 1 },
    { NOTE_D5, 1 },
    { 0, 1 },
    { NOTE_E5, 1 },
    { 0, 1 },
    { NOTE_F5, 1 },
    { 0, 1 },
    { NOTE_G5, 1 },
    { 0, 1 },
    { NOTE_A5, 1 },
    { 0, 1 },
    { NOTE_A4, 1 },
    { NOTE_B4, 1 },
    { NOTE_C5, 1 },
    { NOTE_A4, 1 },
    { NOTE_F5, 1 },
    { 0, 2 },
    { NOTE_B4, 1 },
    { NOTE_E5, 1 },
    { 0, 2 },
    { NOTE_F5, 1 },
    { NOTE_E5, 1 },
    { NOTE_D5, 1 },
    { NOTE_E5, 1 },
    { 0, 2 },
};
