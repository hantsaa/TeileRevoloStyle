#include "PluginEditor.h"

using namespace juce;

namespace
{
const Colour kCream  (0xfff3eddc);
const Colour kAmber  (0xffffb347);
const Colour kLcdOn  (0xff8dffa8);
const Colour kRed    (0xffff3b3b);
const Colour kGreen  (0xff3ddc6a);

Font bold (float h)  { return Font (FontOptions (h, Font::bold)); }
Font plain (float h) { return Font (FontOptions (h)); }
Font mono (float h, bool b = false) { return Font (FontOptions (Font::getDefaultMonospacedFontName(), h, b ? Font::bold : Font::plain)); }
}

//==============================================================================
void RevoloEditor::KnobLook::drawRotarySlider (Graphics& g, int x, int y, int w, int h, float pos,
                                               float a0, float a1, Slider& s)
{
    const auto b = Rectangle<float> ((float) x, (float) y, (float) w, (float) h);
    const float R = jmin (b.getWidth(), b.getHeight()) * 0.5f;
    const auto c = b.getCentre();
    const bool hot = s.isMouseOverOrDragging() && s.isEnabled();
    const float ang = a0 + pos * (a1 - a0);

    // graduations imprimées (une par cran pour les réglages à crans)
    const double range = s.getMaximum() - s.getMinimum();
    int nTicks = 11;
    if (s.getInterval() > 0.0 && range / s.getInterval() <= 12.0)
        nTicks = (int) std::round (range / s.getInterval()) + 1;
    nTicks = jmax (2, nTicks);
    for (int i = 0; i < nTicks; ++i)
    {
        const float t = (float) i / (float) (nTicks - 1);
        const float a = a0 + t * (a1 - a0);
        const float r1 = R - 1.f, r2 = R - ((i == 0 || i == nTicks - 1) ? 8.f : 6.f);
        g.setColour (kCream.withAlpha (0.38f));
        g.drawLine (c.x + r1 * std::sin (a), c.y - r1 * std::cos (a),
                    c.x + r2 * std::sin (a), c.y - r2 * std::cos (a), 1.6f);
    }

    // piste + arc de valeur
    Path track;  track.addCentredArc (c.x, c.y, R - 12.f, R - 12.f, 0.f, a0, a1, true);
    g.setColour (Colours::black.withAlpha (0.55f));
    g.strokePath (track, PathStrokeType (2.6f, PathStrokeType::curved, PathStrokeType::rounded));
    if (pos > 0.001f)
    {
        Path val; val.addCentredArc (c.x, c.y, R - 12.f, R - 12.f, 0.f, a0, ang, true);
        g.setColour (hot ? kAmber.brighter (0.25f) : kAmber);
        g.strokePath (val, PathStrokeType (2.6f, PathStrokeType::curved, PathStrokeType::rounded));
    }

    // corps du bouton
    const float kr = R - 19.f;
    Path body; body.addEllipse (c.x - kr, c.y - kr, 2.f * kr, 2.f * kr);
    DropShadow (Colours::black.withAlpha (0.65f), 9, { 0, 4 }).drawForPath (g, body);
    g.setGradientFill (ColourGradient (Colour (hot ? 0xff737985 : 0xff626875), c.x - kr * 0.25f, c.y - kr * 0.35f,
                                       Colour (0xff1c1e23), c.x - kr * 0.25f + kr * 1.45f, c.y - kr * 0.35f, true));
    g.fillPath (body);
    g.setColour (Colour (0xff0b0c0f));       g.drawEllipse (c.x - kr, c.y - kr, 2.f * kr, 2.f * kr, 1.6f);
    g.setColour (Colours::white.withAlpha (0.11f)); g.drawEllipse (c.x - kr + 2.f, c.y - kr + 2.f, 2.f * kr - 4.f, 2.f * kr - 4.f, 1.f);

    // repère
    Path ind; ind.addRoundedRectangle (-2.1f, -kr + 4.f, 4.2f, kr * 0.44f, 2.f);
    g.setColour (kCream);
    g.fillPath (ind, AffineTransform::rotation (ang).translated (c.x, c.y));
}

