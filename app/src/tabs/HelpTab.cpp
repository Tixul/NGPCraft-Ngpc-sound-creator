#include "tabs/HelpTab.h"

#include <QHBoxLayout>
#include <QListWidget>
#include <QTextBrowser>

#include "i18n/AppLanguage.h"

// ============================================================
// Construction
// ============================================================

HelpTab::HelpTab(QWidget* parent)
    : QWidget(parent)
{
    language_ = load_app_language();
    const auto ui = [this](const char* fr, const char* en) {
        return app_lang_pick(language_, fr, en);
    };

    auto* root = new QHBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // Left: topic list
    topic_list_ = new QListWidget(this);
    topic_list_->setFixedWidth(220);
    topic_list_->setStyleSheet(
        "QListWidget { background: #1e1e28; color: #ccccdd; font-size: 13px;"
        " border-right: 1px solid #333; }"
        "QListWidget::item { padding: 8px 12px; }"
        "QListWidget::item:selected { background: #3a3a55; color: white; }");

    topic_list_->addItem(ui("Bienvenue", "Welcome"));
    topic_list_->addItem(ui("Concepts de base", "Core concepts"));
    topic_list_->addItem(ui("Les Instruments", "Instruments"));
    topic_list_->addItem(ui("Tracker : les bases", "Tracker: basics"));
    topic_list_->addItem(ui("Tracker : le clavier", "Tracker: keyboard"));
    topic_list_->addItem(ui("Tracker : edition", "Tracker: editing"));
    topic_list_->addItem(ui("Tracker : avance", "Tracker: advanced"));
    topic_list_->addItem(ui("Tracker : effets", "Tracker: effects"));
    topic_list_->addItem(ui("Lecture / Playback", "Playback"));
    topic_list_->addItem(ui("Export C / Sauvegarde", "Export / Save"));
    topic_list_->addItem(ui("SFX Lab", "SFX Lab"));
    topic_list_->addItem(ui("Raccourcis clavier", "Keyboard shortcuts"));
    topic_list_->addItem(ui("FAQ / Problemes", "FAQ / Troubleshooting"));

    root->addWidget(topic_list_);

    // Right: content browser
    content_ = new QTextBrowser(this);
    content_->setOpenExternalLinks(false);
    content_->setStyleSheet(
        "QTextBrowser { background: #22222e; color: #dddddd; font-size: 13px;"
        " padding: 16px; border: none; }"
        "h1 { color: #88aaff; font-size: 20px; }"
        "h2 { color: #aaccff; font-size: 16px; margin-top: 18px; }"
        "h3 { color: #ccddff; font-size: 14px; margin-top: 12px; }"
        "code { background: #2a2a3a; color: #ffcc66; padding: 2px 4px; }"
        "pre { background: #1a1a26; color: #cccccc; padding: 8px; }"
        "table { border-collapse: collapse; margin: 8px 0; }"
        "td, th { border: 1px solid #444; padding: 4px 8px; }"
        "th { background: #2a2a3a; color: #aabbdd; }");
    root->addWidget(content_, 1);

    connect(topic_list_, &QListWidget::currentRowChanged, this, &HelpTab::load_topic);
    topic_list_->setCurrentRow(0);
}

void HelpTab::load_topic(int index) {
    if (language_ == AppLanguage::English) {
        content_->setHtml(topic_english(index));
        return;
    }

    switch (index) {
    case 0:  content_->setHtml(topic_intro()); break;
    case 1:  content_->setHtml(topic_concepts()); break;
    case 2:  content_->setHtml(topic_instruments()); break;
    case 3:  content_->setHtml(topic_tracker_basics()); break;
    case 4:  content_->setHtml(topic_tracker_keyboard()); break;
    case 5:  content_->setHtml(topic_tracker_edit()); break;
    case 6:  content_->setHtml(topic_tracker_advanced()); break;
    case 7:  content_->setHtml(topic_tracker_effects()); break;
    case 8:  content_->setHtml(topic_playback()); break;
    case 9:  content_->setHtml(topic_export()); break;
    case 10: content_->setHtml(topic_sfxlab()); break;
    case 11: content_->setHtml(topic_shortcuts()); break;
    case 12: content_->setHtml(topic_faq()); break;
    default: break;
    }
}

