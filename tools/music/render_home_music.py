#!/usr/bin/env python3
"""Renders the home screen's three original chiptune loops to WAV files.

    render_home_music.py OUTPUT_DIRECTORY

Each loop is 8 bars of eighth notes: a square or triangle lead, a triangle bass on every beat and a
soft sine arpeggio, the same scores as the home screen mockup. The files loop seamlessly: every
note's tail that runs past the end is folded back onto the start.
"""

import math
import struct
import sys
import wave
from pathlib import Path

RATE = 22050

NOTE = {
    "A2": 110.0, "Bb2": 116.54, "B2": 123.47, "C3": 130.81, "D3": 146.83, "E3": 164.81,
    "F3": 174.61, "G2": 98.0, "G3": 196.0, "C4": 261.63, "D4": 293.66, "E4": 329.63,
    "F4": 349.23, "F#4": 369.99, "G4": 392.0, "A4": 440.0, "Bb4": 466.16, "B4": 493.88,
    "C5": 523.25, "D5": 587.33, "E5": 659.25, "F5": 698.46, "F#5": 739.99, "G5": 783.99,
    "A5": 880.0, "Bb5": 932.33, "B5": 987.77, "C6": 1046.5, "D6": 1174.66, "E6": 1318.51,
    "F6": 1396.91, "G6": 1567.98,
}

SONGS = [
    {
        "name": "feuille-d-erable", "bpm": 112, "wave": "square",
        "lead": ["E5 G5 C6 G5 E5 - D5 E5", "C5 E5 A5 G5 E5 - - -", "F5 A5 C6 A5 G5 F5 E5 D5",
                 "D5 G5 B5 A5 G5 - - -", "E5 G5 C6 D6 E6 - C6 -", "B5 G5 E5 G5 B5 - A5 G5",
                 "A5 C6 F5 A5 B5 D6 G5 B5", "C6 G5 E5 G5 C6 - - -"],
        "bass": ["C3", "A2", "F3", "G3", "C3", "E3", "F3 G3", "C3"],
        "arp": [["C4", "E4", "G4"], ["A4", "C5", "E5"], ["F4", "A4", "C5"], ["G4", "B4", "D5"],
                ["C4", "E4", "G4"], ["E4", "G4", "B4"], ["F4", "A4", "C5"], ["C4", "E4", "G4"]],
    },
    {
        "name": "clairiere", "bpm": 92, "wave": "triangle",
        "lead": ["A5 - C6 A5 G5 - F5 -", "F5 A5 D6 C6 A5 - - -", "D6 - C6 Bb5 A5 - F5 G5",
                 "E5 G5 C6 Bb5 G5 - - -", "A5 C6 F6 E6 C6 - A5 -", "E5 A5 C6 B5 A5 - E5 -",
                 "F5 Bb5 D6 Bb5 E5 G5 C6 E6", "F6 C6 A5 C6 F5 - - -"],
        "bass": ["F3", "D3", "Bb2", "C3", "F3", "A2", "Bb2 C3", "F3"],
        "arp": [["F4", "A4", "C5"], ["D4", "F4", "A4"], ["Bb4", "D5", "F5"], ["C4", "E4", "G4"],
                ["F4", "A4", "C5"], ["A4", "C5", "E5"], ["Bb4", "D5", "F5"], ["F4", "A4", "C5"]],
    },
    {
        "name": "grand-prix", "bpm": 132, "wave": "square",
        "lead": ["G5 B5 D6 B5 A5 B5 G5 -", "E5 G5 B5 A5 G5 - E5 -", "C6 B5 A5 G5 E5 G5 C6 -",
                 "D6 C6 B5 A5 F#5 - D5 -", "G5 - B5 D6 G6 - D6 -", "D6 B5 F#5 A5 B5 - - -",
                 "E6 C6 G5 C6 D6 A5 F#5 A5", "G5 B5 D6 G6 G5 - - -"],
        "bass": ["G2", "E3", "C3", "D3", "G2", "B2", "C3 D3", "G2"],
        "arp": [["G4", "B4", "D5"], ["E4", "G4", "B4"], ["C4", "E4", "G4"], ["D4", "F#4", "A4"],
                ["G4", "B4", "D5"], ["B4", "D5", "F#5"], ["C4", "E4", "G4"], ["G4", "B4", "D5"]],
    },
]


def oscillator(kind, phase):
    phase %= 1.0
    if kind == "square":
        return 1.0 if phase < 0.5 else -1.0
    if kind == "triangle":
        return 4.0 * phase - 1.0 if phase < 0.5 else 3.0 - 4.0 * phase
    return math.sin(2 * math.pi * phase)


def add_note(buffer, kind, freq, start, length, gain):
    attack = int(0.01 * RATE)
    total = int(length * RATE)
    for i in range(total):
        # 10 ms attack, then an exponential fall to -80 dB at the note's end, as in the mockup
        envelope = i / attack if i < attack else math.exp(math.log(1e-4) * (i - attack) / max(1, total - attack))
        index = (start + i) % len(buffer)
        buffer[index] += gain * envelope * oscillator(kind, freq * i / RATE)


def render(song):
    eighth = 60 / song["bpm"] / 2
    steps = 64
    buffer = [0.0] * int(round(steps * eighth * RATE))
    lead = " ".join(song["lead"]).split()
    lead_gain = 0.07 if song["wave"] == "square" else 0.16
    for step in range(steps):
        bar, beat = divmod(step, 8)
        start = int(round(step * eighth * RATE))
        note = lead[step]
        if note != "-":
            hold = 1
            while lead[(step + hold) % steps] == "-" and hold < 4:
                hold += 1
            add_note(buffer, song["wave"], NOTE[note], start, eighth * hold * 0.95, lead_gain)
        bass = song["bass"][bar].split()
        if beat % 2 == 0:
            name = bass[1] if len(bass) > 1 and beat >= 4 else bass[0]
            add_note(buffer, "triangle", NOTE[name], start, eighth * 1.8, 0.16)
        add_note(buffer, "sine", NOTE[song["arp"][bar][beat % 3]] * 2, start, eighth * 0.6, 0.025)
    peak = max(abs(sample) for sample in buffer) or 1.0
    scale = 0.5 / peak  # quiet background level
    return [int(max(-1.0, min(1.0, sample * scale)) * 32767) for sample in buffer]


def main():
    out = Path(sys.argv[1])
    out.mkdir(parents=True, exist_ok=True)
    for number, song in enumerate(SONGS, 1):
        samples = render(song)
        path = out / f"home_{number}.wav"
        with wave.open(str(path), "wb") as file:
            file.setnchannels(1)
            file.setsampwidth(2)
            file.setframerate(RATE)
            file.writeframes(struct.pack(f"<{len(samples)}h", *samples))
        print(f"{path}: {song['name']}, {len(samples) / RATE:.1f} s")


if __name__ == "__main__":
    main()