//==============================================================================
void FootSwitch::paint (Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    const bool on = ledOn && ledOn();
    const float a = isEnabled() ? 1.f : 0.35f;

    // LED
    const Point<float> lc (b.getCentreX(), 10.f);
    g.setColour (Colour (0xff0b0c0f)); g.fillEllipse (lc.x - 8.f, lc.y - 8.f, 16.f, 16.f);
    if (on && isEnabled())
    {
        g.setGradientFill (ColourGradient (ledColour.withAlpha (0.45f), lc.x, lc.y, ledColour.withAlpha (0.f), lc.x + 17.f, lc.y, true));
        g.fillEllipse (lc.x - 17.f, lc.y - 17.f, 34.f, 34.f);
    }
    g.setGradientFill (ColourGradient (on ? ledColour.brighter (0.7f) : ledColour.darker (0.9f), lc.x - 2.f, lc.y - 2.f,
                                       on ? ledColour.darker (0.2f) : ledColour.darker (1.4f), lc.x + 5.f, lc.y + 5.f, true));
    g.fillEllipse (lc.x - 5.5f, lc.y - 5.5f, 11.f, 11.f);

    // plot de la pédale
    const float d = jmin (b.getWidth() - 4.f, b.getHeight() - 30.f);
    auto pad = Rectangle<float> (d, d).withCentre ({ b.getCentreX(), 28.f + d * 0.5f });
    if (down) pad = pad.translated (0.f, 1.5f);
    g.setOpacity (a);
    g.setColour (Colours::black.withAlpha (0.55f * a)); g.fillEllipse (pad.translated (0.f, 4.f).expanded (1.f));
    g.setGradientFill (ColourGradient (Colour (0xff80858f), pad.getX(), pad.getY(), Colour (0xff23252a), pad.getX(), pad.getBottom(), false));
    g.fillEllipse (pad);
    auto dome = pad.reduced (d * 0.09f);
    g.setGradientFill (down ? ColourGradient (Colour (0xff2c2e34), dome.getX(), dome.getY(), Colour (0xff43464e), dome.getX(), dome.getBottom(), false)
                            : ColourGradient (Colour (0xff4f535d), dome.getX(), dome.getY(), Colour (0xff2c2e34), dome.getX(), dome.getBottom(), false));
    g.fillEllipse (dome);
    g.setColour (Colours::black.withAlpha (0.7f * a)); g.drawEllipse (pad, 1.6f);
    g.setColour (Colours::white.withAlpha (0.09f * a)); g.drawEllipse (dome.reduced (1.f), 1.f);
    // petites nervures antidérapantes
    g.setColour (Colours::black.withAlpha (0.28f * a));
    for (int i = -3; i <= 3; ++i)
    {
        const float yy = dome.getCentreY() + (float) i * d * 0.085f;
        const float hw = std::sqrt (jmax (0.f, dome.getWidth() * dome.getWidth() * 0.25f - (yy - dome.getCentreY()) * (yy - dome.getCentreY()))) - 6.f;
        if (hw > 4.f) g.drawLine (dome.getCentreX() - hw, yy, dome.getCentreX() + hw, yy, 1.2f);
    }
}

void FootSwitch::mouseDown (const MouseEvent&) { if (!isEnabled()) return; down = true; if (onDown) onDown(); repaint(); }
void FootSwitch::mouseUp (const MouseEvent&)   { if (!down) return;        down = false; if (onUp) onUp(); repaint(); }
void FootSwitch::refreshLed()
{
    const bool now = ledOn && ledOn();
    if (now != lastLed) { lastLed = now; repaint(); }
}