QString HelpTab::topic_english(int index) const {
    switch (index) {
    case 0:
        return QStringLiteral(R"(
<h1>Welcome to NGPC Sound Creator</h1>
<p>This tool lets you <b>compose music</b> for <b>Neo Geo Pocket / Color</b> using a tracker workflow:
rows are time steps, channels are independent audio lanes, and each row stores musical events.</p>

<h2>What kind of tool is this?</h2>
<p>Think of it as a musical spreadsheet for retro hardware. You edit note data directly,
preview with a runtime-faithful engine, then export assets for game integration.</p>

<h2>Main tabs</h2>
<table>
<tr><th>Tab</th><th>Purpose</th></tr>
<tr><td><b>Project</b></td><td>Create/open projects, manage songs/SFX, global export flow</td></tr>
<tr><td><b>Player</b></td><td>Load and audition generated/known audio data</td></tr>
<tr><td><b>Tracker</b></td><td>Main composition grid: notes, instrument ids, attenuation, FX</td></tr>
<tr><td><b>Instruments</b></td><td>Define envelopes, modulation, sweep, ADSR/LFO behavior</td></tr>
<tr><td><b>SFX Lab</b></td><td>Design and preview one-shot tone/noise effects</td></tr>
<tr><td><b>Debug</b></td><td>PSG/Z80 diagnostics and runtime comparisons</td></tr>
<tr><td><b>Help</b></td><td>This documentation</td></tr>
</table>

<h2>Current workflow highlights</h2>
<ul>
<li>Multi-pattern songs with order list and loop point</li>
<li>MIDI import directly into tracker patterns</li>
<li>WAV offline rendering for listening and validation</li>
<li>Hybrid export mode with runtime-oriented opcodes</li>
<li>Project-level export assets (songs/instruments/SFX)</li>
<li>Selection loop and advanced tracker editing tools</li>
</ul>

<h2>Start here</h2>
<ol>
<li>Create/open a project (or use free edit mode)</li>
<li>Prepare at least one instrument</li>
<li>Write patterns in Tracker</li>
<li>Arrange order list for full song structure</li>
<li>Export C/ASM for runtime, WAV for listening preview</li>
</ol>
<p>If you are completely new to trackers, read topics in order from top to bottom.</p>

<h2>Language and startup</h2>
<ul>
<li>Startup dialog lets you choose New project, Open project, or Free edit mode</li>
<li>Language (FR/EN) is selected in the same dialog</li>
<li>Language changes are applied after app restart</li>
</ul>

<h2>NGPC audio limits (and why this tool matters)</h2>
<p>The T6W28 chip provides 4 simultaneous channels:</p>
<ul>
<li><b>T0/T1/T2</b>: tonal channels</li>
<li><b>N</b>: dedicated noise channel</li>
</ul>
<p>Those constraints shape composition style. The tracker is designed to work directly inside them.</p>

<h2>Feature highlights (current generation)</h2>
<ul>
<li>Loop Selection playback for focused editing passes</li>
<li>Loop Preview in Instruments for fast tone iteration</li>
<li>Project mode + Free edit mode startup workflow</li>
<li>ADSR envelopes and pitch LFO per instrument</li>
<li>Host command <b>Exy</b> (global fade and tempo control)</li>
<li>Persistent <b>4xx</b> pitch bend and <b>Fxx</b> expression effects</li>
<li>Extended Hybrid opcode export (ADSR/LFO/expression/pitch/host cmd)</li>
<li>Project Export All with global song/instrument/SFX outputs</li>
<li>Project export manifest + centralized C API helper generation</li>
</ul>
)");

    case 1:
        return QStringLiteral(R"(
<h1>Core Concepts</h1>
<h2>Notes</h2>
<p>The tracker uses chromatic note names:</p>
<pre>C  C#  D  D#  E  F  F#  G  G#  A  A#  B</pre>
<p>For beginners: start with natural notes (C D E F G A B), then add sharps for transitions and color.</p>

<h2>Octaves</h2>
<p>The same note exists in different pitch ranges (for example C-3, C-4, C-5).</p>
<ul>
<li>Oct 2-3: bass/low range</li>
<li>Oct 4: default melodic range</li>
<li>Oct 5-7: lead/high range</li>
</ul>

<h2>Attenuation</h2>
<p>NGPC uses attenuation instead of volume. Values are hexadecimal:</p>
<table>
<tr><th>Value</th><th>Meaning</th></tr>
<tr><td><b>0</b></td><td>Loudest</td></tr>
<tr><td><b>4</b></td><td>Medium-high</td></tr>
<tr><td><b>8</b></td><td>Medium-low</td></tr>
<tr><td><b>C</b></td><td>Quiet</td></tr>
<tr><td><b>F</b></td><td>Silence</td></tr>
<tr><td><b>-</b></td><td>No explicit value (instrument/default behavior)</td></tr>
</table>

<h2>Channels</h2>
<ul>
<li><b>T0/T1/T2</b>: tonal channels for notes</li>
<li><b>N</b>: noise channel for drums/noise bursts</li>
</ul>

<h2>Timing</h2>
<p>The tracker advances row by row. Speed is controlled by <b>TPR</b> (ticks per row):</p>
<ul>
<li>Lower TPR = faster playback</li>
<li>Higher TPR = slower playback</li>
</ul>
<p>Rows are visually grouped (every 4 and 16 rows) to make beats and phrasing easier to read.</p>
<p>Hex reminder: tracker values often use <code>0..9, A..F</code>.</p>

<h2>Rhythm reference</h2>
<ul>
<li>Step 1: dense note entry</li>
<li>Step 2: alternating row spacing</li>
<li>Step 4: quarter-note style spacing in many patterns</li>
</ul>

<h2>Instrument concept in one minute</h2>
<p>An instrument is the timbre profile used by notes. The same note can sound like a lead, bass,
pluck, or effect depending on the instrument parameters.</p>
<ul>
<li>Instrument slots range from <b>00</b> to <b>7F</b> (up to 128 instruments)</li>
<li>Each instrument can define envelope, vibrato, sweep, ADSR and LFO behavior</li>
<li>Tracker cells reference instruments through the <b>In</b> field</li>
</ul>
)");

    case 2:
        return QStringLiteral(R"(
<h1>Instruments</h1>
<p>Instruments define the tone character of your notes and how the sound evolves over time.</p>

<h2>What an instrument can control</h2>
<ul>
<li>Base attenuation and envelope shape</li>
<li>Vibrato and LFO-style pitch modulation</li>
<li>Sweep behavior and pitch motion</li>
<li>ADSR mode (attack, decay, sustain, release)</li>
</ul>

<h2>UI structure</h2>
<p>Instrument tab is split into:</p>
<ul>
<li>Left: instrument list (00..7F)</li>
<li>Right: parameters for the selected instrument</li>
</ul>

<h2>Slot management</h2>
<ul>
<li><b>Tracker code</b> label shows current slot id (<code>0x00..0x7F</code>) used in tracker <b>In</b> column</li>
<li><b>Name + Apply Name</b>: rename selected slot without changing tracker code</li>
<li><b>Apply Factory -&gt; Slot</b>: copy selected factory preset into current slot (overwrite)</li>
<li><b>Reset Slot</b>: restore current slot default value</li>
<li><b>Reset Factory Bank</b>: restore full instrument bank to factory defaults</li>
</ul>

<h2>Essential starter settings</h2>
<table>
<tr><th>Parameter</th><th>Role</th><th>Safe default</th></tr>
<tr><td><b>Attenuation</b></td><td>Base loudness</td><td>0</td></tr>
<tr><td><b>Mode</b></td><td>Tone vs noise behavior</td><td>Tone</td></tr>
<tr><td><b>Envelope</b></td><td>How volume evolves</td><td>Medium decay</td></tr>
</table>

<h2>ADSR (when enabled)</h2>
<table>
<tr><th>Phase</th><th>Meaning</th></tr>
<tr><td><b>Attack</b></td><td>Rise from silence to full level</td></tr>
<tr><td><b>Decay</b></td><td>Drop from peak toward sustain level</td></tr>
<tr><td><b>Sustain</b></td><td>Held level while note remains active</td></tr>
<tr><td><b>Release</b></td><td>Fade out after note-off</td></tr>
</table>

<h2>Modulation notes</h2>
<ul>
<li>Vibrato depth controls intensity</li>
<li>Vibrato speed controls oscillation rate</li>
<li>Vibrato delay keeps note stable before modulation starts</li>
<li>Sweep can create laser/siren style transitions</li>
</ul>

<h2>Workflow</h2>
<ol>
<li>Create or select an instrument slot</li>
<li>Adjust envelope, modulation, and behavior</li>
<li>Preview, including loop preview when needed</li>
<li>Use the instrument id in tracker cells (In column)</li>
</ol>

<h2>Starter instrument recipes</h2>
<table>
<tr><th>Slot idea</th><th>Use</th><th>Suggested setup</th></tr>
<tr><td><b>00</b></td><td>Main lead</td><td>Medium decay, no vibrato</td></tr>
<tr><td><b>01</b></td><td>Bass</td><td>Longer decay, stable pitch</td></tr>
<tr><td><b>02</b></td><td>Arp/chords</td><td>Short decay</td></tr>
<tr><td><b>03</b></td><td>Expressive lead</td><td>Light vibrato + slight delay</td></tr>
<tr><td><b>04</b></td><td>FX/zap</td><td>Sweep enabled</td></tr>
</table>

<h2>Tip</h2>
<p>Use loop preview while editing instrument parameters. It is much faster than restarting playback after every tweak.</p>

<h2>Preview recommendation</h2>
<p>Test instrument at multiple octaves. Some settings that sound good in mid register
can become too sharp or too weak at higher/lower ranges.</p>

<h2>Loop preview workflow</h2>
<ol>
<li>Enable the <b>Loop</b> preview button in Instruments</li>
<li>The test note keeps replaying automatically</li>
<li>Adjust parameters while it loops (envelope, vibrato, sweep, ADSR, LFO)</li>
<li>Stop loop preview when the tone is validated</li>
</ol>
<p>This is the fastest way to refine timbre without restarting playback for each tweak.</p>

<h2>ADSR fallback rule</h2>
<p>When ADSR is enabled, it replaces the simple linear envelope. If all ADSR values are neutral/zero,
the classic envelope path can be used again depending on instrument state.</p>
)");

    case 3:
        return QStringLiteral(R"(
<h1>Tracker Basics</h1>
<p>The tracker is the main composition editor. Each row is a time step and each channel is a vertical lane.</p>

<h2>Channel layout</h2>
<table>
<tr><th>Channel</th><th>Type</th><th>Typical use</th></tr>
<tr><td><b>T0</b></td><td>Tone</td><td>Main melody</td></tr>
<tr><td><b>T1</b></td><td>Tone</td><td>Harmony / bass</td></tr>
<tr><td><b>T2</b></td><td>Tone</td><td>Counterline / extra layer</td></tr>
<tr><td><b>N</b></td><td>Noise</td><td>Percussion / effects</td></tr>
</table>

<h2>Main columns</h2>
<ul>
<li><b>Note</b>: pitch event (or OFF)</li>
<li><b>In</b>: instrument id (hex)</li>
<li><b>Vo</b>: attenuation override (hex)</li>
<li><b>FX</b>: effect command and parameter</li>
</ul>

<h2>Cell format example</h2>
<pre>
C-4  01  4  A03
^^^  ^^  ^  ^^^
|    |   |   +-- effect command + parameter
|    |   +------ attenuation override
|    +---------- instrument id
+--------------- note
</pre>

<h2>Reading rows</h2>
<ul>
<li>Row numbers are hexadecimal</li>
<li>Beat groups are color-highlighted</li>
<li>Playback row is highlighted in real time</li>
</ul>

<h2>Toolbar and side panel</h2>
<p>Top rows focus on transport and edit controls (Play, REC, Follow, TPR, Oct, Step, templates).
The right side panel groups import/export, mix controls (Mute/Solo), and keyboard help.</p>

<h2>REC mode behavior</h2>
<ul>
<li><b>REC ON</b>: key presses write data into the grid</li>
<li><b>REC OFF</b>: note keys can still preview sound without writing</li>
</ul>

<h2>Tracker controls at a glance</h2>
<table>
<tr><th>Control</th><th>Purpose</th></tr>
<tr><td><b>Play / Stop</b></td><td>Start/stop playback</td></tr>
<tr><td><b>REC</b></td><td>Record mode toggle for writing key input</td></tr>
<tr><td><b>Follow</b></td><td>Auto-follow playback row in editor</td></tr>
<tr><td><b>Song</b></td><td>Play full order list sequence</td></tr>
<tr><td><b>Loop Sel</b></td><td>Loop selected rows only</td></tr>
<tr><td><b>Clear</b></td><td>Clear active pattern content</td></tr>
<tr><td><b>TPR</b></td><td>Ticks per row (speed)</td></tr>
<tr><td><b>Oct</b></td><td>Keyboard base octave</td></tr>
<tr><td><b>Step</b></td><td>Row advance after note entry</td></tr>
<tr><td><b>Len</b></td><td>Pattern length</td></tr>
<tr><td><b>Pat</b></td><td>Active pattern index</td></tr>
<tr><td><b>+Pat / Clone</b></td><td>Create empty pattern or duplicate current pattern</td></tr>
<tr><td><b>KB</b></td><td>Keyboard layout (AZERTY/QWERTY)</td></tr>
<tr><td><b>Save / Load</b></td><td>Pattern save/load (<code>.ngpat</code>)</td></tr>
<tr><td><b>Save Song / Load Song</b></td><td>Full song save/load (<code>.ngps</code>)</td></tr>
<tr><td><b>Pre-baked / Hybrid</b></td><td>Export mode selection</td></tr>
<tr><td><b>Export C / ASM / WAV</b></td><td>Integration or audio render outputs</td></tr>
<tr><td><b>Import MIDI</b></td><td>Import standard MIDI into tracker/song structure</td></tr>
</table>

<h2>Order list basics</h2>
<ul>
<li>Each entry points to a pattern index</li>
<li>Order defines full song playback path</li>
<li>Loop marker defines where playback jumps back</li>
<li>Reordering entries changes song structure without rewriting patterns</li>
</ul>

<h2>Cursor sub-columns</h2>
<p>Each tracker cell has sub-fields. Left/right moves between them:</p>
<ul>
<li><b>Note</b>: note or OFF token</li>
<li><b>In</b>: instrument id (hex)</li>
<li><b>Vo</b>: attenuation override (hex or empty)</li>
<li><b>FX</b>: command nibble and parameter</li>
</ul>

<h2>Pattern capacity</h2>
<ul>
<li>Pattern length is typically configurable from 16 to 128 rows</li>
<li>Up to 64 patterns can be used in multi-pattern song workflow</li>
</ul>
)");

    case 4:
        return QStringLiteral(R"(
<h1>Tracker Keyboard</h1>
<h2>Note input</h2>
<p>Keyboard input works like a piano layout across two rows of keys.</p>
<ul>
<li>Switch layout between <b>QWERTY</b> and <b>AZERTY</b></li>
<li>Use octave controls to shift the active note range</li>
<li>Use step controls to define row jump after note entry</li>
</ul>

<h2>AZERTY note map</h2>
<pre>
Lower row (Oct):
W=C  S=C#  X=D  D=D#  C=E  V=F  G=F#  B=G  H=G#  N=A  J=A#  ,=B

Upper row (Oct+1):
A=C  2=C#  Z=D  3=D#  E=E  R=F  5=F#  T=G  6=G#  Y=A  7=A#  U=B
</pre>

<h2>QWERTY note map</h2>
<pre>
Lower row (Oct):
Z=C  S=C#  X=D  D=D#  C=E  V=F  G=F#  B=G  H=G#  N=A  J=A#  M=B

Upper row (Oct+1):
Q=C  2=C#  W=D  3=D#  E=E  R=F  5=F#  T=G  6=G#  Y=A  7=A#  U=B
</pre>

<h2>Noise channel key map</h2>
<table>
<tr><th>AZERTY</th><th>QWERTY</th><th>Noise mode</th></tr>
<tr><td>W</td><td>Z</td><td>P.H (Periodic High)</td></tr>
<tr><td>S</td><td>S</td><td>P.M (Periodic Medium)</td></tr>
<tr><td>X</td><td>X</td><td>P.L (Periodic Low)</td></tr>
<tr><td>D</td><td>D</td><td>P.T (Periodic Tone2)</td></tr>
<tr><td>A</td><td>Q</td><td>W.H (White High)</td></tr>
<tr><td>Z</td><td>2</td><td>W.M (White Medium)</td></tr>
<tr><td>E</td><td>W</td><td>W.L (White Low)</td></tr>
<tr><td>R</td><td>3</td><td>W.T (White Tone2)</td></tr>
</table>

<h2>Navigation and quick controls</h2>
<ul>
<li>Arrow keys: cursor movement</li>
<li>Shift + arrows: selection extension</li>
<li>Numpad + / -: octave up/down</li>
<li>Numpad * / /: step up/down</li>
<li>Enter: open edit dialog for active sub-column</li>
</ul>

<h2>Editing keys</h2>
<ul>
<li><code>Insert</code>: insert row</li>
<li><code>Shift+Delete</code>: delete row</li>
<li><code>Delete</code>: clear current cell content</li>
<li><code>Backspace</code>: write note-off and advance by Step</li>
</ul>

<h2>Hex entry for non-note fields</h2>
<p>Instrument, attenuation, FX command and FX parameter fields use hexadecimal digits (<code>0-9</code>, <code>A-F</code>).</p>

<h2>Step and octave quick reminder</h2>
<ul>
<li><b>Step</b> controls how far cursor moves after note entry</li>
<li><b>Oct</b> controls the base note range used by keyboard entry</li>
<li>If Step is <b>0</b>, cursor intentionally stays on the same row</li>
</ul>
)");

    case 5:
        return QStringLiteral(R"(
<h1>Tracker Editing</h1>
<h2>Entering notes quickly</h2>
<ol>
<li>Enable <b>REC</b> (record mode)</li>
<li>Place cursor on the Note sub-column</li>
<li>Play keys on your keyboard map</li>
<li>Use <b>Step</b> to control row advance after each note</li>
</ol>

<h2>Selection</h2>
<ul>
<li>Shift + arrows: row/channel range selection</li>
<li>Ctrl + click: discrete cell selection</li>
<li>Ctrl+A: select current channel range</li>
<li>Ctrl+Shift+A: select all channels</li>
</ul>

<h2>Edit operations</h2>
<ul>
<li>Insert / Delete / Duplicate row</li>
<li>Copy / Cut / Paste</li>
<li>Transpose (+/- semitone or octave)</li>
<li>Interpolate values over selection</li>
<li>Humanize attenuation</li>
<li>Batch apply by active sub-column</li>
</ul>

<h2>Cell-level editing</h2>
<ul>
<li><b>Delete</b>: clear current cell content</li>
<li><b>Backspace</b>: write note-off and move by Step</li>
<li>Hex digits (<code>0..F</code>) edit instrument/attenuation/FX fields</li>
</ul>

<h2>History</h2>
<ul>
<li><b>Ctrl+Z</b>: undo</li>
<li><b>Ctrl+Y</b> or <b>Ctrl+Shift+Z</b>: redo</li>
</ul>

<h2>Interpolate behavior</h2>
<p>Interpolation follows the active field (instrument, attenuation, FX command or FX parameter).
Select a range, place cursor on the field, then use interpolate to fill smooth transitions.</p>
<p>Typical use: create fade-out by interpolating attenuation from <code>0</code> to <code>F</code>.</p>

<h2>Batch apply behavior</h2>
<p>Batch apply writes a fixed value to all selected cells in the active field.
Useful for standardizing data before interpolation or humanize passes.</p>

<h2>Templates</h2>
<p>Use templates for quick rhythmic/melodic scaffolding
(kick, snare, hat, bass pulse, arp triad, starter, groove, fill, duo, full loop).
Templates apply to selection when present, otherwise to the full pattern.</p>

<h2>Mouse and context menu</h2>
<ul>
<li>Double-click a sub-column to open focused input dialog</li>
<li>Right-click to open full edit action menu</li>
<li>Ctrl+click adds/removes isolated cells from a sparse selection</li>
</ul>

<h2>Copy as text</h2>
<p><code>Ctrl+Shift+C</code> copies pattern/selection as readable ASCII text, useful for docs, issues, or chat review.</p>

<h2>Row operations recap</h2>
<ul>
<li><b>Insert</b>: insert empty row (shift down)</li>
<li><b>Shift+Delete</b>: delete row (shift up)</li>
<li><b>Ctrl+D</b>: duplicate current row</li>
</ul>
)");

    case 6:
        return QStringLiteral(R"(
<h1>Tracker Advanced</h1>
<h2>Transpose</h2>
<ul>
<li><b>Ctrl+Up / Ctrl+Down</b>: transpose selected notes by +/-1 semitone</li>
<li><b>Ctrl+Shift+Up / Ctrl+Shift+Down</b>: transpose by +/-12 semitones</li>
</ul>

<h2>Contextual interpolation (Ctrl+I)</h2>
<p>Interpolation depends on the active field:</p>
<ul>
<li><b>In</b>: instrument id interpolation</li>
<li><b>Vo</b>: attenuation interpolation (fade-in/fade-out)</li>
<li><b>FX param</b>: parameter interpolation (00..FF)</li>
</ul>

<h3>How it works</h3>
<ul>
<li>Interpolation uses the active field (where cursor sub-column is)</li>
<li>It applies to the current selection (range or discrete selection)</li>
<li>You need at least a 2-row span per affected channel</li>
</ul>

<h3>Fade-out example (step by step)</h3>
<ol>
<li>Put cursor on Vo field at start row, enter <code>0</code></li>
<li>Move to end row, enter <code>F</code></li>
<li>Select the full range between start and end</li>
<li>Press <b>Ctrl+I</b></li>
</ol>

<h3>Typical result</h3>
<pre>
Before:  0  -  -  -  -  F
After :  0  1  3  6  A  F
</pre>

<h3>Edge cases</h3>
<ul>
<li>If start Vo is empty, tool can infer from default behavior</li>
<li>If end Vo is empty, interpolation may converge to lower level</li>
<li>Interpolation on wrong field does nothing useful, verify cursor sub-column</li>
<li>With no explicit Vo endpoints, interpolation can default to a practical 0 to F fade shape</li>
<li>If the first selected Vo is empty, interpolation can assume a strong start value</li>
<li>If the last selected Vo is empty, interpolation can resolve toward a quieter end value</li>
</ul>

<h3>Fade-in tip</h3>
<p>Invert endpoints: start with <code>F</code>, end with <code>0</code>, then interpolate.</p>

<h3>Detailed visual interpolation example</h3>
<pre>
Before Ctrl+I:            After Ctrl+I:
Row  Note  Vo             Row  Note  Vo
00   C-4   0              00   C-4   0
01   D-4   -              01   D-4   1
02   E-4   -              02   E-4   2
03   F-4   -              03   F-4   3
...
0E   A-4   -              0E   A-4   E
0F   G-4   F              0F   G-4   F
</pre>

<h3>If Ctrl+I seems to do nothing</h3>
<ul>
<li>Confirm a visible selection exists</li>
<li>Confirm cursor is on the intended sub-column (In, Vo, or FX param)</li>
<li>Confirm at least 2 rows are selected for each affected lane</li>
</ul>

<h2>Humanize attenuation (Ctrl+H)</h2>
<p>Applies controlled random attenuation variation over selection or current cell.</p>
<ul>
<li>Range: small deltas such as +/-1 to +/-4</li>
<li>Probability: sparse or full application</li>
<li>Useful to break mechanical sounding patterns</li>
</ul>
<p>For AUTO/empty attenuation cells, humanize can materialize explicit attenuation values when needed.</p>

<h2>Batch apply (Ctrl+B)</h2>
<p>Applies a fixed value to selected cells according to active field:</p>
<ul>
<li>Instrument id</li>
<li>Attenuation (including AUTO behavior where supported)</li>
<li>FX command nibble</li>
<li>FX parameter byte</li>
</ul>
<p>Typical ranges: instrument <code>00..7F</code>, attenuation <code>0..F</code> (or AUTO), FX command <code>0..F</code>, FX param <code>00..FF</code>.</p>

<h2>Pattern templates (Ctrl+T)</h2>
<p>Quick scaffolding templates are available:
kick, snare, hat, bass pulse, arp triad, starter, kick+hat groove, snare fill, bass+arp duo, and full groove loop.</p>
<ul>
<li>Apply to selection when a selection exists</li>
<li>Otherwise apply to full pattern</li>
<li>Useful to bootstrap structure before custom editing</li>
<li>Clear target rows option can reset the zone before applying template data</li>
</ul>

<h2>Mute / Solo</h2>
<ul>
<li><b>F1..F4</b> toggles channel mute for T0/T1/T2/N</li>
<li>Channel header click toggles mute</li>
<li>Solo lets you isolate one channel while editing</li>
</ul>

<h2>Noise channel details</h2>
<p>Noise channel (N) uses 8 hardware noise modes defined by type/rate combinations:
periodic or white noise with High/Medium/Low/Tone2 rate behavior.</p>

<h3>8 noise modes</h3>
<table>
<tr><th>Display</th><th>Type</th><th>Rate</th><th>Typical use</th></tr>
<tr><td><b>P.H</b></td><td>Periodic</td><td>High</td><td>Kick/tom style</td></tr>
<tr><td><b>P.M</b></td><td>Periodic</td><td>Medium</td><td>Mid percussion</td></tr>
<tr><td><b>P.L</b></td><td>Periodic</td><td>Low</td><td>Low rumble</td></tr>
<tr><td><b>P.T</b></td><td>Periodic</td><td>Tone2</td><td>Tone-linked special</td></tr>
<tr><td><b>W.H</b></td><td>White</td><td>High</td><td>Hi-hat/crash</td></tr>
<tr><td><b>W.M</b></td><td>White</td><td>Medium</td><td>Snare-like</td></tr>
<tr><td><b>W.L</b></td><td>White</td><td>Low</td><td>Noise bed</td></tr>
<tr><td><b>W.T</b></td><td>White</td><td>Tone2</td><td>Tone-linked white mode</td></tr>
</table>

<h3>Noise key input reminder</h3>
<p>When cursor is on noise lane and REC is enabled, keyboard note map switches to noise mode mapping.</p>
<p>Double-clicking a noise note cell can open a visual selector dialog in supported views.</p>
<table>
<tr><th>AZERTY</th><th>QWERTY</th><th>Noise mode</th></tr>
<tr><td>W</td><td>Z</td><td>P.H</td></tr>
<tr><td>S</td><td>S</td><td>P.M</td></tr>
<tr><td>X</td><td>X</td><td>P.L</td></tr>
<tr><td>D</td><td>D</td><td>P.T</td></tr>
<tr><td>A</td><td>Q</td><td>W.H</td></tr>
<tr><td>Z</td><td>2</td><td>W.M</td></tr>
<tr><td>E</td><td>W</td><td>W.L</td></tr>
<tr><td>R</td><td>3</td><td>W.T</td></tr>
</table>

<h2>Context menu</h2>
<p>Right-click opens a full action menu: cut/copy/paste, row ops, transpose,
interpolate/humanize/batch, save/load, and clear pattern.</p>

<h2>Multi-pattern song mode</h2>
<p>Compose with multiple patterns, then build full song playback using the order list.</p>

<h3>Pattern management</h3>
<ul>
<li>Add empty pattern for a new section</li>
<li>Clone current pattern for quick variation</li>
<li>Delete patterns you no longer need</li>
<li>Maximum pattern count is 64 in the current song model</li>
</ul>

<h3>Order list</h3>
<ul>
<li>Each order entry references a pattern index</li>
<li>Loop marker defines return point after last order entry</li>
<li>Reorder entries to test arrangement quickly</li>
</ul>
<pre>
00: Pat 00 (64)
L&gt;01: Pat 01 (32)
02: Pat 00 (64)
</pre>
<p>In Song mode, playback returns to <b>L&gt;</b> when it reaches the end of the order list.</p>

<h3>Typical arrangement workflow</h3>
<ol>
<li>Create intro pattern</li>
<li>Create verse and chorus patterns</li>
<li>Build order: intro, verse, chorus, verse, chorus</li>
<li>Set loop point after intro</li>
<li>Validate with Song playback</li>
</ol>

<h2>Grid visual cues</h2>
<ul>
<li>Beat rows are highlighted every 4 and 16 rows</li>
<li>Instrument color helps identify source timbre quickly</li>
<li>Muted channels show a dark overlay</li>
<li>Attenuation bars often use green-to-red intensity cues</li>
<li>Cursor state helps mode reading: normal edit vs REC-focused behavior</li>
</ul>
)");

    case 7:
        return QStringLiteral(R"(
<h1>Tracker Effects</h1>
<p>Each FX entry is a command nibble plus parameter (hex). Enter FX in the dedicated column:
first command digit, then parameter digits.</p>

<h2>How to enter effects</h2>
<ol>
<li>Move cursor to FX command sub-column</li>
<li>Type command nibble (<code>0..F</code>)</li>
<li>Type parameter value (hex)</li>
</ol>

<h2>Effect reference</h2>
<table>
<tr><th>Code</th><th>Name</th><th>Parameter</th><th>Description</th></tr>
<tr><td><b>0xy</b></td><td>Arpeggio</td><td>x,y semitone offsets</td><td>Cycles root and two offsets quickly to emulate chord feel</td></tr>
<tr><td><b>1xx</b></td><td>Slide up</td><td>Speed</td><td>Progressive pitch rise</td></tr>
<tr><td><b>2xx</b></td><td>Slide down</td><td>Speed</td><td>Progressive pitch drop</td></tr>
<tr><td><b>3xx</b></td><td>Portamento</td><td>Speed</td><td>Glide toward target note instead of hard jump</td></tr>
<tr><td><b>4xx</b></td><td>Pitch bend</td><td>Signed offset style value</td><td>Persistent pitch offset until next 4xx command</td></tr>
<tr><td><b>Axy</b></td><td>Volume slide</td><td>x=up, y=down</td><td>Gradual loudness movement</td></tr>
<tr><td><b>Bxx</b></td><td>Set speed</td><td>TPR value</td><td>Updates tracker timing (ticks per row)</td></tr>
<tr><td><b>Cxx</b></td><td>Note cut</td><td>Ticks</td><td>Cuts note after xx ticks</td></tr>
<tr><td><b>Dxx</b></td><td>Note delay</td><td>Ticks</td><td>Delays note trigger within the row</td></tr>
<tr><td><b>Exy</b></td><td>Host command</td><td>x=type, y=value</td><td>Global command, for example fade/tempo related behavior</td></tr>
<tr><td><b>Fxx</b></td><td>Expression</td><td>0..F attenuation offset</td><td>Persistent per-channel expression offset until changed</td></tr>
</table>

<h2>Operational detail notes</h2>
<ul>
<li><b>4xx</b>: use to keep a lane bent until an explicit reset/replacement</li>
<li><b>Bxx</b>: modifies speed immediately (timing-sensitive operation)</li>
<li><b>Cxx</b>: useful for short percussive articulation</li>
<li><b>Dxx</b>: useful for groove offsets and delayed accents</li>
<li><b>Exy</b>: host/global scope, not just one lane</li>
<li><b>Fxx</b>: keeps a lane pushed back/forward in mix over multiple rows</li>
<li><b>5xx..9xx</b>: currently reserved / not implemented in this tracker runtime</li>
</ul>

<h3>Parameter reading quick guide</h3>
<ul>
<li><b>0xy</b>: x and y are two semitone offsets from the base note</li>
<li><b>Axy</b>: x is upward slide amount, y is downward slide amount</li>
<li><b>Bxx</b>: xx is new timing value (TPR-style speed control)</li>
<li><b>Exy</b>: x selects host command family, y carries value/speed</li>
</ul>

<h3>Per-effect behavior summary</h3>
<ul>
<li><b>0xy Arpeggio</b>: cycles chord tones quickly for classic chip chord illusion</li>
<li><b>1xx/2xx Slides</b>: continuously pull pitch up/down while active</li>
<li><b>3xx Portamento</b>: glides toward target note instead of instant jump</li>
<li><b>4xx Pitch bend</b>: persistent divider offset until replaced/cleared</li>
<li><b>Axy Volume slide</b>: dynamic gain movement by row progression</li>
<li><b>Cxx Note cut</b>: hard stop after tick delay, useful for tight hats/staccato</li>
<li><b>Dxx Note delay</b>: delayed trigger inside row, useful for groove/swing</li>
<li><b>Exy Host command</b>: global engine-level command flow</li>
<li><b>Fxx Expression</b>: persistent channel attenuation offset</li>
</ul>

<h3>4xx signed behavior</h3>
<ul>
<li><code>01..7F</code>: positive divider offset style (usually perceived as lower pitch)</li>
<li><code>80..FF</code>: negative divider offset style (usually perceived as higher pitch)</li>
<li><code>00</code>: clear/no bend offset</li>
</ul>

<h3>Exy practical split</h3>
<ul>
<li><code>E0x</code>: global fade control family (x controls speed, 0 clears fade)</li>
<li><code>E1x</code>: compact speed/TPR control family</li>
</ul>

<h2>Practical examples</h2>
<h3>Arpeggio chord feel</h3>
<p><code>047</code> on C-4 cycles C, E, G quickly for a major chord impression.
Use <code>037</code> for a minor flavor (C, D#, G).</p>

<h3>Rising tone</h3>
<p><code>110</code> gives a slow rise, <code>140</code> rises faster.</p>

<h3>Portamento phrase</h3>
<p>Place a new note with <code>310</code> to glide from previous pitch to target note.</p>

<h3>Crescendo / decrescendo</h3>
<p><code>A10</code> can push level up; <code>A01</code> can pull level down.</p>

<h3>Short hi-hat style noise</h3>
<p>On noise lane, combine bright noise with <code>C02</code> for short hit length.</p>

<h3>Swing-like timing</h3>
<p>Use <code>D03</code> on alternating notes to delay off-beat hits.</p>
<pre>
00  C-4  ---
04  E-4  D03
08  C-4  ---
0C  G-4  D03
</pre>

<h3>Global fade behavior</h3>
<p><code>E03</code> applies a global fade-like command flow; <code>E00</code> can stop/clear it depending on command type.</p>

<h3>Channel backgrounding</h3>
<p><code>F04</code> keeps a channel more discreet; reset with <code>F00</code>.</p>
<pre>
00  C-4  F04
04  E-4  ---
08  G-4  F00
</pre>

<h2>Important notes</h2>
<ul>
<li>Most effects are row-local; persistent exceptions include <b>4xx</b> and <b>Fxx</b></li>
<li><b>Exy</b> is global and has channel constraints in hybrid export path</li>
<li>Effect-only rows are valid (note can stay empty)</li>
<li><code>000</code> is effectively neutral/default in many contexts</li>
<li>Row-to-row state continuity matters for slides/portamento/expression</li>
<li>Effect data is preserved in <code>.ngpat</code>/<code>.ngps</code> and included in copy-as-text output</li>
<li>Portamento can continue across rows until the target pitch is reached</li>
</ul>
<p>In hybrid export flows, host/global effects may be emitted with channel restrictions; keep driver and tool expectations aligned.</p>

<p>For difficult parity issues, use runtime debug row dumps and compare tracker playback against exported runtime behavior.</p>
)");

    case 8:
        return QStringLiteral(R"(
<h1>Playback</h1>
<h2>Commands</h2>
<table>
<tr><th>Action</th><th>Shortcut</th><th>Control</th></tr>
<tr><td>Play/Pause</td><td>Space</td><td>Play</td></tr>
<tr><td>Play from start</td><td>F5</td><td>-</td></tr>
<tr><td>Stop</td><td>F8</td><td>Stop</td></tr>
<tr><td>Song playback</td><td>-</td><td>Song</td></tr>
<tr><td>Loop selection</td><td>-</td><td>Loop Sel</td></tr>
</table>

<h2>Per-channel controls</h2>
<ul>
<li><b>F1..F4</b>: mute toggle for T0, T1, T2, N</li>
<li>Mute/Solo buttons: isolate or silence channels while editing</li>
</ul>

<h2>Modes</h2>
<ul>
<li>Pattern playback</li>
<li>Song playback through order list</li>
<li>Loop selected rows</li>
</ul>

<h2>Song mode behavior</h2>
<p>Song mode plays patterns using order list sequence.
When playback reaches the end, it returns to configured loop marker.</p>

<h2>Loop selection workflow</h2>
<ol>
<li>Select rows (Shift + arrows)</li>
<li>Start <b>Loop Sel</b></li>
<li>Edit while loop runs</li>
<li>Stop loop or clear selection to resume normal flow</li>
</ol>

<h2>Useful controls</h2>
<ul>
<li>Follow mode keeps current row visible</li>
<li>Mute/Solo per channel</li>
<li>Runtime debug per-row dump</li>
</ul>

<h2>Timing behavior</h2>
<p>Playback rate is based on TPR and the internal engine tick flow. Speed-change effects can update timing during playback.</p>

<h2>TPR quick guidance</h2>
<ul>
<li>TPR 4: fast</li>
<li>TPR 8: default mid-tempo</li>
<li>TPR 12+: slower feel</li>
</ul>

<h2>Approximate BPM reference</h2>
<table>
<tr><th>TPR</th><th>Approx BPM</th><th>Feel</th></tr>
<tr><td>3</td><td>~300</td><td>Very fast</td></tr>
<tr><td>4</td><td>~225</td><td>Fast</td></tr>
<tr><td>6</td><td>~150</td><td>Energetic</td></tr>
<tr><td>8</td><td>~112</td><td>Default medium</td></tr>
<tr><td>10</td><td>~90</td><td>Moderate-slow</td></tr>
<tr><td>12</td><td>~75</td><td>Slow</td></tr>
<tr><td>16</td><td>~56</td><td>Very slow</td></tr>
</table>

<h2>Runtime debug (Dbg RT)</h2>
<p>Enable Dbg RT to append per-row runtime state in tracker log.
Use it to inspect note, instrument, attenuation, FX state and output behavior per channel.</p>
<ul>
<li>Workflow: enable Dbg RT, run Play/Song, inspect row blocks in tracker log</li>
<li>Typical fields: cell note/instrument/attenuation/FX, runtime divider/noise mode, runtime attenuation, active FX/expression/pitch bend state</li>
<li>Disable Dbg RT after diagnosis to keep logs readable</li>
</ul>

<h2>Looping behavior</h2>
<p>Pattern playback loops at pattern end by design.
Use Stop or Song mode when you need different flow.</p>

<h2>Live note preview</h2>
<p>During REC note entry, a short preview trigger (~100 ms) is expected. It is a quick confirmation mechanism, not a full phrase playback.</p>
)");

    case 9:
        return QStringLiteral(R"(
<h1>Export and Save</h1>
<h2>File formats</h2>
<table>
<tr><th>Extension</th><th>Content</th><th>Use</th></tr>
<tr><td><b>.ngpat</b></td><td>Single pattern JSON</td><td>Pattern save/load</td></tr>
<tr><td><b>.ngps</b></td><td>Song JSON (multi-pattern + order)</td><td>Song save/load</td></tr>
<tr><td><b>.wav</b></td><td>PCM 16-bit mono audio</td><td>Offline listening preview</td></tr>
<tr><td><b>.mid/.midi</b></td><td>MIDI input</td><td>Import into tracker</td></tr>
</table>

<h2>Save/Load</h2>
<p>Use Save/Load for pattern/song persistence in project workflow or standalone editing.</p>

<h3>Pattern save (.ngpat)</h3>
<ul>
<li><code>Ctrl+S</code> or Save button stores current pattern JSON</li>
<li>Frequent saves are recommended during heavy editing sessions</li>
</ul>

<h3>Song save (.ngps)</h3>
<ul>
<li>Save Song stores multi-pattern content, order list and loop marker</li>
<li>Load Song restores full song structure and arrangement state</li>
</ul>

<h3>Pattern load</h3>
<ul>
<li><code>Ctrl+O</code> or Load opens a <code>.ngpat</code> file</li>
<li>Current pattern is replaced; undo can help recover if needed</li>
</ul>

<h2>Typical save workflow</h2>
<ul>
<li>Use regular saves while editing patterns</li>
<li>Save full song before major structural changes</li>
<li>Reload older .ngps snapshots when testing alternatives</li>
</ul>

<h2>Export targets</h2>
<ul>
<li><b>Export C</b>: source export for integration in C projects</li>
<li><b>Export ASM</b>: include-oriented assembly flow</li>
<li><b>Export WAV</b>: offline audio rendering for listening and validation</li>
</ul>

<h2>Export mode guidance</h2>
<p><b>Pre-baked</b> is recommended when strict playback parity is required.</p>
<p><b>Hybrid</b> is useful when you want compact streams and instrument/runtime opcode behavior.</p>

<h2>Mode</h2>
<p><b>Pre-baked</b>: maximum playback fidelity (tick-driven behavior baked in stream).</p>
<p><b>Hybrid</b>: compact runtime stream with more instrument/runtime semantics.</p>

<h2>MIDI import notes</h2>
<ul>
<li>Tone data is mapped to T0/T1/T2</li>
<li>Percussion/noise-oriented data is mapped to channel N</li>
<li>Velocity is converted into attenuation where applicable</li>
<li>Importer can split long material into multiple patterns</li>
<li>Very dense polyphony will be reduced to fit available channels</li>
<li>MIDI tempo can be used to suggest a practical TPR</li>
<li>Channel 10 percussion is typically routed toward noise-lane behavior</li>
</ul>

<h2>WAV export</h2>
<ol>
<li>Click <b>Export WAV</b></li>
<li>Choose output file location</li>
<li>Tool renders offline (song mode or current pattern mode)</li>
<li>Listen and compare against tracker intent</li>
</ol>
<p>Output is PCM 16-bit mono, 44100 Hz. Mute states are respected.</p>

<h2>MIDI import workflow</h2>
<ol>
<li>Click <b>Import MIDI</b></li>
<li>Select .mid or .midi file</li>
<li>Review imported patterns/order and timing</li>
<li>Adjust instruments, attenuation and FX as needed</li>
</ol>

<h2>C/ASM export workflow</h2>
<ol>
<li>Compose and validate in tracker</li>
<li>Pick export mode (Pre-baked or Hybrid)</li>
<li>Export C or ASM</li>
<li>Integrate files into game project</li>
</ol>

<h3>Pre-baked mode</h3>
<p>Pre-baked simulates playback tick-by-tick and writes streams matching preview behavior as closely as possible.
It is the best mode for parity and debugging confidence.</p>
<ul>
<li>Excellent tool-to-runtime consistency</li>
<li>Lower runtime CPU burden</li>
<li>Larger stream size in ROM</li>
<li>Best for fast stabilization and deterministic behavior</li>
</ul>

<h3>Hybrid mode</h3>
<p>Hybrid emits more compact row-oriented streams and relies more on runtime/instrument opcodes.</p>
<ul>
<li>Smaller stream footprint</li>
<li>Higher dependency on matching driver runtime behavior</li>
<li>Great when integration pipeline is stable and controlled</li>
<li>More runtime-side behavior resolution</li>
</ul>
<p>Hybrid does <b>not</b> target full tracker FX parity by default for every effect family.
Typically supported FX in hybrid flow are <b>4xx</b>, <b>Bxx</b>, <b>Cxx</b>, <b>Dxx</b>, <b>Exy</b>, <b>Fxx</b>.
Arpeggio/slide families often require pre-baked for strict parity.</p>

<h3>Quick choice guide</h3>
<ul>
<li>Need reliable parity fast: choose <b>Pre-baked</b></li>
<li>Need tighter ROM size with driver-aware flow: choose <b>Hybrid</b></li>
<li>Using many tracker FX behaviors: Pre-baked is usually safer</li>
</ul>

<h3>Pre-baked vs Hybrid comparison</h3>
<table>
<tr><th>Aspect</th><th>Pre-baked</th><th>Hybrid</th></tr>
<tr><td>Tool to runtime parity</td><td>Excellent</td><td>Good with aligned driver pack</td></tr>
<tr><td>Runtime CPU cost</td><td>Lower</td><td>Higher (more runtime behavior)</td></tr>
<tr><td>Stream size</td><td>Larger</td><td>Smaller</td></tr>
<tr><td>Integration sensitivity</td><td>Lower</td><td>Higher (driver coupling)</td></tr>
<tr><td>Best for</td><td>Stability and debugging</td><td>Compact runtime deployment</td></tr>
</table>

<h3>Detailed mode comparison</h3>
<table>
<tr><th>Dimension</th><th>Pre-baked</th><th>Hybrid</th></tr>
<tr><td>Playback predictability</td><td>Very high</td><td>High with correct driver set</td></tr>
<tr><td>Tool/driver drift risk</td><td>Low</td><td>Medium to high</td></tr>
<tr><td>Runtime state complexity</td><td>Lower</td><td>Higher</td></tr>
<tr><td>ROM footprint</td><td>Higher</td><td>Lower</td></tr>
<tr><td>Best project phase</td><td>Early/mid development</td><td>Optimization phase</td></tr>
<tr><td>FX coverage safety</td><td>Broader tracker parity</td><td>Driver-dependent subset behavior</td></tr>
</table>

<h3>Hybrid opcode set (high level)</h3>
<ul>
<li><code>F0..F4</code>: instrument/runtime setup family</li>
<li><code>F6</code>: host command family</li>
<li><code>F7</code>: expression offset</li>
<li><code>F8</code>: pitch bend</li>
<li><code>F9</code>: ADSR parameters</li>
<li><code>FA</code>: LFO parameters</li>
</ul>
<table>
<tr><th>Opcode</th><th>Role</th><th>Notes</th></tr>
<tr><td><code>F4</code></td><td>Set instrument</td><td>Runtime selects instrument profile</td></tr>
<tr><td><code>F0</code></td><td>Set attenuation</td><td>Base lane loudness state</td></tr>
<tr><td><code>F1</code></td><td>Set envelope</td><td>Envelope step/speed style parameters</td></tr>
<tr><td><code>F2</code></td><td>Set vibrato</td><td>Depth/speed/delay behavior</td></tr>
<tr><td><code>F3</code></td><td>Set sweep</td><td>Sweep target/step/speed style parameters</td></tr>
<tr><td><code>F6</code></td><td>Host command</td><td>Global flow commands, emitted with channel constraints</td></tr>
<tr><td><code>F7</code></td><td>Expression</td><td>Persistent per-channel attenuation offset</td></tr>
<tr><td><code>F8</code></td><td>Pitch bend</td><td>Persistent per-channel divider offset</td></tr>
<tr><td><code>F9</code></td><td>ADSR</td><td>Attack/Decay/Sustain/Release runtime state</td></tr>
<tr><td><code>FA</code></td><td>LFO</td><td>Pitch modulation wave/rate/depth</td></tr>
</table>

<h2>Player tab buttons (export validation context)</h2>
<ul>
<li><b>Load Driver</b>: advanced option for external custom driver tests; optional in normal flow</li>
<li><b>Load MIDI</b>: quick import route for player-side check</li>
<li><b>Convert + Play</b>: rebuilds temporary export stream and starts immediate validation playback</li>
<li><b>Export</b>: generate final integration files</li>
</ul>

<h2>Player runtime opcode coverage</h2>
<p>Player preview path supports the extended opcode family
<code>F0..F4</code>, <code>F6</code>, <code>F7</code>, <code>F8</code>, <code>F9</code>, <code>FA</code>
to validate instrument/runtime-oriented behavior earlier in the tool.</p>

<h2>Export audit warnings</h2>
<p>Export runs an internal audit and reports warnings in tracker log.</p>
<ul>
<li>Warnings indicate parity risk, unsupported patterns, or out-of-range values</li>
<li>Fix warnings in tracker, then re-export</li>
<li>Warnings can also be echoed in generated export comments</li>
<li>Typical examples: out-of-range instrument index, unsupported runtime nibble, note-table range pressure</li>
</ul>

<h2>Driver pack reminder</h2>
<p>For advanced hybrid parity, keep the game-side driver pack aligned with tool expectations
(minimum synchronized audio runtime source/header set).</p>
<p>Practical baseline: keep core runtime audio source/header files synchronized with the export opcode model.</p>

<h2>Generated stream structures</h2>
<p>C/ASM export includes note tables and per-channel streams, with loop offsets for runtime playback.</p>
<ul>
<li><code>NOTE_TABLE</code> data</li>
<li>Tone channel streams</li>
<li>Noise channel stream</li>
<li>Loop offset markers</li>
</ul>

<h2>C/ASM export content checklist</h2>
<ul>
<li>Frequency divider note tables</li>
<li>Per-channel byte streams</li>
<li>Loop offsets and end markers</li>
<li>Optional warnings header comments</li>
</ul>

<h2>Project mode export all</h2>
<p>Project workflow can export all songs and shared banks in one pass.</p>
<ul>
<li>Song exports by project list</li>
<li>Global instrument bank export</li>
<li>Global SFX bank export</li>
<li>Manifest/API helper generation for integration</li>
<li>Optional driver-pack export action for integration bootstrap</li>
</ul>

<h2>Typical generated project files</h2>
<ul>
<li>Song exports (<code>song_*.c</code> or ASM includes)</li>
<li><code>project_instruments.c</code> global instrument data</li>
<li><code>project_sfx.c</code> global SFX data</li>
<li>Project audio manifest and API helper files</li>
</ul>

<h2>Project export outputs (common set)</h2>
<ul>
<li><code>project_audio_manifest.txt</code></li>
<li><code>project_audio_api.h</code> / <code>project_audio_api.c</code> (C mode)</li>
<li>Per-song generated stream files with namespaced symbols</li>
</ul>
<p>These files are typically generated under the project <code>exports/</code> directory.</p>

<h2>Symbol namespace behavior</h2>
<p>Project song symbols can be namespaced to reduce linker collisions across large game codebases.</p>

<h2>Project Export All routine</h2>
<ol>
<li>Select export mode for songs/all</li>
<li>Run full project export</li>
<li>Review generated files and warning report</li>
<li>Validate with Player and runtime build</li>
</ol>

<h2>Project integration helper</h2>
<p>Project API helpers can centralize note-table binding and multi-channel playback start by song index.
Use those generated helpers to reduce integration mistakes and symbol mismatches.</p>
<ul>
<li>Typical helper flow: bind note table, then start 4-channel loop playback</li>
<li>Project API wrappers are designed to keep song index usage consistent</li>
</ul>

<h2>Level and normalization notes</h2>
<p>Use project-level analysis tools to estimate peak behavior and adjust attenuation offsets before final export.</p>
<ul>
<li>Song-level analysis can estimate offline peak behavior</li>
<li>Song normalization usually adjusts explicit attenuation cells (AUTO cells are preserved)</li>
<li>Project SFX normalization can apply global offsets to tone/noise attenuation banks</li>
</ul>

<h2>Integration recommendation</h2>
<p>Lock one mode per milestone (usually Pre-baked first), validate in game runtime,
then migrate to Hybrid only after driver/tool parity is confirmed.</p>

<h2>Quick SFX integration reminder</h2>
<p>Keep generated SFX banks synchronized with game-side runtime symbols and active driver set.</p>

<h2>When warnings appear</h2>
<ul>
<li>Treat warnings as integration risk indicators, not cosmetic messages</li>
<li>Fix warnings before release candidates</li>
<li>Re-export and re-validate after each warning-related fix</li>
<li>Export can still finish with warnings, but parity risk remains until resolved</li>
</ul>

<h2>Quick game-side SFX integration reminder</h2>
<p>Keep generated SFX data and runtime symbols synchronized with the active driver pack to avoid mismatches.</p>

<h2>SFX project workflow recap</h2>
<ol>
<li>Create/tune SFX in SFX Lab</li>
<li>Save each SFX to project bank</li>
<li>Run Project Export All</li>
<li>Integrate generated project SFX source into game build</li>
</ol>

<h2>Game-side SFX binding note</h2>
<p>Game runtime usually triggers exported project SFX by index (for example through a local <code>Sfx_Play(id)</code> wrapper)
using generated tone/noise parameter arrays. Extended tone ADSR/LFO arrays are also exported
for advanced/custom runtime paths.</p>

<h2>Player tab relation</h2>
<p>Use Player tab to validate exported behavior quickly.
Driver-like preview paths help detect tool/runtime differences early.</p>

<h2>Recommended export check</h2>
<ul>
<li>Review export warnings</li>
<li>Compare preview and runtime behavior</li>
<li>Re-export after tracker fixes</li>
</ul>

<h2>Copy as text</h2>
<p>Use tracker copy-as-text for quick sharing/review of pattern content in chats, docs, or issue reports.</p>
)");

    case 10:
        return QStringLiteral(R"(
<h1>SFX Lab</h1>
<p>SFX Lab is for fast experimentation with tone/noise behavior and one-shot effects.</p>

<h2>Main controls</h2>
<ul>
<li>Tone/noise mode switches</li>
<li>Attenuation and envelope shaping</li>
<li>Pitch/sweep related controls</li>
<li>Tone ADSR5 (AR/DR/SL/SR/RR) controls</li>
<li>Tone dual-LFO (LFO1/LFO2 + algorithm 0..7) controls</li>
<li>Trigger and preview controls</li>
<li>Realtime parameter knobs/sliders for quick iteration</li>
<li>Driver-faithful preview mode for closer runtime behavior</li>
<li>Presets and Save To Project for production workflow</li>
</ul>

<h2>Control reference (quick map)</h2>
<table>
<tr><th>Control</th><th>Purpose</th></tr>
<tr><td>Tone On / Noise On</td><td>Enable/disable tonal or noise layer in the SFX definition</td></tr>
<tr><td>Tone Ch / Tone Div</td><td>Select tone lane target and pitch divider</td></tr>
<tr><td>Tone Attn / Frames</td><td>Tone loudness and duration profile</td></tr>
<tr><td>Tone Sweep / Env</td><td>Tone glide and envelope driver-side behavior</td></tr>
<tr><td>Tone ADSR</td><td>Attack/Decay/Sustain level/Sustain rate/Release for tone one-shots</td></tr>
<tr><td>Tone LFO / MOD2</td><td>LFO1 + LFO2 modulation with algorithm routing (0..7)</td></tr>
<tr><td>Noise Rate / Type</td><td>Noise mode selection (high/med/low/tied-to-T2 + periodic/white)</td></tr>
<tr><td>Noise Attn / Frames</td><td>Noise loudness and duration profile</td></tr>
<tr><td>Noise Burst / Env</td><td>Advanced noise burst and envelope shaping</td></tr>
<tr><td>Save To Project</td><td>Append current SFX definition to project SFX bank</td></tr>
</table>

<h2>Typical workflow</h2>
<ol>
<li>Design a raw effect</li>
<li>Preview with direct controls</li>
<li>Refine envelope/pitch/noise parameters</li>
<li>Integrate with project export flow</li>
</ol>

<h2>Use cases</h2>
<ul>
<li>Percussion and hit sounds</li>
<li>UI beeps and confirmations</li>
<li>Short retro noise textures</li>
<li>Transient "zap", "laser", and impact sounds</li>
</ul>

<h2>What to tune first</h2>
<ul>
<li>Attenuation and envelope shape</li>
<li>ADSR + LFO modulation for tone dynamics</li>
<li>Noise type/rate for transient character</li>
<li>Sweep or pitch behavior for impact</li>
</ul>

<h2>Practical tip</h2>
<p>Use SFX Lab for rapid prototyping, then move validated behavior into the project export pipeline.</p>

<h2>Why this tab matters</h2>
<p>SFX Lab is ideal for quick iteration on game-feel sounds before locking them into project-level assets.</p>

<h2>Recommended production flow</h2>
<ol>
<li>Prototype quickly in SFX Lab</li>
<li>Validate in context (player/project playback)</li>
<li>Adjust level and timing against music mix</li>
<li>Finalize in project export pack</li>
</ol>

<h2>Project export link</h2>
<p>Saved SFX entries are turned into indexed project-bank data during Export All,
so gameplay code can trigger effects by id in a deterministic way.</p>
)");

    case 11:
        return QStringLiteral(R"(
<h1>Keyboard Shortcuts</h1>
<h2>Navigation</h2>
<ul>
<li><code>Arrows</code>: move cursor</li>
<li><code>Tab / Shift+Tab</code>: move between channels</li>
<li><code>PageUp / PageDown</code>: jump rows</li>
<li><code>Home / End</code>: first/last row</li>
<li><code>Esc</code>: clear selection</li>
<li><code>Left / Right</code>: move between sub-columns (Note/In/Vo/FX)</li>
</ul>

<h2>Transport</h2>
<ul>
<li><code>Space</code>: play/pause</li>
<li><code>F5</code>: play from start</li>
<li><code>F8</code>: stop</li>
<li><code>F1..F4</code>: mute T0/T1/T2/N</li>
</ul>

<h2>File</h2>
<ul>
<li><code>Ctrl+S / Ctrl+O</code>: save/load</li>
</ul>

<h2>Editing</h2>
<ul>
<li><code>Ctrl+Z / Ctrl+Y</code>: undo/redo</li>
<li><code>Ctrl+Shift+Z</code>: redo alternative</li>
<li><code>Ctrl+C / Ctrl+X / Ctrl+V</code>: copy/cut/paste</li>
<li><code>Ctrl+Shift+C</code>: copy as text</li>
<li><code>Ctrl+D</code>: duplicate row</li>
<li><code>Ctrl+I</code>: interpolate</li>
<li><code>Ctrl+H</code>: humanize</li>
<li><code>Ctrl+B</code>: batch apply</li>
<li><code>Ctrl+T</code>: apply template</li>
<li><code>Ctrl+Up / Ctrl+Down</code>: transpose +1 / -1 semitone</li>
<li><code>Ctrl+Shift+Up / Ctrl+Shift+Down</code>: transpose +12 / -12</li>
<li><code>Ctrl+Delete</code>: clear current pattern</li>
<li><code>Insert</code>: insert row</li>
<li><code>Shift+Delete</code>: delete row</li>
<li><code>Delete</code>: clear cell</li>
<li><code>Backspace</code>: note-off then step advance</li>
<li><code>Enter</code>: open dialog for active field</li>
<li><code>0..9 / A..F</code>: hex entry for In/Vo/FX fields</li>
</ul>

<h2>Selection and movement</h2>
<ul>
<li><code>Shift + Arrows</code>: extend selection</li>
<li><code>Shift + PageUp/PageDown</code>: extend by 16 rows</li>
<li><code>Shift + Home/End</code>: extend to pattern boundaries</li>
<li><code>Shift + Click</code>: extend selection to clicked location</li>
<li><code>Double-click</code>: select full channel area</li>
<li><code>Ctrl+A</code>: select channel range</li>
<li><code>Ctrl+Shift+A</code>: select all channels</li>
<li><code>Numpad + / -</code>: octave up/down</li>
<li><code>Numpad * / /</code>: step up/down</li>
<li><code>Right-click</code>: context menu</li>
</ul>

<h2>Note entry reminder</h2>
<p>Piano-like key mapping depends on AZERTY/QWERTY mode and octave setting. See Tracker Keyboard topic for full map.</p>
)");

    case 12:
        return QStringLiteral(R"(
<h1>FAQ / Troubleshooting</h1>
<h2>I press keys but nothing is written</h2>
<ul>
<li>Enable <b>REC</b> mode</li>
<li>Place cursor on the Note sub-column</li>
<li>Check keyboard layout (QWERTY/AZERTY)</li>
<li>Verify octave and step are sensible</li>
</ul>

<h2>No sound?</h2>
<ul>
<li>Check mute/solo state</li>
<li>Check instrument assignment in cells</li>
<li>Verify attenuation is not muted (too high)</li>
<li>Confirm REC/edit state and channel focus</li>
<li>Remember note-entry preview is short and not full playback</li>
<li>Press Play/Space to confirm full playback, not only note preview</li>
</ul>

<h2>I changed language but UI looks unchanged</h2>
<p>Language is applied at startup. Reopen app from startup dialog and select FR/EN again.</p>
<p>The selected language is kept for next launches once applied.</p>

<h2>Unexpected playback?</h2>
<ul>
<li>Review FX commands on active rows</li>
<li>Check TPR and song order flow</li>
<li>Use runtime debug output for row-level inspection</li>
</ul>

<h2>Export mismatch?</h2>
<p>Re-check export mode (Pre-baked vs Hybrid), then validate warning logs and rerun export.</p>

<h2>Tracker feels too rigid?</h2>
<ul>
<li>Use humanize on attenuation</li>
<li>Use note delay/cut effects for groove variation</li>
<li>Use loop selection to fine-tune short passages</li>
</ul>

<h2>MIDI import looks odd?</h2>
<ul>
<li>Simplify dense/polyphonic passages before import</li>
<li>Recheck channel assignment after import</li>
<li>Adjust attenuation and instruments after auto-mapping</li>
</ul>

<h2>Cursor does not move after note entry</h2>
<p>Check <b>Step</b>. If Step is 0, cursor intentionally stays on the same row.</p>

<h2>I only hear one channel</h2>
<p>Most likely a Solo state is active. Disable Solo buttons and re-check F1..F4 mute toggles.</p>

<h2>Pitch sounds too low or too high</h2>
<p>Adjust octave and verify note entry lane. Also check for active pitch FX (1xx/2xx/3xx/4xx).</p>

<h2>How do I create fade-out?</h2>
<p>Use Vo interpolation from 0 to F on a selected range, or host fade command depending on workflow.</p>
<ol>
<li>Set <code>0</code> on start row (Vo field)</li>
<li>Set <code>F</code> on end row</li>
<li>Select the full range (visible highlight)</li>
<li>Press <code>Ctrl+I</code> for interpolation</li>
</ol>

<h2>How do I create drum/percussion?</h2>
<p>Use noise lane N and choose periodic/white modes (P.* / W.*), then shape with cut/delay and attenuation.</p>
<p>Quick starter beat: place kick-like periodic noise on rows 00/08 and short white-noise hats on 04/0C with <code>C02</code>.</p>

<h2>How do I copy one channel idea to another?</h2>
<ol>
<li>Select source channel region (or full channel)</li>
<li>Copy with <code>Ctrl+C</code></li>
<li>Move to destination channel start row</li>
<li>Paste with <code>Ctrl+V</code></li>
<li>Adjust instrument/attenuation where needed</li>
</ol>

<h2>I made a mistake. Can I recover?</h2>
<p>Yes. Use undo/redo (<code>Ctrl+Z</code>, <code>Ctrl+Y</code>) and keep regular song snapshots.</p>

<h2>How do I share my music?</h2>
<ul>
<li>Share <code>.ngps</code> for editable project song</li>
<li>Share <code>.ngpat</code> for single-pattern exchange</li>
<li>Share <code>.wav</code> for easy listening</li>
<li>Share exported <code>.c/.h</code> or <code>.inc</code> for runtime integration</li>
<li>Share text dump via copy-as-text for quick review</li>
</ul>

<h2>I see WARN export in log. Is this critical?</h2>
<p>Warnings are not always fatal, but they indicate risks. Fix warnings before final integration for safer parity.</p>

<h2>MIDI import result is messy. What should I do?</h2>
<ul>
<li>Simplify polyphony before import (NGPC has 3 tonal lanes + 1 noise lane)</li>
<li>Recheck TPR after import and adjust timing if needed</li>
<li>Clean note overlap and lane distribution manually in tracker</li>
</ul>

<h2>Song loop does not restart where expected</h2>
<p>Check loop marker position in order list and verify order entries are in intended sequence.</p>
<p>Set loop marker on the intended order entry, then retest in Song mode.</p>

<h2>Exported WAV is silent or too quiet</h2>
<ul>
<li>Check mute/solo state before rendering</li>
<li>Check attenuation/expression values and instrument behavior</li>
<li>Confirm note data exists in active song/order</li>
<li>In Song mode, verify order list is not empty</li>
</ul>

<h2>What is the output meter for?</h2>
<ul>
<li>It gives a quick visual estimate of signal level before export</li>
<li><b>CLIP</b> means numeric headroom is exceeded (distortion risk)</li>
<li>If clipping appears often, increase attenuation on notes/instruments/SFX layers</li>
</ul>
)");
    default:
        return QStringLiteral("<h1>Help</h1><p>Select a topic.</p>");
    }
}

// ============================================================
// Topics
// ============================================================

QString HelpTab::topic_intro() {
    return QStringLiteral(R"(
<h1>Bienvenue dans NGPC Sound Creator</h1>
<p>Ce logiciel vous permet de <b>composer de la musique</b> pour la console
<b>Neo Geo Pocket / Color</b>, meme si vous n'avez <b>jamais touche un logiciel de son</b> de votre vie.</p>

<h2>C'est quoi ce programme ?</h2>
<p>Imaginez un tableau Excel, mais au lieu de chiffres, vous mettez des <b>notes de musique</b>.
Le programme lit ce tableau de haut en bas et joue chaque note au bon moment.
C'est ce qu'on appelle un <b>"tracker"</b> - un type de logiciel musical invente dans les annees 80.</p>

<h2>Les 7 onglets</h2>
<table>
<tr><th>Onglet</th><th>A quoi ca sert</th></tr>
<tr><td><b>Projet</b></td><td>Creer/ouvrir un projet, gerer songs/SFX, export global</td></tr>
<tr><td><b>Player</b></td><td>Charger et ecouter des fichiers son existants</td></tr>
<tr><td><b>Tracker</b></td><td>Composer votre musique note par note (c'est l'onglet principal !)</td></tr>
<tr><td><b>Instruments</b></td><td>Creer et modifier les "sons" utilises par le Tracker</td></tr>
<tr><td><b>SFX Lab</b></td><td>Tester des bruits bruts (bips, bruits blancs...)</td></tr>
<tr><td><b>Debug</b></td><td>Verifier l'etat PSG/Z80 pour debug audio</td></tr>
<tr><td><b>Aide</b></td><td>Vous etes ici !</td></tr>
</table>

<h2>Nouveautes</h2>
<ul>
<li><b>Multi-pattern / Song mode</b> : composez avec plusieurs patterns enchaines</li>
<li><b>Export WAV</b> : rendez votre morceau en fichier audio</li>
<li><b>Import MIDI</b> : importez un fichier MIDI directement dans le tracker</li>
<li><b>Loop Selection</b> : bouclez sur une selection de lignes pendant le travail</li>
<li><b>Loop Preview (Instruments)</b> : rejouez le son en boucle pendant l'edition</li>
<li><b>Mode Projet + Edition libre</b> : workflow complet ou test rapide sans projet</li>
<li><b>ADSR</b> : enveloppe Attack/Decay/Sustain/Release par instrument</li>
<li><b>LFO pitch</b> : modulation instrument (triangle/square, rate, depth)</li>
<li><b>Effet Exy</b> : fade out global et changement de tempo en temps reel</li>
<li><b>Effet 4xx</b> : pitch bend (decalage de frequence persistant)</li>
<li><b>Effet Fxx</b> : expression (offset de volume persistant par canal)</li>
<li><b>Export hybride etendu</b> : opcodes ADSR/LFO, expression, fade, tempo, pitch bend</li>
<li><b>Export projet global</b> : songs + project_instruments.c + project_sfx.c</li>
<li><b>Export projet API</b> : manifest + table C centralisee des songs exportees</li>
</ul>

<h2>Par ou commencer ?</h2>
<p>Lisez les sections dans l'ordre, de haut en bas dans le menu a gauche.
En 10 minutes vous saurez composer votre premiere melodie !</p>

<h2>Demarrage et langue</h2>
<ul>
<li>Au lancement, une fenetre de demarrage vous propose : <b>Nouveau projet</b>, <b>Ouvrir projet</b> ou <b>Edition libre</b>.</li>
<li>La langue de l'interface se choisit dans cette meme fenetre (FR/EN).</li>
<li>Si vous changez la langue, l'application redemarre automatiquement pour appliquer la langue partout.</li>
<li>La version actuelle de l'onglet <b>Aide</b> est maintenue en francais en priorite.</li>
</ul>

<h2>Le Neo Geo Pocket, c'est quoi ?</h2>
<p>C'est une console portable de SNK (1998). Sa puce sonore s'appelle le <b>T6W28</b>.
Elle peut jouer <b>4 sons en meme temps</b> :</p>
<ul>
<li><b>3 canaux "Tone"</b> (T0, T1, T2) : des sons musicaux (notes de piano, basses, melodies...)</li>
<li><b>1 canal "Noise"</b> (N) : des bruits (percussions, explosions, bruits de fond...)</li>
</ul>
<p>C'est peu, mais c'est suffisant pour faire de la super musique retro !</p>
)");
}

QString HelpTab::topic_concepts() {
    return QStringLiteral(R"(
<h1>Concepts de base</h1>
<p>Pas besoin de savoir lire une partition. Voici juste ce qu'il faut savoir :</p>

<h2>Les notes</h2>
<p>En musique, il y a <b>12 notes</b> qui se repetent en boucle, de grave a aigu.
En anglais (c'est ce qu'affiche le tracker) :</p>
<pre>  C   C#   D   D#   E   F   F#   G   G#   A   A#   B
  Do  Do#  Re  Re#  Mi  Fa  Fa#  Sol Sol#  La  La#  Si</pre>

<p>Les notes avec un <b>#</b> (diese) sont les "touches noires" du piano.
Si vous ne connaissez pas la musique, ignorez-les au debut et utilisez juste
C, D, E, F, G, A, B.</p>

<h2>Les octaves</h2>
<p>La meme note peut etre <b>grave</b> ou <b>aigue</b>. C'est l'<b>octave</b> qui change :</p>
<ul>
<li><b>Octave 2</b> : tres grave (basse)</li>
<li><b>Octave 3</b> : grave</li>
<li><b>Octave 4</b> : milieu (voix humaine) - c'est le defaut</li>
<li><b>Octave 5</b> : aigu</li>
<li><b>Octave 6-7</b> : tres aigu</li>
</ul>
<p>Dans le tracker, <code>C-4</code> veut dire "Do a l'octave 4".</p>

<h2>Le volume (Attenuation)</h2>
<p>Le NGPC ne parle pas de "volume" mais d'<b>attenuation</b> (combien on baisse le son) :</p>
<table>
<tr><th>Valeur</th><th>Signification</th></tr>
<tr><td><b>0</b></td><td>Volume MAXIMUM (le plus fort possible)</td></tr>
<tr><td><b>4</b></td><td>Moyen-fort</td></tr>
<tr><td><b>8</b></td><td>Moyen-faible</td></tr>
<tr><td><b>C</b></td><td>Faible</td></tr>
<tr><td><b>F</b></td><td>SILENCE total</td></tr>
<tr><td><b>-</b> (tiret)</td><td>Pas de valeur = c'est l'instrument qui decide</td></tr>
</table>
<p>Attention : c'est en <b>hexadecimal</b> (0-9 puis A-F), soit 16 niveaux au total.</p>

<h2>Les instruments</h2>
<p>Un instrument, c'est la "couleur" du son. La meme note Do peut sonner comme :
une cloche, un piano, un bip electronique, une basse... selon l'instrument.</p>
<p>Vous pouvez avoir <b>jusqu'a 128 instruments</b> (numerotes de 00 a 7F).
Chaque instrument definit :</p>
<ul>
<li><b>Enveloppe</b> : est-ce que le son diminue vite ou lentement ? (un piano vs un orgue)</li>
<li><b>Vibrato</b> : est-ce que le son tremble ? (comme une voix qui vibre)</li>
<li><b>Sweep</b> : est-ce que la note glisse vers le haut ou le bas ?</li>
</ul>

<h2>Le rythme</h2>
<p>Le tracker lit les notes de haut en bas, ligne par ligne. La vitesse est controlee par le <b>TPR</b> (Ticks Per Row) :</p>
<ul>
<li><b>TPR petit (4)</b> = rapide (~225 BPM)</li>
<li><b>TPR moyen (8)</b> = normal (~112 BPM) - c'est le defaut</li>
<li><b>TPR grand (12+)</b> = lent</li>
</ul>
<p>Les lignes du tracker sont regroupees par 4 (un "temps" musical).
Les lignes 00, 04, 08, 0C, 10... sont les <b>temps forts</b> - ils sont colores differemment pour vous aider.</p>
)");
}

QString HelpTab::topic_instruments() {
    return QStringLiteral(R"(
<h1>Les Instruments</h1>
<p>Avant de composer, il faut preparer au moins un instrument.
Allez dans l'onglet <b>Instruments</b>.</p>

<h2>L'interface</h2>
<p>A gauche : la <b>liste des instruments</b> (00 a 7F).<br>
A droite : les <b>reglages</b> de l'instrument selectionne.</p>

<h2>Gestion des slots</h2>
<ul>
<li>Le label <b>Code tracker</b> affiche l'id du slot (<code>0x00..0x7F</code>) a utiliser dans la colonne <b>In</b></li>
<li><b>Nom + Appliquer Nom</b> : renomme le slot selectionne sans changer son code tracker</li>
<li><b>Appliquer Factory -&gt; Slot</b> : copie un preset factory dans le slot courant (ecrasement)</li>
<li><b>Reinit Slot</b> : remet le slot courant a sa valeur par defaut</li>
<li><b>Reset Banque Factory</b> : remet toute la banque d'instruments aux presets factory</li>
</ul>

<h2>Reglages essentiels</h2>
<table>
<tr><th>Reglage</th><th>C'est quoi</th><th>Valeur pour debuter</th></tr>
<tr><td><b>Attenuation</b></td><td>Volume de depart</td><td>0 (maximum)</td></tr>
<tr><td><b>Mode</b></td><td>0 = son normal (tone), 1 = bruit (noise)</td><td>0</td></tr>
<tr><td><b>Envelope</b></td><td>Comment le volume evolue dans le temps</td><td>Cocher + choisir "Decay"</td></tr>
</table>

<h2>L'enveloppe, c'est quoi ?</h2>
<p>Quand vous frappez une touche de piano, le son est fort au debut puis diminue.
C'est l'enveloppe. Sans enveloppe, le son joue a fond en continu (comme un orgue).</p>
<p>Courbes utiles pour debuter :</p>
<ul>
<li><b>Decay court</b> : son bref (bip, percussion melodique)</li>
<li><b>Decay moyen</b> : son normal (melodie)</li>
<li><b>Decay long</b> : son tenu (basse, pad)</li>
</ul>

<h2>Le vibrato</h2>
<p>Ca fait "trembler" la note (la hauteur oscille legerement).
Cochez la case et reglez :</p>
<ul>
<li><b>Depth</b> : amplitude du tremblement (1=subtil, 4=prononce)</li>
<li><b>Speed</b> : vitesse du tremblement</li>
<li><b>Delay</b> : nombre de frames avant que le vibrato commence (utile pour que la note soit stable au debut)</li>
</ul>

<h2>L'enveloppe ADSR</h2>
<p>En plus de l'enveloppe simple (decay lineaire), vous pouvez activer une enveloppe <b>ADSR</b>
(Attack / Decay / Sustain / Release) pour un controle plus fin du volume dans le temps :</p>
<table>
<tr><th>Phase</th><th>Description</th></tr>
<tr><td><b>Attack</b></td><td>Le son monte du silence au volume max. Valeur = vitesse de montee (0 = instantane).</td></tr>
<tr><td><b>Decay</b></td><td>Le son descend du max vers le niveau Sustain. Valeur = vitesse de descente.</td></tr>
<tr><td><b>Sustain</b></td><td>Niveau de volume maintenu tant que la note joue. 0 = fort, 15 = silence.</td></tr>
<tr><td><b>Release</b></td><td>Quand la note s'arrete (note-off), le son descend vers le silence. Valeur = vitesse.</td></tr>
</table>
<p>L'ADSR <b>remplace</b> l'enveloppe lineaire quand elle est activee. Si les 4 valeurs sont a 0, l'ADSR est desactivee
et l'enveloppe lineaire classique reprend.</p>
<p>Cochez la case <b>ADSR</b> dans l'onglet Instruments et reglez les 4 curseurs.</p>

<h2>Le sweep</h2>
<p>La note glisse progressivement vers le haut ou le bas.
Utile pour des effets "laser", "sirene", etc.</p>

<h2>Suggestions d'instruments</h2>
<table>
<tr><th>Slot</th><th>Usage</th><th>Reglages</th></tr>
<tr><td><b>00</b></td><td>Melodie principale</td><td>Envelope decay moyen, pas de vibrato</td></tr>
<tr><td><b>01</b></td><td>Basse</td><td>Envelope decay long, pas de vibrato</td></tr>
<tr><td><b>02</b></td><td>Arpege / accords</td><td>Envelope decay court</td></tr>
<tr><td><b>03</b></td><td>Lead avec vibrato</td><td>Envelope decay moyen + vibrato (depth 2, delay 10)</td></tr>
<tr><td><b>04</b></td><td>Effet "zap"</td><td>Sweep actif vers le bas</td></tr>
</table>

<h2>Tester un instrument</h2>
<p>Utilisez le bouton <b>Preview</b> dans l'onglet Instruments pour entendre le resultat.
Changez la note de preview pour tester a differentes hauteurs.</p>

<h2>Loop Preview (lecture en boucle)</h2>
<p>Quand vous cherchez un son, revenir cliquer sur Play apres chaque modification est fastidieux.
Le bouton <b>Loop</b> (a cote de Play/Stop) resout ce probleme :</p>
<ol>
<li>Cliquez sur <b>Loop</b> (il reste enfonce)</li>
<li>L'instrument se joue en boucle automatiquement</li>
<li>Modifiez les parametres (enveloppe, vibrato, sweep...) : chaque repetition prend en compte vos changements</li>
<li>Cliquez sur <b>Stop</b> ou decochez <b>Loop</b> pour arreter</li>
</ol>
<p>Ideal pour affiner un son de maniere interactive !</p>
)");
}

QString HelpTab::topic_tracker_basics() {
    return QStringLiteral(R"(
<h1>Tracker : les bases</h1>

<h2>La grille</h2>
<p>L'onglet Tracker affiche une grande grille avec <b>4 colonnes</b> (une par canal) :</p>
<table>
<tr><th>Colonne</th><th>Nom</th><th>Role</th></tr>
<tr><td><b>T0</b></td><td>Tone 0</td><td>Canal melodie principal</td></tr>
<tr><td><b>T1</b></td><td>Tone 1</td><td>2e canal melodie (harmonie, basse...)</td></tr>
<tr><td><b>T2</b></td><td>Tone 2</td><td>3e canal melodie</td></tr>
<tr><td><b>N</b></td><td>Noise</td><td>Canal bruit (percussions)</td></tr>
</table>

<h2>Chaque cellule</h2>
<p>Chaque case de la grille contient 3 informations :</p>
<pre>  C-4 00 -
  ^^^ ^^ ^
   |   |  |
   |   |  Attenuation (volume) : 0-F ou - (defaut instrument)
   |   Instrument : 00 a 7F
   Note : C-4 = Do octave 4, ou "---" (vide) ou "OFF" (couper le son)</pre>

<h2>Le curseur</h2>
<p>Le curseur est le rectangle jaune (ou rouge en mode REC). Il indique ou vous etes.
Vous pouvez le deplacer avec les fleches ou en cliquant.</p>
<p>Le curseur a une <b>sous-position</b> dans la cellule :</p>
<ul>
<li>Position 1 : <b>Note</b> (la ou vous tapez les notes au clavier)</li>
<li>Position 2 : <b>Instrument</b> (la ou vous tapez le numero d'instrument en hex)</li>
<li>Position 3 : <b>Attenuation</b> (la ou vous tapez le volume en hex)</li>
</ul>
<p>Fleche gauche/droite = changer de sous-position. Tab = changer de canal.</p>

<h2>La barre d'outils</h2>
<table>
<tr><th>Element</th><th>Role</th></tr>
<tr><td><b>Play / Stop</b></td><td>Lancer / arreter la lecture</td></tr>
<tr><td><b>REC</b></td><td>Mode enregistrement (rouge = actif). Quand c'est actif, les touches clavier ecrivent des notes</td></tr>
<tr><td><b>Follow</b></td><td>Le curseur suit la position de lecture</td></tr>
<tr><td><b>Loop Sel</b></td><td>Boucler la lecture sur les lignes selectionnees</td></tr>
<tr><td><b>Song</b></td><td>Lancer la lecture en mode Song (tous les patterns enchaines)</td></tr>
<tr><td><b>Clear</b></td><td>Effacer tout le pattern</td></tr>
<tr><td><b>TPR</b></td><td>Vitesse de lecture (Ticks Per Row)</td></tr>
<tr><td><b>Oct</b></td><td>Octave de base pour le clavier</td></tr>
<tr><td><b>Step</b></td><td>De combien de lignes le curseur avance apres avoir tape une note</td></tr>
<tr><td><b>Len</b></td><td>Longueur du pattern (nombre de lignes, 16-128)</td></tr>
<tr><td><b>Pat</b></td><td>Numero du pattern actif (pour multi-pattern)</td></tr>
<tr><td><b>+Pat / Clone</b></td><td>Ajouter un pattern vide / dupliquer le pattern actif</td></tr>
<tr><td><b>KB</b></td><td>Disposition clavier (AZERTY ou QWERTY)</td></tr>
<tr><td><b>Save / Load</b></td><td>Sauvegarder / charger un fichier pattern (.ngpat)</td></tr>
<tr><td><b>Save Song / Load Song</b></td><td>Sauvegarder / charger un morceau complet (.ngps)</td></tr>
<tr><td><b>Pre-baked / Hybride</b></td><td>Mode d'export : Pre-baked (fidelite parfaite) ou Hybride (opcodes instrument, compact)</td></tr>
<tr><td><b>Export C</b></td><td>Exporter en code C (pour programmer le NGPC)</td></tr>
<tr><td><b>ASM</b></td><td>Exporter en assembleur .inc (toolchain SNK)</td></tr>
<tr><td><b>WAV</b></td><td>Exporter le morceau en fichier audio WAV</td></tr>
<tr><td><b>MIDI</b></td><td>Importer un fichier MIDI dans le tracker</td></tr>
</table>

<h2>La liste d'ordre (Order list)</h2>
<p>A gauche de la grille se trouve la <b>liste d'ordre</b>. Elle definit dans quel ordre
les patterns sont joues en mode Song :</p>
<ul>
<li>Chaque entree = un numero de pattern a jouer</li>
<li><b>L&gt;</b> devant une entree = point de boucle (la lecture revient ici apres le dernier pattern)</li>
<li><b>[+]</b> : ajouter une entree a la liste</li>
<li><b>[x]</b> : supprimer l'entree selectionnee</li>
<li><b>[^] [v]</b> : deplacer l'entree vers le haut / bas</li>
<li><b>[Loop]</b> : definir le point de boucle sur l'entree selectionnee</li>
<li>Double-clic sur une entree = editer le pattern correspondant</li>
</ul>

<h2>Le mode REC</h2>
<p>Le bouton <b>REC</b> est TRES important :</p>
<ul>
<li><b>REC actif (rouge)</b> : quand vous appuyez sur une touche du clavier, ca ecrit une note dans la grille</li>
<li><b>REC inactif</b> : les touches ne font rien (mode "navigation seule")</li>
</ul>
<p>Si vous tapez des touches et rien ne se passe, verifiez que REC est actif !</p>
)");
}

QString HelpTab::topic_tracker_keyboard() {
    return QStringLiteral(R"(
<h1>Tracker : le clavier</h1>
<p>Votre clavier d'ordinateur sert de <b>piano</b>. Deux rangees de touches = deux octaves.</p>

<h2>Disposition AZERTY (par defaut)</h2>
<p><b>Rangee du bas</b> = octave basse (la valeur affichee dans "Oct") :</p>
<pre>  W = Do (C)      V = Fa (F)      N = Sol (G)
  X = Do# (C#)    G = Fa# (F#)    H = Sol# (G#)
  C = Re (D)      B = Fa (F)      , = La (A)
  D = Re# (D#)                    J = La# (A#)
  V = Mi (E)                      ; = Si (B)</pre>

<p><b>Rangee du haut</b> = octave suivante (Oct + 1) :</p>
<pre>  A = Do (C)      R = Fa (F)      Y = La (A)
  2 = Do# (C#)    5 = Fa# (F#)    7 = La# (A#)
  Z = Re (D)      T = Sol (G)     U = Si (B)
  3 = Re# (D#)    6 = Sol# (G#)
  E = Mi (E)</pre>

<h2>Disposition QWERTY</h2>
<p><b>Rangee du bas</b> :</p>
<pre>  Z = Do    S = Do#    X = Re    D = Re#    C = Mi
  V = Fa    G = Fa#    B = Sol   H = Sol#   N = La
  J = La#   M = Si</pre>

<p><b>Rangee du haut</b> :</p>
<pre>  Q = Do    2 = Do#    W = Re    3 = Re#    E = Mi
  R = Fa    5 = Fa#    T = Sol   6 = Sol#   Y = La
  7 = La#   U = Si</pre>

<h2>Astuce : la reference clavier</h2>
<p>Pas besoin de tout retenir ! La <b>barre de reference</b> sous la barre d'outils
affiche en temps reel quel touche = quelle note, selon votre octave et layout actuels.</p>

<h2>Changer d'octave</h2>
<p>Utilisez la spinbox <b>Oct</b> dans la barre d'outils, ou les raccourcis :</p>
<ul>
<li><b>Numpad +</b> : monter d'une octave</li>
<li><b>Numpad -</b> : descendre d'une octave</li>
</ul>

<h2>Le "Step" (pas d'edition)</h2>
<p>Apres avoir tape une note, le curseur avance automatiquement de <b>Step</b> lignes.
Exemples :</p>
<ul>
<li><b>Step = 0</b> : le curseur ne bouge pas (pour modifier une note existante)</li>
<li><b>Step = 1</b> : avance d'une ligne (pour entrer des notes continues)</li>
<li><b>Step = 2</b> : avance de 2 lignes (notes toutes les 2 lignes = croches)</li>
<li><b>Step = 4</b> : avance de 4 lignes (une note par temps = noires)</li>
</ul>
<p>Raccourcis : <b>Numpad *</b> = step +1, <b>Numpad /</b> = step -1</p>
)");
}

QString HelpTab::topic_tracker_edit() {
    return QStringLiteral(R"(
<h1>Tracker : edition</h1>

<h2>Entrer des notes</h2>
<ol>
<li>Verifiez que <b>REC</b> est actif (rouge)</li>
<li>Placez le curseur sur la sous-colonne <b>Note</b> (la premiere)</li>
<li>Appuyez sur une touche du clavier (voir section "Le clavier")</li>
<li>La note s'affiche (ex: <code>C-4</code>) et le curseur avance de Step lignes</li>
</ol>

<h2>Couper le son (Note-Off)</h2>
<p>Pour arreter une note qui joue, appuyez sur <b>Backspace</b>.
Ca insere <code>OFF</code> dans la cellule. Sans note-off, la note continue de jouer
jusqu'a ce que l'instrument finisse naturellement (si l'enveloppe descend a 0).</p>

<h2>Effacer une cellule</h2>
<p><b>Delete</b> efface la cellule courante et avance de Step lignes.</p>

<h2>Changer l'instrument</h2>
<ol>
<li>Naviguez vers la sous-colonne <b>Inst</b> (fleche droite depuis la note)</li>
<li>Tapez un chiffre hex (0-9, A-F) pour choisir l'instrument</li>
</ol>
<p>Note : il faut d'abord qu'il y ait une note dans la cellule. Vous ne pouvez pas
mettre un instrument sur une cellule vide.</p>

<h2>Changer le volume</h2>
<ol>
<li>Naviguez vers la sous-colonne <b>Attn</b> (fleche droite x2 depuis la note)</li>
<li>Tapez un chiffre hex (0 = fort, F = silence)</li>
</ol>
<p>La cellule affiche une petite barre de volume coloree (vert = fort, rouge = faible).</p>

<h2>Selection</h2>
<p>Pour selectionner un bloc de cellules :</p>
<ul>
<li><b>Shift + fleches haut/bas</b> : etendre la selection ligne par ligne</li>
<li><b>Shift + clic</b> : selectionner jusqu'au point clique</li>
<li><b>Double-clic</b> : selectionner tout le canal</li>
<li><b>Ctrl+A</b> : selectionner tout le canal courant</li>
<li><b>Ctrl+Shift+A</b> : selectionner tous les canaux</li>
<li><b>Echap</b> : annuler la selection</li>
</ul>

<h2>Copier / Coller / Couper</h2>
<ul>
<li><b>Ctrl+C</b> : copier la selection</li>
<li><b>Ctrl+X</b> : couper la selection</li>
<li><b>Ctrl+V</b> : coller a la position du curseur</li>
</ul>
<p>Astuce : <b>Ctrl+Shift+C</b> copie le pattern en texte lisible dans le presse-papiers
(utile pour partager sur un forum ou un chat).</p>

<h2>Annuler / Refaire</h2>
<ul>
<li><b>Ctrl+Z</b> : annuler (jusqu'a 100 niveaux !)</li>
<li><b>Ctrl+Y</b> ou <b>Ctrl+Shift+Z</b> : refaire</li>
</ul>

<h2>Inserer / Supprimer des lignes</h2>
<ul>
<li><b>Insert</b> : inserer une ligne vide a la position courante (decale tout vers le bas)</li>
<li><b>Shift+Delete</b> : supprimer la ligne courante (decale tout vers le haut)</li>
<li><b>Ctrl+D</b> : dupliquer la ligne courante</li>
</ul>
)");
}

QString HelpTab::topic_tracker_advanced() {
    return QStringLiteral(R"(
<h1>Tracker : fonctions avancees</h1>

<h2>Transposer</h2>
<p>Selectionez un bloc de notes, puis :</p>
<ul>
<li><b>Ctrl+Fleche haut</b> : monter d'un demi-ton</li>
<li><b>Ctrl+Fleche bas</b> : descendre d'un demi-ton</li>
<li><b>Ctrl+Shift+Fleche haut</b> : monter d'une octave (+12 demi-tons)</li>
<li><b>Ctrl+Shift+Fleche bas</b> : descendre d'une octave</li>
</ul>

<h2>Interpolation contextuelle (Ctrl+I)</h2>
<p>L'interpolation remplit automatiquement des valeurs entre deux points selon la colonne active :</p>
<ul>
<li><b>Inst</b> : interpolation d'ID instrument</li>
<li><b>Vol/Attn</b> : interpolation d'attenuation (fade-in / fade-out)</li>
<li><b>FX param</b> : interpolation de parametre effet (00..FF)</li>
</ul>

<h3>Important : comment ca fonctionne</h3>
<ul>
<li>L'interpolation agit sur la <b>colonne active</b> (Inst / Vol / FX param)</li>
<li>Elle agit sur la selection courante (range ou selection discrete), canal par canal</li>
<li>Il faut une <b>selection active</b> (surlignage bleu visible dans la grille)</li>
</ul>

<h3>Pas-a-pas : faire un fade-out</h3>
<ol>
<li><b>Etape 1</b> : Placez le curseur sur le canal souhaite (ex: T0)</li>
<li><b>Etape 2</b> : Allez a la ligne de depart (ex: ligne 00)</li>
<li><b>Etape 3</b> : Naviguez jusqu'a la sous-colonne <b>Vol</b> (fleche droite x2 depuis Note)</li>
<li><b>Etape 4</b> : Tapez <b>0</b> (volume maximum) - la cellule affiche "0"</li>
<li><b>Etape 5</b> : Descendez a la ligne de fin (ex: ligne 0F)</li>
<li><b>Etape 6</b> : Tapez <b>F</b> (silence) - la cellule affiche "F"</li>
<li><b>Etape 7</b> : Selectionnez de la ligne 00 a 0F avec <b>Shift + fleches haut</b>
    (ou placez-vous sur ligne 00 puis <b>Shift + fleche bas</b> jusqu'a 0F)</li>
<li><b>Etape 8</b> : Appuyez sur <b>Ctrl+I</b></li>
</ol>

<h3>Resultat visuel</h3>
<pre>Avant Ctrl+I :           Apres Ctrl+I :
Row  Note  Vol            Row  Note  Vol
00   C-4   0              00   C-4   0  (fort)
01   D-4   -              01   D-4   1
02   E-4   -              02   E-4   2
03   F-4   -              03   F-4   3
04   G-4   -              04   G-4   4
...                       ...
0E   A-4   -              0E   A-4   E
0F   G-4   F              0F   G-4   F  (silence)</pre>
<p>Toutes les lignes entre le debut et la fin sont remplies progressivement.</p>

<h3>Cas particuliers</h3>
<ul>
<li>Si la premiere ligne de la selection n'a <b>pas de valeur Vol</b> (tiret -), elle est traitee comme <b>0</b> (fort)</li>
<li>Si la derniere ligne n'a <b>pas de valeur Vol</b>, elle est traitee comme <b>F</b> (silence)</li>
<li>Donc si vous selectionnez une zone sans aucune valeur Vol et faites Ctrl+I, vous obtenez un fade-out de 0 a F</li>
<li>Il faut au moins <b>2 lignes</b> par canal dans la selection</li>
</ul>

<h3>Astuce : fade-in</h3>
<p>Inversez les valeurs : mettez <b>F</b> (silence) en haut et <b>0</b> (fort) en bas,
puis selectionnez et Ctrl+I. Le son apparait progressivement !</p>

<h3>Erreur courante</h3>
<p>Si rien ne se passe quand vous faites Ctrl+I, verifiez que :</p>
<ul>
<li>Vous avez bien une <b>selection active</b> (zone surlignee en bleu). Utilisez Shift+fleches pour selectionner.</li>
<li>Le <b>curseur est sur la bonne colonne</b> (Inst, Vol ou FX param).</li>
</ul>

<h2>Humanize attenuation (Ctrl+H)</h2>
<p><b>Ctrl+H</b> applique une legere variation aleatoire d'attenuation sur la selection
(ou la cellule courante), utile pour casser un rendu trop "robotique".</p>
<ul>
<li>Vous choisissez la plage aleatoire (<code>+/-1</code> a <code>+/-4</code>)</li>
<li>Vous choisissez la probabilite d'application (1% a 100%)</li>
<li>Sur une note en mode AUTO (attn "-"), le tool materialise une attenuation explicite si necessaire</li>
</ul>

<h2>Batch apply (Ctrl+B)</h2>
<p><b>Ctrl+B</b> applique une valeur fixe sur toute la selection, selon la colonne active :</p>
<ul>
<li>Colonne <b>Inst</b> : applique un ID instrument (0..127) sur les notes</li>
<li>Colonne <b>Vol/Attn</b> : applique une attenuation fixe (-1=AUTO, 0..15)</li>
<li>Colonne <b>FX</b> : applique une commande FX (0..F)</li>
<li>Colonne <b>FX param</b> : applique un parametre FX (00..FF)</li>
</ul>
<p>Pratique pour uniformiser rapidement une zone avant de refaire une interpolation/humanize.</p>

<h2>Templates de pattern (Ctrl+T)</h2>
<p>Le tracker propose des templates rapides (<b>Kick</b>, <b>Snare</b>, <b>Hi-Hat</b>, <b>Bass</b>, <b>Arp</b>, <b>Starter all channels</b>,
<b>Kick + Hat Groove</b>, <b>Snare Fill 16ths</b>, <b>Bass + Arp Duo</b>, <b>Full Groove Loop</b>).</p>
<ul>
<li>Selectionnez un template dans la barre d'outils puis cliquez <b>Apply Tpl</b> (ou <b>Ctrl+T</b>)</li>
<li>Le template s'applique a la selection (si active), sinon au pattern complet</li>
<li>Vous pouvez choisir <b>Clear target rows</b> pour repartir proprement, ou fusionner avec l'existant</li>
</ul>

<h2>Muter / Solo</h2>
<p>Pour ecouter un seul canal ou couper un canal :</p>
<ul>
<li><b>F1 a F4</b> : muter/demuter les canaux T0, T1, T2, Noise</li>
<li>Cliquer sur le <b>nom du canal</b> en haut de la grille = muter</li>
<li>Boutons <b>Mute</b> dans la barre sous les outils</li>
<li>Boutons <b>Solo</b> : n'ecouter qu'un seul canal (les autres sont mutes)</li>
</ul>
<p>Les canaux mutes ont un overlay gris fonce sur la grille.</p>

<h2>Le canal Noise</h2>
<p>Le canal Noise (colonne N) est special : il ne joue pas de notes musicales
mais des <b>bruits</b>. La puce T6W28 offre <b>8 configurations</b> de bruit,
controlees par 2 parametres :</p>
<ul>
<li><b>Type</b> : <code>P</code> (Periodic / periodique) ou <code>W</code> (White / bruit blanc)</li>
<li><b>Rate</b> : <code>H</code> (High / rapide), <code>M</code> (Medium), <code>L</code> (Low / lent), <code>T</code> (Tone2 / suit la frequence du canal T2)</li>
</ul>

<h3>Les 8 bruits disponibles</h3>
<table>
<tr><th>Affichage</th><th>Type</th><th>Rate</th><th>Usage typique</th></tr>
<tr><td><b>P.H</b></td><td>Periodic</td><td>High</td><td>Kick, tom grave</td></tr>
<tr><td><b>P.M</b></td><td>Periodic</td><td>Medium</td><td>Tom moyen</td></tr>
<tr><td><b>P.L</b></td><td>Periodic</td><td>Low</td><td>Bourdonnement grave</td></tr>
<tr><td><b>P.T</b></td><td>Periodic</td><td>Tone 2</td><td>Suit le canal T2</td></tr>
<tr><td><b>W.H</b></td><td>White</td><td>High</td><td>Hi-hat, crash</td></tr>
<tr><td><b>W.M</b></td><td>White</td><td>Medium</td><td>Snare, caisse claire</td></tr>
<tr><td><b>W.L</b></td><td>White</td><td>Low</td><td>Souffle, vent</td></tr>
<tr><td><b>W.T</b></td><td>White</td><td>Tone 2</td><td>Suit le canal T2</td></tr>
</table>

<h3>Saisie clavier (canal Noise)</h3>
<p>Quand le curseur est sur le canal N en mode REC, les touches sont differentes :</p>
<table>
<tr><th>AZERTY</th><th>QWERTY</th><th>Bruit</th></tr>
<tr><td>W</td><td>Z</td><td>P.H (Periodic High)</td></tr>
<tr><td>S</td><td>S</td><td>P.M (Periodic Medium)</td></tr>
<tr><td>X</td><td>X</td><td>P.L (Periodic Low)</td></tr>
<tr><td>D</td><td>D</td><td>P.T (Periodic Tone2)</td></tr>
<tr><td>A</td><td>Q</td><td>W.H (White High)</td></tr>
<tr><td>Z</td><td>2</td><td>W.M (White Medium)</td></tr>
<tr><td>E</td><td>W</td><td>W.L (White Low)</td></tr>
<tr><td>R</td><td>3</td><td>W.T (White Tone2)</td></tr>
</table>
<p>Astuce : <b>double-clic</b> sur une cellule noise ouvre un dialogue pour choisir visuellement.</p>

<h2>Le menu contextuel (clic droit)</h2>
<p>Un clic droit n'importe ou dans la grille ouvre un menu avec toutes les actions :
couper, copier, coller, inserer, supprimer, transposer, interpoler, sauvegarder...</p>

<h2>Multi-pattern (Song mode)</h2>
<p>Un morceau complet est rarement contenu dans un seul pattern de 128 lignes.
Le mode <b>multi-pattern</b> permet de composer avec plusieurs patterns :</p>

<h3>Gestion des patterns</h3>
<ul>
<li><b>Pat (spinbox)</b> : selectionne le pattern actif pour l'edition</li>
<li><b>+Pat</b> : cree un nouveau pattern vide</li>
<li><b>Clone</b> : duplique le pattern actif (utile pour faire des variations)</li>
</ul>
<p>Vous pouvez avoir jusqu'a <b>64 patterns</b>.</p>

<h3>La liste d'ordre</h3>
<p>La liste d'ordre (a gauche de la grille) definit l'enchainement des patterns :</p>
<pre>  00: Pat 00 (64)      <- joue le pattern 00 (64 lignes)
  L>01: Pat 01 (32)    <- point de boucle, joue le pattern 01
  02: Pat 00 (64)      <- rejoue le pattern 00</pre>
<p>Quand la lecture en mode Song atteint la fin de la liste, elle revient au point de boucle <b>L&gt;</b>.</p>

<h3>Workflow typique</h3>
<ol>
<li>Composez un pattern d'intro (Pat 00)</li>
<li>Cliquez <b>+Pat</b> pour creer Pat 01 (couplet)</li>
<li>Cliquez <b>Clone</b> pour dupliquer Pat 01 en Pat 02, modifiez-le (refrain)</li>
<li>Configurez l'ordre : 00, 01, 02, 01, 02</li>
<li>Placez le point de boucle sur l'entree 1 (pour que l'intro ne soit jouee qu'une fois)</li>
<li>Lancez en mode <b>Song</b> pour ecouter le morceau entier</li>
</ol>

<h2>Les couleurs dans la grille</h2>
<ul>
<li>Chaque <b>instrument</b> a sa propre couleur (00 = blanc, 01 = bleu, 02 = orange, etc.)</li>
<li>Les <b>lignes de beat</b> sont plus claires (toutes les 4 lignes) et encore plus marquees toutes les 16</li>
<li>La <b>barre de volume</b> dans la colonne Attn va du vert (fort) au rouge (faible)</li>
<li>Le <b>curseur</b> est jaune en mode normal, rouge en mode REC</li>
<li>La <b>barre rouge</b> a gauche indique que le mode REC est actif</li>
</ul>
)");
}

QString HelpTab::topic_tracker_effects() {
    return QStringLiteral(R"(
<h1>Tracker : Effets par cellule</h1>

<p>Les <b>effets</b> permettent de modifier le son <b>en temps reel pendant la lecture</b>.
Chaque cellule du tracker a maintenant une colonne FX supplementaire (a droite de l'attenuation).</p>

<h2>Comment ca marche ?</h2>
<p>Un effet se compose de <b>3 chiffres hexadecimaux</b> : <code>Xxy</code> ou <code>Xxx</code></p>
<ul>
<li>Le <b>premier chiffre</b> (X) = le type d'effet (0-F)</li>
<li>Les <b>deux suivants</b> (xy ou xx) = le parametre de l'effet</li>
</ul>

<p>Pour entrer un effet :</p>
<ol>
<li>Naviguez avec les fleches droite jusqu'a la colonne FX (apres l'attenuation)</li>
<li>Tapez le chiffre de commande (0-9, A-F) - le curseur avance automatiquement</li>
<li>Tapez les 2 chiffres du parametre</li>
</ol>

<h2>Liste des effets</h2>
<table>
<tr><th>Code</th><th>Nom</th><th>Parametre</th><th>Description</th></tr>
<tr><td><b>0xy</b></td><td>Arpege</td><td>x=+demi-tons 1, y=+demi-tons 2</td>
    <td>Fait alterner rapidement entre 3 notes : la note de base, +x demi-tons, +y demi-tons.
    Cree un effet d'accord ou de chip-tune classique.</td></tr>
<tr><td><b>1xx</b></td><td>Pitch slide up</td><td>xx=vitesse</td>
    <td>Le son monte progressivement (la frequence augmente). Plus xx est grand, plus ca monte vite.</td></tr>
<tr><td><b>2xx</b></td><td>Pitch slide down</td><td>xx=vitesse</td>
    <td>Le son descend progressivement. Plus xx est grand, plus ca descend vite.</td></tr>
<tr><td><b>3xx</b></td><td>Portamento</td><td>xx=vitesse</td>
    <td>Glissement continu d'une note a l'autre. La note precedente "glisse" vers la nouvelle note
    a la vitesse xx, au lieu de changer brusquement.</td></tr>
<tr><td><b>4xx</b></td><td>Pitch bend</td><td>xx=offset (signe)</td>
    <td>Decale la frequence de la note d'un offset fixe sur le diviseur.
    01-7F = son plus grave (diviseur +), 80-FF = son plus aigu (diviseur -), 00 = pas de bend.
    L'effet est <b>persistant</b> : il reste actif jusqu'au prochain 4xx.</td></tr>
<tr><td><b>Axy</b></td><td>Volume slide</td><td>x=monter, y=descendre</td>
    <td>Change le volume progressivement. Si x &gt; 0 : le volume monte (son plus fort).
    Si y &gt; 0 : le volume descend (son plus faible). Un seul des deux doit etre non-zero.</td></tr>
<tr><td><b>Bxx</b></td><td>Set speed</td><td>xx=TPR (1-32)</td>
    <td>Change la vitesse de lecture immediatement. TPR = Ticks Per Row.
    Valeur basse = rapide, valeur haute = lent.</td></tr>
<tr><td><b>Cxx</b></td><td>Note cut</td><td>xx=ticks</td>
    <td>Coupe le son apres xx ticks (fractions de ligne). Utile pour des notes tres courtes,
    comme des hi-hats ou des staccatos.</td></tr>
<tr><td><b>Dxx</b></td><td>Note delay</td><td>xx=ticks</td>
    <td>Retarde le declenchement de la note de xx ticks. La note ne commence pas tout de suite
    mais apres un petit delai. Utile pour des effets de groove ou de decalage.</td></tr>
<tr><td><b>Exy</b></td><td>Host command</td><td>x=type, y=valeur</td>
    <td>Commandes globales qui affectent tout le morceau (pas un seul canal).
    <b>E0x</b> = Fade out : le volume global diminue progressivement. x = vitesse (1=rapide, F=lent, 0=annuler le fade).
    <b>E1x</b> = Changement de vitesse : x = nouveau TPR (0 = 16). Equivalent a Bxx mais sur un demi-octet.</td></tr>
<tr><td><b>Fxx</b></td><td>Expression</td><td>xx=attenuation (0-F)</td>
    <td>Ajoute un offset d'attenuation permanent au canal. 0 = pas de reduction, F = quasi-silence.
    L'expression est <b>persistante</b> : elle reste active jusqu'au prochain Fxx.
    Utile pour baisser un canal en arriere-plan (ex: F04 = canal un peu plus discret).</td></tr>
</table>

<h2>Exemples pratiques</h2>

<h3>Accord majeur avec arpege (effet 0)</h3>
<p>Entrez une note C-4 avec l'effet <code>037</code> :</p>
<ul>
<li>0 = arpege, 3 = +3 demi-tons (Mi), 7 = +7 demi-tons (Sol)</li>
<li>Resultat : Do-Mi-Sol qui alterne tres vite = accord de Do majeur !</li>
</ul>
<p>Pour un accord mineur, utilisez <code>037</code> remplace par <code>047</code> : Do-Mi-Sol -> Do-Mib-Sol</p>

<h3>Son qui monte (effet 1)</h3>
<p>Note C-4 + effet <code>110</code> : le son monte lentement a partir de Do.
Note C-4 + effet <code>140</code> : le son monte plus vite.</p>

<h3>Glissement entre notes (effet 3)</h3>
<p>Ligne 00 : C-4 (Do), pas d'effet<br>
Ligne 04 : E-4 (Mi) + effet <code>310</code><br>
Resultat : a la ligne 04, le son glisse de Do vers Mi au lieu de sauter directement.</p>

<h3>Crescendo / Decrescendo (effet A)</h3>
<p>Note C-4 attn=F (silence) + effet <code>A10</code> : le volume monte progressivement (crescendo).
Note C-4 attn=0 (fort) + effet <code>A01</code> : le volume descend progressivement (decrescendo).</p>

<h3>Hi-hat court (effet C)</h3>
<p>Sur le canal Noise, entrez <b>W.H</b> (bruit blanc rapide) avec l'effet <code>C02</code> :
le bruit est coupe apres 2 ticks. Resultat : un "tss" tres court, parfait pour un hi-hat.</p>

<h3>Groove / Swing (effet D)</h3>
<p>Alternez des notes sans delai et avec <code>D03</code> pour creer un rythme "swing" :</p>
<pre>
00  C-4  ---    (sur le temps)
04  E-4  D03    (legerement en retard = groove)
08  C-4  ---    (sur le temps)
0C  G-4  D03    (legerement en retard)
</pre>

<h3>Fade out progressif (effet E)</h3>
<p>Placez <code>E03</code> sur n'importe quelle ligne : le volume global diminue progressivement
(vitesse 3 = moyen). Le morceau s'arrete automatiquement quand le silence est atteint.</p>
<p>Pour annuler un fade en cours, utilisez <code>E00</code>.</p>

<h3>Expression - baisser un canal (effet F)</h3>
<p>Pour qu'un canal soit plus discret (accompagnement), placez <code>F04</code> sur ce canal.
L'attenuation +4 reste active sur toutes les notes suivantes, jusqu'a un <code>F00</code> qui restaure le volume normal.</p>
<pre>
00  C-4  F04    (canal T1, volume reduit de 4)
04  E-4  ---    (toujours reduit, pas besoin de repeter F04)
08  G-4  F00    (retour au volume normal)
</pre>

<h2>Notes importantes</h2>
<ul>
<li>Un effet dure <b>une seule ligne</b>. A la ligne suivante, l'effet s'arrete (sauf portamento qui continue jusqu'a atteindre la cible).</li>
<li><b>Exceptions persistantes</b> : les effets <b>4xx (Pitch bend)</b> et <b>Fxx (Expression)</b> restent actifs jusqu'au prochain 4xx ou Fxx.</li>
<li>L'effet <b>Exy (Host command)</b> est <b>global</b> : il affecte tous les canaux. Dans l'export hybride, il n'est emis que sur le canal 0.</li>
<li>Les commandes <b>5xx..9xx</b> sont actuellement reservees / non implementees dans ce runtime.</li>
<li>Vous pouvez mettre un effet <b>sans note</b> (la ligne aura "---" en note mais un effet). L'effet s'applique au son deja en cours.</li>
<li>L'effet <code>000</code> (arpege avec parametre 00) ne fait rien. C'est la valeur par defaut.</li>
<li>Les effets sont <b>sauvegardes</b> dans les fichiers .ngpat et affiches dans le texte copie (Ctrl+Shift+C).</li>
</ul>
)");
}

QString HelpTab::topic_playback() {
    return QStringLiteral(R"(
<h1>Lecture / Playback</h1>

<h2>Commandes de lecture</h2>
<table>
<tr><th>Action</th><th>Raccourci</th><th>Bouton</th></tr>
<tr><td>Play / Pause</td><td><b>Espace</b></td><td>Play [Space]</td></tr>
<tr><td>Play depuis le debut</td><td><b>F5</b></td><td>-</td></tr>
<tr><td>Stop</td><td><b>F8</b></td><td>Stop [F8]</td></tr>
<tr><td>Jouer le song entier</td><td>-</td><td>Song</td></tr>
<tr><td>Boucler la selection</td><td>-</td><td>Loop Sel</td></tr>
<tr><td>Debug runtime par canal</td><td>-</td><td>Dbg RT</td></tr>
</table>

<h2>Mode Song (multi-pattern)</h2>
<p>Le bouton <b>Song</b> lance la lecture de tous les patterns dans l'ordre defini
par la <b>liste d'ordre</b> (Order list). Quand le dernier pattern est fini,
la lecture revient au <b>point de boucle</b> (marqueur L> dans la liste d'ordre).</p>

<h2>Loop Selection</h2>
<p>Pour travailler sur une partie specifique sans attendre tout le pattern :</p>
<ol>
<li>Selectionnez des lignes avec <b>Shift + fleches haut/bas</b></li>
<li>Cliquez sur <b>Loop Sel</b></li>
<li>La lecture boucle uniquement sur les lignes selectionnees !</li>
</ol>
<p>Tres pratique pour peaufiner un passage precis.</p>

<h2>Le mode Follow</h2>
<p>Quand <b>Follow</b> est actif (bouton enfonce), le curseur suit la position de lecture.
Desactivez-le si vous voulez editer pendant que la musique joue.</p>

<h2>La vitesse (TPR)</h2>
<p><b>TPR</b> = Ticks Per Row. C'est le nombre de frames (1/60e de seconde) entre chaque ligne.
Le BPM approximatif est affiche a cote :</p>
<table>
<tr><th>TPR</th><th>BPM approx.</th><th>Style</th></tr>
<tr><td>3</td><td>~300</td><td>Tres rapide (chiptune energique)</td></tr>
<tr><td>4</td><td>~225</td><td>Rapide</td></tr>
<tr><td>6</td><td>~150</td><td>Allegro</td></tr>
<tr><td>8</td><td>~112</td><td>Moderato (defaut)</td></tr>
<tr><td>10</td><td>~90</td><td>Andante</td></tr>
<tr><td>12</td><td>~75</td><td>Lent</td></tr>
<tr><td>16</td><td>~56</td><td>Tres lent</td></tr>
</table>

<h2>Preview de notes</h2>
<p>Quand vous entrez une note dans le tracker (en mode REC), vous entendez un court
apercu du son (~100ms). C'est normal, ca vous aide a savoir ce que vous tapez.</p>

<h2>Debug runtime (bouton Dbg RT)</h2>
<p>Le bouton <b>Dbg RT</b> ajoute un log de playback par ligne pour diagnostiquer
les ecarts entre tracker, export et driver.</p>
<ul>
<li>Activez <b>Dbg RT</b>, puis lancez Play/Song.</li>
<li>Chaque nouvelle ligne ajoute un bloc "DBG row ..." dans le log tracker.</li>
<li>Par canal, vous voyez: note cellule, instrument, attenuation/fx cellule, sortie runtime
(`divider` tone ou mode noise), attenuation runtime, fx runtime actif, expression, pitch bend.</li>
<li>Desactivez <b>Dbg RT</b> apres diagnostic pour garder un log lisible.</li>
</ul>

<h2>Le playback boucle</h2>
<p>Quand la lecture arrive a la derniere ligne du pattern, elle <b>reboucle au debut</b>
automatiquement. C'est le comportement normal d'un tracker.</p>
)");
}

QString HelpTab::topic_export() {
    return QStringLiteral(R"(
<h1>Export / Import / Sauvegarde</h1>

<h2>Formats de fichiers</h2>
<table>
<tr><th>Extension</th><th>Contenu</th><th>Usage</th></tr>
<tr><td><b>.ngpat</b></td><td>Un seul pattern (JSON)</td><td>Save/Load pattern individuel</td></tr>
<tr><td><b>.ngps</b></td><td>Morceau complet (JSON)</td><td>Save Song / Load Song (multi-pattern + ordre + loop)</td></tr>
<tr><td><b>.wav</b></td><td>Audio PCM 16-bit mono</td><td>Export WAV (ecoutable dans tout lecteur)</td></tr>
<tr><td><b>.mid</b></td><td>MIDI standard</td><td>Import MIDI dans le tracker</td></tr>
</table>

<h2>Sauvegarder un pattern (.ngpat)</h2>
<p>Pour ne pas perdre votre travail :</p>
<ul>
<li><b>Ctrl+S</b> ou bouton <b>Save</b> : sauvegarde le pattern actif</li>
<li>Le format est <b>.ngpat</b> (c'est du JSON lisible)</li>
<li>Sauvegardez souvent !</li>
</ul>

<h2>Sauvegarder un morceau (.ngps)</h2>
<p>Pour sauvegarder le morceau entier (tous les patterns + liste d'ordre + point de boucle) :</p>
<ul>
<li>Bouton <b>Save Song</b> : sauvegarde tout en fichier <b>.ngps</b></li>
<li>Bouton <b>Load Song</b> : charge un fichier .ngps (ou .ngpat pour compatibilite)</li>
</ul>

<h2>Charger un pattern</h2>
<ul>
<li><b>Ctrl+O</b> ou bouton <b>Load</b> : ouvre un fichier .ngpat</li>
<li>Le pattern actuel est remplace (mais vous pouvez Ctrl+Z pour annuler)</li>
</ul>

<h2>Export WAV</h2>
<p>Pour exporter votre morceau en fichier audio jouable partout :</p>
<ol>
<li>Cliquez sur le bouton <b>WAV</b> dans la barre d'outils</li>
<li>Choisissez l'emplacement et le nom du fichier</li>
<li>Le rendu se fait hors-ligne (tout le morceau est calcule)</li>
<li>En mode Song : tous les patterns sont rendus dans l'ordre</li>
<li>En mode pattern : seul le pattern actif est rendu</li>
</ol>
<p>Le fichier WAV est en PCM 16-bit mono 44100 Hz. Les canaux mutes sont pris en compte.</p>

<h2>Import MIDI</h2>
<p>Vous pouvez importer un fichier MIDI standard directement dans le tracker :</p>
<ol>
<li>Cliquez sur le bouton <b>MIDI</b> dans la barre d'outils</li>
<li>Selectionnez un fichier .mid ou .midi</li>
<li>L'importateur cree automatiquement les patterns necessaires</li>
</ol>
<p>Notes sur l'import MIDI :</p>
<ul>
<li>Les 3 premiers canaux melodiques MIDI vont dans T0, T1, T2</li>
<li>Le canal MIDI 10 (percussions) va dans le canal Noise</li>
<li>Si plus de 3 notes jouent en meme temps, les plus anciennes sont remplacees</li>
<li>La velocite MIDI est convertie en attenuation (0-F)</li>
<li>Le tempo MIDI est utilise pour suggerer un TPR adapte</li>
<li>Plusieurs patterns sont crees si le MIDI est plus long qu'un pattern</li>
</ul>

<h2>Exporter en code C / ASM</h2>
<p>C'est la fonctionnalite cle pour les developpeurs NGPC !</p>
<ol>
<li>Composez votre musique dans le Tracker</li>
<li>Choisissez le <b>mode d'export</b> dans le combo de la barre d'outils</li>
<li>Cliquez sur <b>Export C</b> (fichier .c/.h) ou <b>ASM</b> (fichier .inc)</li>
<li>Choisissez l'emplacement et le nom du fichier</li>
<li>Incluez le fichier dans votre projet NGPC</li>
</ol>

<h3>Mode Pre-baked (par defaut)</h3>
<p>L'export simule le playback tick par tick, exactement comme
le rendu WAV. Tous les <b>effets</b> (arpege, pitch slide, portamento, volume slide,
note cut, note delay) et les <b>instruments</b> (enveloppe, vibrato, sweep, pitch curve)
sont automatiquement integres dans les streams. Le son dans le jeu est
<b>identique</b> au son dans le tracker.</p>
<p>Les streams sont plus detailles car chaque changement d'etat (pitch, volume)
est capture au tick pres. Un arpege genere plusieurs evenements par row.</p>
<ul>
<li><b>Stabilite / previsible :</b> excellente. C'est le mode le plus "what you hear is what you get".</li>
<li><b>CPU driver (console) :</b> plus faible, car le stream contient deja le resultat calcule.</li>
<li><b>Taille ROM (poids dans le jeu) :</b> plus elevee, car les streams sont plus volumineux.</li>
<li><b>RAM :</b> faible impact, peu d'etats runtime a maintenir.</li>
<li><b>Dependance driver :</b> minimale (lecture de streams standard).</li>
</ul>

<h3>Mode Hybride</h3>
<p>L'export parcourt les rows directement et emet des <b>opcodes instrument</b>
pour que le driver NGPC gere les effets (enveloppe, vibrato, sweep) :</p>
<ul>
<li><code>0xF4</code> <b>SET_INST</b> : selectionne un instrument (pour le driver compile)</li>
<li><code>0xF0</code> <b>SET_ATTN</b> : change l'attenuation de base</li>
<li><code>0xF1</code> <b>SET_ENV</b> : parametres enveloppe (step, speed)</li>
<li><code>0xF2</code> <b>SET_VIB</b> : parametres vibrato (depth, speed, delay)</li>
<li><code>0xF3</code> <b>SET_SWEEP</b> : parametres sweep (end, step, speed)</li>
<li><code>0xF6</code> <b>HOST_CMD</b> : commande globale (fade out, tempo). Emis sur canal 0 uniquement.</li>
<li><code>0xF7</code> <b>SET_EXPR</b> : expression par canal (offset d'attenuation 0-15)</li>
<li><code>0xF8</code> <b>PITCH_BEND</b> : pitch bend par canal (2 bytes s16 LE, offset diviseur)</li>
<li><code>0xF9</code> <b>SET_ADSR</b> : enveloppe ADSR (attack, decay, sustain, release)</li>
<li><code>0xFA</code> <b>SET_LFO</b> : LFO pitch (wave, rate, depth)</li>
</ul>
<p>Les streams sont plus <b>compacts</b> (1 evenement par row au lieu de 1 par tick).
Le driver applique les effets instrument en temps reel.</p>
<p><b>Attention :</b> les effets tracker (arpege, pitch slide, portamento, volume slide)
ne sont <b>pas</b> inclus en mode hybride car le driver ne les supporte pas.
Effets pris en charge : 4xx (pitch bend), Bxx (speed), Cxx (note cut), Dxx (note delay), Exy (host cmd), Fxx (expression).</p>
<ul>
<li><b>Stabilite / previsible :</b> bonne si vous utilisez le bon Pack Driver NGPC, mais plus sensible aux ecarts tool/driver.</li>
<li><b>CPU driver (console) :</b> plus eleve que pre-baked, car le driver calcule les effets en runtime.</li>
<li><b>Taille ROM (poids dans le jeu) :</b> plus faible, streams plus compacts.</li>
<li><b>RAM :</b> legerement plus d'etat runtime par canal (enveloppe, vibrato, ADSR, LFO), mais impact generalement faible.</li>
<li><b>Dependance driver :</b> forte (il faut le pack driver aligne avec le tool).</li>
</ul>

<h3>Quel mode choisir ?</h3>
<table>
<tr><th>Aspect</th><th>Pre-baked</th><th>Hybride</th></tr>
<tr><td>Fidelite tool -> console</td><td>Excellente (quasi identique)</td><td>Bonne si driver aligne, sinon ecarts possibles</td></tr>
<tr><td>Stabilite integration</td><td>Plus simple, moins de surprises</td><td>Plus de points de vigilance (driver/opcodes)</td></tr>
<tr><td>CPU audio (Z80)</td><td>Plus faible</td><td>Plus eleve (effets calcules en runtime)</td></tr>
<tr><td>RAM runtime</td><td>Tres faible</td><td>Un peu plus d'etat par canal</td></tr>
<tr><td>Poids ROM des musiques</td><td>Plus lourd</td><td>Plus leger</td></tr>
<tr><td>Effets tracker</td><td>Tous inclus (arpege/slides/etc.)</td><td>Subset: 4xx/Bxx/Cxx/Dxx/Exy/Fxx</td></tr>
<tr><td>Effets instrument avances</td><td>Deja "bakes" dans le stream</td><td>Gerés par le driver (F0..FA)</td></tr>
<tr><td>Dependance Pack Driver NGPC</td><td>Faible</td><td>Forte</td></tr>
<tr><td>Profil recommande</td><td>Debutant, debug, parite maximale</td><td>Avance, optimisation taille, pipeline driver maitrise</td></tr>
</table>

<p>Les deux modes exportent le <b>morceau complet</b> (toute la liste d'ordre)
avec le point de boucle (<code>BGM_CHx_LOOP</code>) calcule automatiquement.</p>

<h3>Decision rapide</h3>
<ul>
<li><b>Tu veux du resultat fiable rapidement :</b> prends <b>Pre-baked</b>.</li>
<li><b>Tu veux reduire le poids ROM des songs :</b> prends <b>Hybride</b> (avec le Pack Driver NGPC).</li>
<li><b>Tu utilises beaucoup d'effets tracker (arpege/slides/volume slide) :</b> reste en <b>Pre-baked</b>.</li>
<li><b>Tu veux un pipeline long terme optimise :</b> Hybride est pertinent, mais demande plus de rigueur d'integration.</li>
<li><b>Approche pragmatique :</b> demarrer en Pre-baked, passer en Hybride quand le jeu est stable et le driver valide.</li>
</ul>

<h3>Audit export (warnings)</h3>
<p>A chaque export C/ASM, le tracker lance un audit de coherence tool/driver.</p>
<ul>
<li>Les warnings apparaissent dans le log tracker: <code>WARN export: ...</code></li>
<li>Les memes warnings sont copies en commentaire au debut du fichier exporte</li>
<li>Exemples: instrument hors plage, FX nibble non supporte runtime, table notes > 51</li>
</ul>
<p>L'export continue meme avec warnings, mais ces messages signalent des risques de
parite audio a corriger avant integration finale.</p>
<p><b>Rappel important :</b> pour une parite tool -> console sur les fonctions avancees,
utilisez le <b>Pack Driver NGPC</b> (source technique: <code>driver_custom_latest</code>,
minimum <code>sounds.c</code> + <code>sounds.h</code>).</p>
<p><b>Astuce verification :</b> l'onglet <b>Player</b> utilise desormais un preview
<b>Hybride (driver-like opcodes)</b> force pour tester les streams avec opcodes instrument.</p>

<h3>Onglet Player : a quoi servent les boutons ?</h3>
<ul>
<li><b>Load Driver</b> : option <b>avancee</b>. Sert uniquement si vous voulez charger un driver Z80 externe specifique pour des tests.
En usage normal du tool, laissez vide (non requis).</li>
<li><b>Load MIDI</b> : convertit le MIDI en streams preview et le charge dans le Player.</li>
<li><b>Convertir + Play</b> : reconstruit un export temporaire depuis le MIDI courant puis lance la lecture immediatement
(pratique pour tester vite un changement sans sauvegarder de fichier final).</li>
<li><b>Export</b> : genere le fichier final <code>.c</code> ou <code>.asm</code> pour integration dans le jeu.</li>
</ul>

<h2>Format de l'export C / ASM</h2>
<p>L'export genere :</p>
<ul>
<li><code>NOTE_TABLE[]</code> : table des diviseurs de frequence (paires lo/hi, max 51 notes)</li>
<li><code>BGM_CH0[]</code>, <code>BGM_CH1[]</code>, <code>BGM_CH2[]</code> : streams pour les 3 canaux tone</li>
<li><code>BGM_CHN[]</code> : stream pour le canal noise</li>
<li><code>BGM_CHx_LOOP</code> : offset byte du point de boucle dans chaque stream</li>
</ul>
<p>Ce format est compatible avec l'onglet <b>Player</b> : vous pouvez
recharger l'export pour verifier le rendu.</p>
<p>Le PlayerTab traite desormais les opcodes <code>0xF0</code>-<code>0xF4</code>, <code>0xF6</code>,
<code>0xF7</code>, <code>0xF8</code>, <code>0xF9</code> et <code>0xFA</code> : les effets instrument (enveloppe, vibrato, sweep, ADSR, LFO),
l'expression, le pitch bend, le fade out et le tempo sont joues en temps reel dans la preview.</p>

<h2>Mode Projet : Export All</h2>
<p>Si vous travaillez en mode projet, utilisez l'onglet <b>Projet</b> puis :</p>
<ul>
<li><b>Export All C</b> ou <b>Export All ASM</b></li>
<li>Un choix de mode <b>Pre-baked</b> ou <b>Hybride</b> est propose avant l'export songs/all</li>
<li><b>Exporter Pack Driver NGPC...</b> pour copier le pack d'integration dans un dossier cible</li>
<li>Le logiciel exporte toutes les songs du projet en une passe</li>
<li>Il genere aussi une banque globale d'instruments et une banque globale SFX</li>
</ul>

<p>Fichiers generes dans <code>exports/</code> :</p>
<ul>
<li><code>song_xxx.c</code> (ou <code>.inc</code>) : musique</li>
<li><code>project_instruments.c</code> : instruments globaux du projet</li>
<li><code>project_sfx.c</code> : SFX globaux du projet</li>
<li><code>project_audio_manifest.txt</code> : index songs/fichiers/prefixes symboles</li>
<li><code>project_audio_api.h</code> + <code>project_audio_api.c</code> (mode C) : table <code>NGPC_PROJECT_SONGS[]</code></li>
</ul>

<p>Details techniques :</p>
<ul>
<li>Les symbols de songs sont namespaces (<code>PROJECT_..._BGM_CH0</code>, etc.) pour eviter les collisions au link.</li>
<li>Le manifest est utile comme checklist d'integration dans le code du jeu.</li>
<li>L'API C projet centralise les pointeurs streams/loops de toutes les songs.</li>
</ul>

<p><b>Integration runtime conseillee :</b></p>
<ul>
<li>Utilisez <code>NgpcProject_BgmStartLoop4ByIndex(i)</code> (dans <code>project_audio_api.c</code>)</li>
<li>Ce helper applique automatiquement la bonne <code>NOTE_TABLE</code> via <code>Bgm_SetNoteTable(...)</code></li>
<li>Puis il lance les streams avec <code>Bgm_StartLoop4Ex(...)</code></li>
</ul>

<h3>Niveau / normalisation (onglet Projet)</h3>
<ul>
<li><b>Analyser niveau song</b> : calcule un peak offline (rendu WAV interne)</li>
<li><b>Normaliser song active</b> : propose un offset d'attenuation vers une cible de peak</li>
<li>Cette normalisation modifie seulement les cellules avec attenuation explicite (pas les cellules AUTO)</li>
<li><b>Normaliser SFX projet</b> : applique un offset global sur <code>tone_attn</code> / <code>noise_attn</code> de la banque SFX</li>
</ul>

<h3>Integration rapide de project_sfx.c (cote jeu)</h3>
<p>1) Sauvegardez vos SFX dans <b>SFX Lab</b> via <b>Save To Project</b>.<br>
2) Lancez <b>Export All C</b>.<br>
3) Ajoutez <code>project_sfx.c</code> au build du jeu.<br>
4) Branchez <code>Sfx_Play(id)</code> sur la banque exportee.</p>

<pre>
extern const unsigned char PROJECT_SFX_COUNT;
extern const unsigned char PROJECT_SFX_TONE_ON[];
extern const unsigned char PROJECT_SFX_TONE_CH[];
extern const unsigned short PROJECT_SFX_TONE_DIV[];
extern const unsigned char PROJECT_SFX_TONE_ATTN[];
extern const unsigned char PROJECT_SFX_TONE_FRAMES[];
extern const unsigned char PROJECT_SFX_TONE_SW_ON[];
extern const unsigned short PROJECT_SFX_TONE_SW_END[];
extern const signed short PROJECT_SFX_TONE_SW_STEP[];
extern const unsigned char PROJECT_SFX_TONE_SW_SPEED[];
extern const unsigned char PROJECT_SFX_TONE_SW_PING[];
extern const unsigned char PROJECT_SFX_TONE_ENV_ON[];
extern const unsigned char PROJECT_SFX_TONE_ENV_STEP[];
extern const unsigned char PROJECT_SFX_TONE_ENV_SPD[];
extern const unsigned char PROJECT_SFX_NOISE_ON[];
extern const unsigned char PROJECT_SFX_NOISE_RATE[];
extern const unsigned char PROJECT_SFX_NOISE_TYPE[];
extern const unsigned char PROJECT_SFX_NOISE_ATTN[];
extern const unsigned char PROJECT_SFX_NOISE_FRAMES[];
extern const unsigned char PROJECT_SFX_NOISE_BURST[];
extern const unsigned char PROJECT_SFX_NOISE_BURST_DUR[];
extern const unsigned char PROJECT_SFX_NOISE_ENV_ON[];
extern const unsigned char PROJECT_SFX_NOISE_ENV_STEP[];
extern const unsigned char PROJECT_SFX_NOISE_ENV_SPD[];
extern const unsigned char PROJECT_SFX_TONE_ADSR_ON[];
extern const unsigned char PROJECT_SFX_TONE_ADSR_AR[];
extern const unsigned char PROJECT_SFX_TONE_ADSR_DR[];
extern const unsigned char PROJECT_SFX_TONE_ADSR_SL[];
extern const unsigned char PROJECT_SFX_TONE_ADSR_SR[];
extern const unsigned char PROJECT_SFX_TONE_ADSR_RR[];
extern const unsigned char PROJECT_SFX_TONE_LFO1_ON[];
extern const unsigned char PROJECT_SFX_TONE_LFO1_WAVE[];
extern const unsigned char PROJECT_SFX_TONE_LFO1_HOLD[];
extern const unsigned char PROJECT_SFX_TONE_LFO1_RATE[];
extern const unsigned char PROJECT_SFX_TONE_LFO1_DEPTH[];
extern const unsigned char PROJECT_SFX_TONE_LFO2_ON[];
extern const unsigned char PROJECT_SFX_TONE_LFO2_WAVE[];
extern const unsigned char PROJECT_SFX_TONE_LFO2_HOLD[];
extern const unsigned char PROJECT_SFX_TONE_LFO2_RATE[];
extern const unsigned char PROJECT_SFX_TONE_LFO2_DEPTH[];
extern const unsigned char PROJECT_SFX_TONE_LFO_ALGO[];

void Sfx_Play(u8 id) {
    if (id >= PROJECT_SFX_COUNT) return;
    if (PROJECT_SFX_TONE_ON[id]) {
        Sfx_PlayToneEx(PROJECT_SFX_TONE_CH[id], PROJECT_SFX_TONE_DIV[id], PROJECT_SFX_TONE_ATTN[id],
                       PROJECT_SFX_TONE_FRAMES[id], PROJECT_SFX_TONE_SW_END[id], PROJECT_SFX_TONE_SW_STEP[id],
                       PROJECT_SFX_TONE_SW_SPEED[id], PROJECT_SFX_TONE_SW_PING[id], PROJECT_SFX_TONE_SW_ON[id],
                       PROJECT_SFX_TONE_ENV_ON[id], PROJECT_SFX_TONE_ENV_STEP[id], PROJECT_SFX_TONE_ENV_SPD[id]);
    }
    if (PROJECT_SFX_NOISE_ON[id]) {
        Sfx_PlayNoiseEx(PROJECT_SFX_NOISE_RATE[id], PROJECT_SFX_NOISE_TYPE[id], PROJECT_SFX_NOISE_ATTN[id],
                        PROJECT_SFX_NOISE_FRAMES[id], PROJECT_SFX_NOISE_BURST[id], PROJECT_SFX_NOISE_BURST_DUR[id],
                        PROJECT_SFX_NOISE_ENV_ON[id], PROJECT_SFX_NOISE_ENV_STEP[id], PROJECT_SFX_NOISE_ENV_SPD[id]);
    }
}
</pre>

<p>Ensuite, dans votre code gameplay : <code>Sfx_Play(0);</code></p>
<p>Note: les tableaux <code>PROJECT_SFX_TONE_ADSR_*</code> et <code>PROJECT_SFX_TONE_LFO*</code>
sont fournis pour les runtimes SFX avances (driver custom etendu).</p>

<h2>Copier en texte</h2>
<p><b>Ctrl+Shift+C</b> copie le pattern (ou la selection) en texte ASCII formate.
Utile pour partager sur un forum, un README, ou un chat :</p>
<pre>Row | Tone 0     | Tone 1
----+----------+----------
00  | C-4 00 -  | C-2 01 -
01  | E-4 00 -  | --- -- -
02  | G-4 00 -  | --- -- -
03  | C-5 00 -  | --- -- -</pre>
)");
}

QString HelpTab::topic_sfxlab() {
    return QStringLiteral(R"(
<h1>SFX Lab</h1>
<p>L'onglet <b>SFX Lab</b> est un labo de test pour les sons bruts du T6W28.
C'est utile pour comprendre comment la puce sonore fonctionne,
mais vous n'en avez pas besoin pour composer dans le Tracker.</p>

<h2>Controles</h2>
<table>
<tr><th>Parametre</th><th>Ce que ca fait</th></tr>
<tr><td><b>Tone On / Noise On</b></td><td>Active/desactive la partie tone et/ou noise du SFX</td></tr>
<tr><td><b>Tone Ch</b></td><td>Canal tone cible (0..2)</td></tr>
<tr><td><b>Tone Divisor</b></td><td>Frequence du son tone (plus petit = plus aigu)</td></tr>
<tr><td><b>Tone Attn</b></td><td>Volume du tone (0 = fort, 15 = silence)</td></tr>
<tr><td><b>Tone Frames</b></td><td>Duree tone (0 = one-shot)</td></tr>
<tr><td><b>Tone Sweep*</b></td><td>Glide du tone (end/step/speed/ping)</td></tr>
<tr><td><b>Tone Env*</b></td><td>Envelope tone (step/speed)</td></tr>
<tr><td><b>Tone ADSR*</b></td><td>ADSR complet AR/DR/SL/SR/RR pour les one-shots tone</td></tr>
<tr><td><b>Tone LFO / MOD2*</b></td><td>LFO1 + LFO2 + algo (0..7) pour modulation pitch/volume tone</td></tr>
<tr><td><b>Noise Rate</b></td><td>Vitesse noise (haut/moyen/bas/lie T2)</td></tr>
<tr><td><b>Noise Type</b></td><td>0 = bruit periodique, 1 = bruit blanc</td></tr>
<tr><td><b>Noise Attn</b></td><td>Volume du noise</td></tr>
<tr><td><b>Noise Frames</b></td><td>Duree noise (0 = one-shot)</td></tr>
<tr><td><b>Noise Burst*</b></td><td>Burst noise (on + duree)</td></tr>
<tr><td><b>Noise Env*</b></td><td>Envelope noise (step/speed)</td></tr>
<tr><td><b>Mode preview</b></td><td>Driver-faithful force (logique frames/sweep/env/burst/ADSR/LFO)</td></tr>
<tr><td><b>Presets</b></td><td>Sons pre-configures (laser, explosion, etc.)</td></tr>
<tr><td><b>Save To Project</b></td><td>Ajoute le SFX courant a la banque SFX du projet</td></tr>
</table>

<p><b>*</b> Reglages avances du driver (exportes dans <code>project_sfx.c</code>).</p>

<h2>Workflow recommande</h2>
<ol>
<li>Chargez un preset ou reglez manuellement les valeurs</li>
<li>Le mode preview est force en <b>Driver-faithful</b></li>
<li>Testez avec <b>Play Tone</b> / <b>Play Noise</b></li>
<li>Cliquez <b>Save To Project</b> et donnez un nom</li>
<li>Repetez pour chaque SFX utile du jeu</li>
<li>Dans l'onglet Projet, lancez <b>Export All C</b></li>
</ol>

<p>Le fichier <code>exports/project_sfx.c</code> est genere automatiquement.
Chaque ligne de la liste SFX projet devient un index (0, 1, 2, ...), utilisable via <code>Sfx_Play(id)</code> cote jeu.</p>
<p><b>Play Tone</b> / <b>Play Noise</b> / <b>Play Full SFX</b> appliquent tous la logique
driver-like (frames, sweep, env, burst, ADSR, LFO), pour coller au rendu console.</p>

<h2>A quoi ca sert ?</h2>
<p>Principalement pour :</p>
<ul>
<li>Comprendre la relation entre les registres PSG et le son</li>
<li>Tester rapidement un son avant de creer un instrument</li>
<li>Deboguer des problemes de son</li>
<li>Constituer une banque SFX reutilisable pour tout le projet</li>
</ul>
)");
}

QString HelpTab::topic_shortcuts() {
    return QStringLiteral(R"(
<h1>Raccourcis clavier</h1>

<h2>Navigation</h2>
<table>
<tr><th>Touche</th><th>Action</th></tr>
<tr><td>Fleches haut/bas</td><td>Monter/descendre d'une ligne</td></tr>
<tr><td>Fleches gauche/droite</td><td>Changer de sous-colonne (Note &rarr; Inst &rarr; Attn)</td></tr>
<tr><td>Tab</td><td>Canal suivant</td></tr>
<tr><td>Shift+Tab</td><td>Canal precedent</td></tr>
<tr><td>Page Up / Page Down</td><td>Sauter de 16 lignes</td></tr>
<tr><td>Home / End</td><td>Debut / fin du pattern</td></tr>
</table>

<h2>Edition</h2>
<table>
<tr><th>Touche</th><th>Action</th></tr>
<tr><td>Touches piano</td><td>Entrer une note (voir section "Le clavier")</td></tr>
<tr><td>Backspace</td><td>Inserer un note-off (OFF = couper le son)</td></tr>
<tr><td>Delete</td><td>Effacer la cellule</td></tr>
<tr><td>0-9, A-F</td><td>Hex digit (sous-colonne Inst ou Attn)</td></tr>
<tr><td>Insert</td><td>Inserer une ligne vide</td></tr>
<tr><td>Shift+Delete</td><td>Supprimer la ligne</td></tr>
</table>

<h2>Raccourcis Ctrl</h2>
<table>
<tr><th>Touche</th><th>Action</th></tr>
<tr><td>Ctrl+Z</td><td>Annuler</td></tr>
<tr><td>Ctrl+Y / Ctrl+Shift+Z</td><td>Refaire</td></tr>
<tr><td>Ctrl+C</td><td>Copier</td></tr>
<tr><td>Ctrl+X</td><td>Couper</td></tr>
<tr><td>Ctrl+V</td><td>Coller</td></tr>
<tr><td>Ctrl+A</td><td>Selectionner tout (canal)</td></tr>
<tr><td>Ctrl+Shift+A</td><td>Selectionner tout (tous canaux)</td></tr>
<tr><td>Ctrl+Shift+C</td><td>Copier en texte</td></tr>
<tr><td>Ctrl+D</td><td>Dupliquer la ligne</td></tr>
<tr><td>Ctrl+I</td><td>Interpoler colonne active (Inst/Vol/FX param)</td></tr>
<tr><td>Ctrl+H</td><td>Humanize attenuation (aleatoire leger)</td></tr>
<tr><td>Ctrl+B</td><td>Batch apply colonne active</td></tr>
<tr><td>Ctrl+T</td><td>Appliquer template pattern</td></tr>
<tr><td>Ctrl+Haut / Ctrl+Bas</td><td>Transposer +1 / -1 demi-ton</td></tr>
<tr><td>Ctrl+Shift+Haut / Bas</td><td>Transposer +12 / -12 (octave)</td></tr>
<tr><td>Ctrl+S</td><td>Sauvegarder</td></tr>
<tr><td>Ctrl+O</td><td>Charger</td></tr>
<tr><td>Ctrl+Delete</td><td>Effacer tout le pattern</td></tr>
</table>

<h2>Transport</h2>
<table>
<tr><th>Touche</th><th>Action</th></tr>
<tr><td>Espace</td><td>Play / Stop</td></tr>
<tr><td>F5</td><td>Play depuis le debut</td></tr>
<tr><td>F8</td><td>Stop</td></tr>
<tr><td>F1 / F2 / F3 / F4</td><td>Muter canal T0 / T1 / T2 / N</td></tr>
<tr><td>Numpad +</td><td>Octave +1</td></tr>
<tr><td>Numpad -</td><td>Octave -1</td></tr>
<tr><td>Numpad *</td><td>Step +1</td></tr>
<tr><td>Numpad /</td><td>Step -1</td></tr>
</table>

<h2>Selection</h2>
<table>
<tr><th>Touche</th><th>Action</th></tr>
<tr><td>Shift + fleches</td><td>Etendre la selection</td></tr>
<tr><td>Shift + Page Up/Down</td><td>Etendre de 16 lignes</td></tr>
<tr><td>Shift + Home/End</td><td>Etendre jusqu'au debut/fin</td></tr>
<tr><td>Shift + clic</td><td>Selectionner jusqu'au clic</td></tr>
<tr><td>Double-clic</td><td>Selectionner tout le canal</td></tr>
<tr><td>Echap</td><td>Annuler la selection</td></tr>
<tr><td>Clic droit</td><td>Menu contextuel</td></tr>
</table>
)");
}

QString HelpTab::topic_faq() {
    return QStringLiteral(R"(
<h1>FAQ / Problemes courants</h1>

<h2>Je tape des touches mais rien ne se passe !</h2>
<p><b>Solution</b> : Verifiez que le bouton <b>REC</b> est actif (rouge).
Si il n'est pas rouge, cliquez dessus pour l'activer.</p>
<p>Verifiez aussi que le curseur est sur la sous-colonne <b>Note</b>
(la premiere colonne de la cellule, pas Inst ou Attn).</p>

<h2>J'ai change la langue mais je ne vois pas la difference</h2>
<p>Le changement de langue s'applique au redemarrage. Normalement le logiciel redemarre automatiquement
apres validation dans la fenetre de demarrage.</p>
<ul>
<li>Si besoin, fermez et relancez manuellement l'application.</li>
<li>Le choix de langue est memorise pour les lancements suivants.</li>
</ul>

<h2>Le son est bizarre / pas de son</h2>
<ul>
<li>Verifiez que le canal n'est pas <b>mute</b> (F1-F4 ou boutons Mute)</li>
<li>Verifiez que l'<b>attenuation</b> n'est pas a F (silence)</li>
<li>Verifiez que vous avez un <b>instrument</b> configure dans l'onglet Instruments</li>
<li>Essayez de cliquer sur <b>Play</b> (Espace) - le preview ne joue que 100ms</li>
</ul>

<h2>Le curseur ne bouge pas apres avoir tape une note</h2>
<p><b>Solution</b> : Le <b>Step</b> est probablement a 0. Mettez-le a 1 ou plus dans la barre d'outils.</p>

<h2>Je n'entends qu'un seul canal</h2>
<p>Un canal est probablement en mode <b>Solo</b>. Verifiez les boutons Solo dans la barre
sous les outils et desactivez-les.</p>

<h2>Les notes sonnent trop graves / trop aigues</h2>
<p>Changez l'<b>octave</b> avec la spinbox Oct ou les raccourcis Numpad +/-.</p>
<ul>
<li>Oct 2-3 : basse</li>
<li>Oct 4 : medium (defaut)</li>
<li>Oct 5-6 : aigu</li>
</ul>

<h2>Comment faire un fade-out ?</h2>
<p>Utilisez l'interpolation (voir section "Tracker : avance" pour le detail) :</p>
<ol>
<li>Naviguez sur la sous-colonne <b>Vol</b> (fleche droite x2 depuis Note)</li>
<li>Sur la premiere ligne, tapez <b>0</b> (fort)</li>
<li>Descendez a la derniere ligne, tapez <b>F</b> (silence)</li>
<li>Selectionnez toute la zone avec <b>Shift + fleches</b> (vous devez voir le surlignage bleu)</li>
<li><b>Ctrl+I</b> = interpolation automatique !</li>
</ol>
<p>Si ca ne marche pas : verifiez que vous avez bien une selection active (surlignage bleu)
et que le curseur est sur le bon canal.</p>

<h2>Comment faire des percussions ?</h2>
<p>Utilisez le canal <b>Noise</b> (derniere colonne, "N"). Le noise a 8 configurations :</p>
<ul>
<li><b>P.H / P.M / P.L / P.T</b> = bruit periodique (kick, tom, buzz)</li>
<li><b>W.H / W.M / W.L / W.T</b> = bruit blanc (hihat, snare, crash)</li>
</ul>
<p><b>Saisie rapide (AZERTY)</b> : W=P.H, S=P.M, X=P.L, D=P.T, A=W.H, Z=W.M, E=W.L, R=W.T</p>
<p>Exemple de rythme basique :</p>
<ol>
<li>Mettez le curseur sur la colonne Noise (Tab x3)</li>
<li>Ligne 00 : tapez <b>W</b> (= P.H, kick), attn 0</li>
<li>Ligne 04 : tapez <b>A</b> (= W.H, hihat), attn 3, effet <code>C02</code> (coupe courte)</li>
<li>Ligne 08 : tapez <b>W</b> (= P.H, kick), attn 0</li>
<li>Ligne 0C : tapez <b>A</b> (= W.H, hihat), attn 3, effet <code>C02</code></li>
</ol>
<p>Vous pouvez aussi <b>double-cliquer</b> sur la sous-colonne Note du canal Noise
pour ouvrir un menu deroulant avec les 8 configurations et leur description.</p>

<h2>Comment copier un canal vers un autre ?</h2>
<ol>
<li>Selectionnez tout le canal source (double-clic ou Ctrl+A)</li>
<li><b>Ctrl+C</b> pour copier</li>
<li>Changez de canal avec <b>Tab</b></li>
<li><b>Home</b> pour aller au debut</li>
<li><b>Ctrl+V</b> pour coller</li>
</ol>

<h2>J'ai fait une erreur !</h2>
<p><b>Ctrl+Z</b> pour annuler. Vous pouvez annuler jusqu'a 100 actions !</p>

<h2>Comment partager ma musique ?</h2>
<ul>
<li><b>Save Song</b> : sauvegarder en .ngps (morceau complet reouvrable)</li>
<li><b>Ctrl+S</b> : sauvegarder en .ngpat (pattern seul)</li>
<li><b>WAV</b> : exporter en audio (ecoutable dans tout lecteur)</li>
<li><b>Export C</b> : fichier .c/.h (pre-baked ou hybride selon le combo)</li>
<li><b>ASM</b> : fichier .inc assembleur (pre-baked ou hybride selon le combo)</li>
<li><b>Ctrl+Shift+C</b> : texte lisible a coller dans un message/forum</li>
</ul>

<h2>J'ai des "WARN export" dans le log, c'est grave ?</h2>
<p>Pas forcement bloquant, mais c'est un signal utile:</p>
<ul>
<li>L'export a bien ete genere.</li>
<li>Le warning indique un point a verifier pour garder la parite tool/driver.</li>
<li>Corrigez dans le tracker, puis re-exportez jusqu'a obtenir un log propre.</li>
</ul>

<h2>L'import MIDI ne donne pas un bon resultat</h2>
<ul>
<li>Le NGPC n'a que 3 canaux melodiques + 1 bruit : les MIDI complexes perdent des notes</li>
<li>Simplifiez votre MIDI avant import (reduisez la polyphonie a 3 voix max)</li>
<li>Ajustez le TPR apres import si le tempo ne semble pas bon</li>
<li>Retouchez les notes dans le tracker apres import pour corriger les artefacts</li>
</ul>

<h2>Le mode Song ne boucle pas au bon endroit</h2>
<p>Verifiez le <b>point de boucle</b> dans la liste d'ordre :
selectionnez l'entree souhaitee et cliquez <b>[Loop]</b>.
Le marqueur <b>L&gt;</b> doit apparaitre devant la bonne entree.</p>

<h2>Le WAV exporte est silencieux</h2>
<ul>
<li>Verifiez que les canaux ne sont pas tous <b>mutes</b></li>
<li>Verifiez que les patterns contiennent bien des notes</li>
<li>En mode Song, verifiez que la liste d'ordre n'est pas vide</li>
</ul>

<h2>A quoi sert le Output meter ?</h2>
<ul>
<li>Il affiche le niveau <b>peak</b> audio en temps reel (0-100%)</li>
<li><b>CLIP</b> signifie que le signal a touche la limite numerique (risque de saturation)</li>
<li>Si CLIP apparait souvent, augmentez les attenuations (notes/instruments/SFX)</li>
</ul>
)");
}
