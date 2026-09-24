#pragma once

#include <cstdint>
#include <vector>

class TrackerDocument;

// ============================================================
// NoteSpans — tracker cells seen as piano-roll bars.
//
// A note starts on its row and lasts until the next event on the same
// channel (another note, a note-off, a 3xx portamento target) or the end
// of the pattern. Each PSG voice is monophonic, so spans never overlap.
// ============================================================

struct NoteSpan {
    int     start      = 0;     // first row
    int     length     = 1;     // rows covered (>= 1)
    uint8_t note       = 0;     // tracker note id (1-127, C-0 = 1)
    uint8_t instrument = 0;
    uint8_t attn       = 0xFF;  // 0xFF = instrument default
    bool    ends_with_off = false; // closed by an explicit note-off (else next note / pattern end)
    bool    slide      = false; // started by 3xx: glides from the previous note, no retrigger
    bool    has_fx     = false; // start cell carries an effect
};

namespace note_spans {

// Build the bars of one channel of a pattern.
std::vector<NoteSpan> from_channel(const TrackerDocument& doc, int ch);

// Index of the bar covering `row`, -1 if the channel is silent there.
int span_at(const std::vector<NoteSpan>& spans, int row);

// Write a bar: note-on at its start, no other note or note-off inside it,
// and a note-off right after unless a note starts there. A bar that was
// sounding when it starts is cut; bars starting inside it are removed.
// Effects stay on their rows. The caller pushes the undo snapshot.
void place(TrackerDocument& doc, int ch, const NoteSpan& span);

// Remove the bar that starts on `start`. The previous note does not
// stretch over the gap: it gets a note-off where the removed bar began.
void erase(TrackerDocument& doc, int ch, int start);

// Erase the bars `from` (by their start rows), then place `to`. Used to
// move, transpose, cut or paste a group of bars in one edit.
void replace(TrackerDocument& doc, int ch,
             const std::vector<NoteSpan>& from, const std::vector<NoteSpan>& to);

// Lowest tracker note the tone channels play in tune. Below it the PSG
// divider saturates at 1023 and the pitch no longer follows the note.
uint8_t lowest_tone_note();

} // namespace note_spans