//==============================================================================
void TwoWaySwitch::paintButton (Graphics& g, bool over, bool)
{
    auto b = getLocalBounds().toFloat().reduced (1.f);
    const float r = b.getHeight() * 0.5f;
    const float a = isEnabled() ? 1.f : 0.4f;
    g.setColour (Colour (0xff0b0c0f).withAlpha (a)); g.fillRoundedRectangle (b, r);
    g.setColour (Colours::white.withAlpha (over ? 0.16f : 0.08f)); g.drawRoundedRectangle (b, r, 1.f);

    const bool on = getToggleState();
    auto inner = b.reduced (3.f);
    auto half = inner.withWidth (inner.getWidth() * 0.5f);
    if (on) half = half.withX (inner.getCentreX());
    g.setGradientFill (ColourGradient (kAmber.brighter (0.2f).withAlpha (a), half.getX(), half.getY(),
                                       kAmber.darker (0.3f).withAlpha (a), half.getX(), half.getBottom(), false));
    g.fillRoundedRectangle (half, half.getHeight() * 0.5f);

    g.setFont (bold (b.getHeight() * 0.42f));
    auto row = inner;
    g.setColour ((on ? kCream.withAlpha (0.55f) : Colour (0xff1a1205)).withMultipliedAlpha (a));
    g.drawText (offText, row.removeFromLeft (row.getWidth() * 0.5f), Justification::centred);
    g.setColour ((on ? Colour (0xff1a1205) : kCream.withAlpha (0.55f)).withMultipliedAlpha (a));
    g.drawText (onText, row, Justification::centred);
}

//==============================================================================
RevoloEditor::RevoloEditor (RevoloProcessor& p) : AudioProcessorEditor (&p), proc (p)
{
    setLookAndFeel (&look);

    addKnob (reverb,    "reverb",    "REVERB",    110, 134);
    addKnob (character, "character", "CHARACTER", 250, 134);
    addKnob (length,    "length",    "LENGTH",    390, 134);
    addKnob (air,       "air",       "AIR",       530, 134);
    addKnob (noise,     "noise",     "NOISE",     250, 270);
    addKnob (tremolo,   "tremolo",   "TREMOLO",   390, 270);
    addKnob (tempo,     "tempo",     "TEMPO",     530, 270);
    addKnob (filter,    "filter",    "FILTER",    320, 462);

    for (auto* sw : { &mixSwitch, &momSwitch, &syncSwitch }) addAndMakeVisible (*sw);
    mixAtt  = std::make_unique<AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, "mix",  mixSwitch);
    momAtt  = std::make_unique<AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, "mom",  momSwitch);
    syncAtt = std::make_unique<AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, "sync", syncSwitch);

    // Passer en MOM coupe l'effet : en mode momentané il ne doit agir que pendant l'appui
    momSwitch.onClick = [this]
    {
        if (momSwitch.getToggleState())
            if (auto* a = proc.apvts.getParameter ("active"))
            {
                a->beginChangeGesture();
                a->setValueNotifyingHost (0.f);
                a->endChangeGesture();
            }
    };

    // ONF : bascule en mode ONF, actif seulement pendant l'appui en mode MOM
    auto* active = proc.apvts.getParameter ("active");
    auto isMom = [this] { return proc.apvts.getRawParameterValue ("mom")->load() > 0.5f; };
    auto isOn  = [this] { return proc.apvts.getRawParameterValue ("active")->load() > 0.5f; };
    onf.ledColour = kRed;
    onf.ledOn = isOn;
    onf.onDown = [active, isMom, isOn]
    {
        active->beginChangeGesture();
        active->setValueNotifyingHost (isMom() ? 1.f : (isOn() ? 0.f : 1.f));
    };
    onf.onUp = [active, isMom]
    {
        if (isMom()) active->setValueNotifyingHost (0.f);
        active->endChangeGesture();
    };
    addAndMakeVisible (onf);

    tapBtn.ledColour = kGreen;
    tapBtn.ledOn = [this] { return proc.lfoPhase.load() < 0.5f; };
    tapBtn.onDown = [this] { proc.tap(); };
    addAndMakeVisible (tapBtn);

    attachTempoKnob (proc.apvts.getRawParameterValue ("sync")->load() > 0.5f);

    setResizeLimits (400, 274, 1600, 1096);
    getConstrainer()->setFixedAspectRatio ((double) ui::kW / (double) ui::kH);
    setResizable (true, true);
    setSize (ui::kW, ui::kH);
    startTimerHz (30);
}

RevoloEditor::~RevoloEditor() { stopTimer(); setLookAndFeel (nullptr); }

