#include "models/NoteSpans.h"

#include <algorithm>

#include "audio/TrackerPlaybackEngine.h"
#include "models/TrackerDocument.h"

namespace note_spans {

std::vector<NoteSpan> from_channel(const TrackerDocument& doc, int ch) {
    std::vector<NoteSpan> spans;
    if (ch < 0 || ch >= TrackerDocument::kChannelCount) return spans;

    const int len = doc.length();
    int open = -1; // index of the span still sounding, -1 = silence

    auto close_at = [&](int row) {
        if (open >= 0) {
            spans[open].length = std::max(1, row - spans[open].start);
            open = -1;
        }
    };

    for (int row = 0; row < len; ++row) {
        const TrackerCell& c = doc.cell(ch, row);
        if (c.is_note_off()) {
            if (open >= 0) spans[open].ends_with_off = true;
            close_at(row);
            continue;
        }
        if (!c.is_note_on()) continue;

        const bool slide = (c.fx == 0x3) && open >= 0;
        close_at(row);

        NoteSpan s;
        s.start      = row;
        s.note       = c.note;
        s.instrument = c.instrument;
        s.attn       = c.attn;
        s.slide      = slide;
        s.has_fx     = c.has_fx();
        spans.push_back(s);
        open = static_cast<int>(spans.size()) - 1;
    }
    close_at(len);
    return spans;
}

int span_at(const std::vector<NoteSpan>& spans, int row) {
    for (size_t i = 0; i < spans.size(); ++i) {
        if (row >= spans[i].start && row < spans[i].start + spans[i].length) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

void place(TrackerDocument& doc, int ch, const NoteSpan& span) {
    const int len = doc.length();
    if (ch < 0 || ch >= TrackerDocument::kChannelCount) return;
    if (span.start < 0 || span.start >= len || span.note < 1 || span.note > 127) return;
    const int end = std::min(len, span.start + std::max(1, span.length));

    TrackerCell c = doc.cell(ch, span.start);
    c.note = span.note;
    c.instrument = span.instrument;
    c.attn = span.attn;
    doc.set_cell(ch, span.start, c);

    for (int row = span.start + 1; row < end; ++row) {
        c = doc.cell(ch, row);
        if (c.note == 0) continue;
        c.note = 0;
        c.instrument = 0;
        c.attn = 0xFF;
        doc.set_cell(ch, row, c);
    }

    if (end < len) {
        const TrackerCell& after = doc.cell(ch, end);
        if (!after.is_note_on() && !after.is_note_off()) doc.set_note(ch, end, 0xFF);
    }
}

void erase(TrackerDocument& doc, int ch, int start) {
    const std::vector<NoteSpan> spans = from_channel(doc, ch);
    int idx = -1;
    for (size_t i = 0; i < spans.size(); ++i) {
        if (spans[i].start == start) { idx = static_cast<int>(i); break; }
    }
    if (idx < 0) return;
    const NoteSpan& s = spans[static_cast<size_t>(idx)];

    // Was a note cut by this one? Then it must stop here, not run on.
    const bool prev_runs_into = idx > 0
        && spans[static_cast<size_t>(idx - 1)].start + spans[static_cast<size_t>(idx - 1)].length == start
        && !spans[static_cast<size_t>(idx - 1)].ends_with_off;

    TrackerCell c = doc.cell(ch, start);
    c.note = prev_runs_into ? 0xFF : 0;
    c.instrument = 0;
    c.attn = 0xFF;
    doc.set_cell(ch, start, c);

    // The note-off that closed this bar is no longer needed
    const int end = s.start + s.length;
    if (s.ends_with_off && end < doc.length() && doc.cell(ch, end).is_note_off()) {
        doc.set_note(ch, end, 0);
    }
}

void replace(TrackerDocument& doc, int ch,
             const std::vector<NoteSpan>& from, const std::vector<NoteSpan>& to) {
    // Erase from the last bar backwards: each erase only has to deal with
    // bars that are still in place before it.
    std::vector<int> starts;
    for (const NoteSpan& s : from) starts.push_back(s.start);
    std::sort(starts.begin(), starts.end());
    for (auto it = starts.rbegin(); it != starts.rend(); ++it) erase(doc, ch, *it);

    std::vector<NoteSpan> sorted = to;
    std::sort(sorted.begin(), sorted.end(),
              [](const NoteSpan& a, const NoteSpan& b) { return a.start < b.start; });
    for (const NoteSpan& s : sorted) place(doc, ch, s);
}

uint8_t lowest_tone_note() {
    static const uint8_t kLowest = []() {
        for (int n = 1; n <= 127; ++n) {
            if (TrackerPlaybackEngine::midi_to_divider(static_cast<uint8_t>(n)) < 1023) {
                return static_cast<uint8_t>(n);
            }
        }
        return static_cast<uint8_t>(1);
    }();
    return kLowest;
}

} // namespace note_spans