void RevoloEditor::addKnob (Knob& k, const String& id, const String& name, int cx, int cy)
{
    k.name = name; k.cx = cx; k.cy = cy;
    addAndMakeVisible (k.slider);
    k.slider.setRotaryParameters (degreesToRadians (225.f), degreesToRadians (495.f), true);
    k.slider.setMouseDragSensitivity (220);
    k.slider.setPopupDisplayEnabled (false, false, nullptr);
    k.slider.addMouseListener (this, false);   // pour l'afficheur LCD
    knobs.push_back (&k);
    if (id != "tempo")
        k.att = std::make_unique<AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, id, k.slider);
}

// Le bouton TEMPO règle la vitesse en Hz (FREE) ou la division du tempo de l'hôte (SYNC)
void RevoloEditor::attachTempoKnob (bool sync)
{
    tempoIsSync = sync;
    tempo.att.reset();
    tempo.att = std::make_unique<AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, sync ? "division" : "tempo", tempo.slider);
    tempo.name = sync ? "DIVISION" : "TEMPO";
    tapBtn.setEnabled (! sync);
    repaint();
}

void RevoloEditor::showReadoutFor (const MouseEvent& e)
{
    for (auto* k : knobs)
        if (e.eventComponent == &k->slider)
        {
            lcdTitle = k->name;
            lcdValue = k->slider.getTextFromValue (k->slider.getValue());
            lcdUntil = Time::getMillisecondCounter() + 1800;
            repaint (lcdArea());
            return;
        }
}

void RevoloEditor::timerCallback()
{
    onf.refreshLed();
    tapBtn.refreshLed();

    const bool sync = proc.apvts.getRawParameterValue ("sync")->load() > 0.5f;
    if (sync != tempoIsSync) attachTempoKnob (sync);

    const bool mom = proc.apvts.getRawParameterValue ("mom")->load() > 0.5f;
    if (mom != lastMom) { lastMom = mom; repaint(); }

    if (lcdUntil != 0 && Time::getMillisecondCounter() > lcdUntil) { lcdUntil = 0; repaint (lcdArea()); }
    if (sync && lcdUntil == 0) repaint (lcdArea());   // affiche le tempo de l'hôte
}

Rectangle<int> RevoloEditor::S (int x, int y, int w, int h) const
{
    return { roundToInt ((float) x * scale), roundToInt ((float) y * scale),
             roundToInt ((float) w * scale), roundToInt ((float) h * scale) };
}

void RevoloEditor::resized()
{
    scale = (float) getWidth() / (float) ui::kW;
    for (auto* k : knobs) k->slider.setBounds (S (k->cx - 48, k->cy - 48, 96, 96));

    mixSwitch.setBounds  (S (58, 238, 104, 28));
    momSwitch.setBounds  (S (58, 298, 104, 28));
    syncSwitch.setBounds (S (478, 342, 104, 26));
    onf.setBounds    (S (140 - 48, 388, 96, 124));
    tapBtn.setBounds (S (500 - 48, 388, 96, 124));
}

//==============================================================================
void RevoloEditor::paint (Graphics& g)
{
    g.addTransform (AffineTransform::scale (scale));
    const Rectangle<float> full (0.f, 0.f, (float) ui::kW, (float) ui::kH);

    g.setGradientFill (ColourGradient (Colour (0xff23262e), 0.f, 0.f, Colour (0xff101216), 0.f, (float) ui::kH, false));
    g.fillRect (full);

    // châssis
    auto body = full.reduced (6.f);
    g.setGradientFill (ColourGradient (Colour (0xff30343e), 0.f, 6.f, Colour (0xff1a1d24), 0.f, (float) ui::kH, false));
    g.fillRoundedRectangle (body, 18.f);
    g.setColour (Colour (0xff08090b)); g.drawRoundedRectangle (body, 18.f, 2.f);
    g.setColour (Colours::white.withAlpha (0.06f)); g.drawRoundedRectangle (body.reduced (2.f), 16.f, 1.f);

    // vis
    for (auto pt : { Point<float> (18.f, 18.f), Point<float> (622.f, 18.f), Point<float> (18.f, 530.f), Point<float> (622.f, 530.f) })
    {
        g.setGradientFill (ColourGradient (Colour (0xff8b909b), pt.x - 2.f, pt.y - 2.f, Colour (0xff2b2d33), pt.x + 5.f, pt.y + 5.f, true));
        g.fillEllipse (pt.x - 5.f, pt.y - 5.f, 10.f, 10.f);
        g.setColour (Colour (0xff0b0c0f)); g.drawEllipse (pt.x - 5.f, pt.y - 5.f, 10.f, 10.f, 1.f);
        g.drawLine (pt.x - 3.2f, pt.y + 1.8f, pt.x + 3.2f, pt.y - 1.8f, 1.4f);
    }

    // titre
    g.setColour (kCream);
    g.setFont (bold (23.f));
    g.drawText ("REVOLO-STYLE", Rectangle<float> (36.f, 12.f, 330.f, 26.f), Justification::centredLeft);
    g.setColour (kAmber.withAlpha (0.85f));
    g.setFont (bold (9.5f));
    g.drawText ("DJ REVERB + TREMOLO", Rectangle<float> (37.f, 36.f, 330.f, 12.f), Justification::centredLeft);

    // afficheur LCD
    const auto lcd = Rectangle<float> (396.f, 14.f, 220.f, 28.f);
    g.setColour (Colour (0xff09100c)); g.fillRoundedRectangle (lcd, 6.f);
    g.setColour (Colour (0xff2d3f33)); g.drawRoundedRectangle (lcd, 6.f, 1.3f);
    String title, value;
    if (lcdUntil != 0)                      { title = lcdTitle; value = lcdValue; }
    else if (tempoIsSync && proc.hostBpm.load() > 0.f) { title = "HOST"; value = String (proc.hostBpm.load(), 1) + " BPM"; }
    else                                    { title = "READY"; }
    g.setColour (kLcdOn.withAlpha (0.55f)); g.setFont (mono (12.f));
    g.drawText (title, lcd.reduced (11.f, 0.f), Justification::centredLeft);
    g.setColour (kLcdOn); g.setFont (mono (14.f, true));
    g.drawText (value, lcd.reduced (11.f, 0.f), Justification::centredRight);

    // noms des boutons
    g.setColour (kCream.withAlpha (0.9f)); g.setFont (bold (12.f));
    for (auto* k : knobs)
        g.drawText (k->name, Rectangle<float> ((float) k->cx - 70.f, (float) k->cy - 74.f, 140.f, 16.f), Justification::centred);

    // légendes des interrupteurs
    g.setColour (kCream.withAlpha (0.45f)); g.setFont (bold (9.f));
    g.drawText ("OUTPUT",     Rectangle<float> (58.f, 222.f, 104.f, 12.f), Justification::centred);
    g.drawText ("FOOTSWITCH", Rectangle<float> (58.f, 282.f, 104.f, 12.f), Justification::centred);
    g.drawText ("TREMOLO RATE", Rectangle<float> (478.f, 326.f, 104.f, 12.f), Justification::centred);

    // séparation + légendes des pédales
    g.setColour (Colours::black.withAlpha (0.45f)); g.drawLine (30.f, 384.f, 610.f, 384.f, 1.2f);
    g.setColour (Colours::white.withAlpha (0.05f)); g.drawLine (30.f, 385.2f, 610.f, 385.2f, 1.f);

    const bool mom = proc.apvts.getRawParameterValue ("mom")->load() > 0.5f;
    g.setColour (kCream.withAlpha (0.9f)); g.setFont (bold (12.f));
    g.drawText (mom ? "HOLD" : "ON / OFF", Rectangle<float> (140.f - 60.f, 514.f, 120.f, 16.f), Justification::centred);
    g.setColour (kCream.withAlpha (tapBtn.isEnabled() ? 0.9f : 0.35f));
    g.drawText ("TAP TEMPO", Rectangle<float> (500.f - 60.f, 514.f, 120.f, 16.f), Justification::centred);

    g.setColour (kCream.withAlpha (0.28f)); g.setFont (plain (8.5f));
    g.drawText ("unofficial - inspired by the TEIL2 Revolo - not affiliated with Teile", Rectangle<float> (0.f, 530.f, (float) ui::kW, 12.f), Justification::centred);
}
