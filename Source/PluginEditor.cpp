#include "PluginEditor.h"

using namespace juce;

namespace
{
const Colour kCream  (0xfff1ece0);
const Colour kLcdOn  (0xff8dffa8);
const Colour kRed    (0xffff3b3b);
const Colour kGreen  (0xff3ddc6a);

Font bold (float h)  { return Font (FontOptions (h, Font::bold)); }
Font plain (float h) { return Font (FontOptions (h)); }
Font mono (float h, bool b = false) { return Font (FontOptions (Font::getDefaultMonospacedFontName(), h, b ? Font::bold : Font::plain)); }

// Positions (repère de conception 640 x 640) — reprises du dessin du manuel :
// 4 boutons en haut, MIX|WET + MOM/ONF à gauche, NOISE/TREMOLO/TEMPO au milieu, logo, deux pédales, FILTER en bas au centre.
constexpr int kColX[4] = { 134, 254, 380, 506 };
constexpr int kRow1 = 176, kRow2 = 318, kFilterY = 540;
constexpr int kOnfX = 122, kTapX = 518, kPadY = 500, kPadD = 112;
const Point<float> kOnfLed (166.f, 369.f), kTapLed (520.f, 386.f);
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

    // graduations sérigraphiées (une par cran pour les réglages à crans)
    const double range = s.getMaximum() - s.getMinimum();
    int nTicks = 11;
    if (s.getInterval() > 0.0 && range / s.getInterval() <= 12.0)
        nTicks = (int) std::round (range / s.getInterval()) + 1;
    nTicks = jmax (2, nTicks);
    for (int i = 0; i < nTicks; ++i)
    {
        const float t = (float) i / (float) (nTicks - 1);
        const float a = a0 + t * (a1 - a0);
        const float r1 = R - 1.f, r2 = R - ((i == 0 || i == nTicks - 1) ? 9.f : 6.f);
        g.setColour (kCream.withAlpha (hot ? 0.55f : 0.34f));
        g.drawLine (c.x + r1 * std::sin (a), c.y - r1 * std::cos (a),
                    c.x + r2 * std::sin (a), c.y - r2 * std::cos (a), 1.5f);
    }

    // bouton noir mat à bague
    const float kr = R - 14.f;
    Path body; body.addEllipse (c.x - kr, c.y - kr, 2.f * kr, 2.f * kr);
    DropShadow (Colours::black.withAlpha (0.7f), 10, { 0, 5 }).drawForPath (g, body);
    g.setGradientFill (ColourGradient (Colour (hot ? 0xff3b3e45 : 0xff2f3238), c.x - kr * 0.3f, c.y - kr * 0.45f,
                                       Colour (0xff0b0b0d), c.x - kr * 0.3f + kr * 1.6f, c.y - kr * 0.45f, true));
    g.fillPath (body);
    g.setColour (Colours::black);                    g.drawEllipse (c.x - kr, c.y - kr, 2.f * kr, 2.f * kr, 1.6f);
    g.setColour (Colours::white.withAlpha (0.07f));  g.drawEllipse (c.x - kr + 1.5f, c.y - kr + 1.5f, 2.f * kr - 3.f, 2.f * kr - 3.f, 1.f);
    // dessus légèrement concave
    const float tr = kr * 0.72f;
    g.setGradientFill (ColourGradient (Colour (0xff0e0f11), c.x, c.y - tr, Colour (0xff25272c), c.x, c.y + tr, false));
    g.fillEllipse (c.x - tr, c.y - tr, 2.f * tr, 2.f * tr);

    // repère blanc
    Path ind; ind.addRoundedRectangle (-1.9f, -kr + 3.f, 3.8f, kr * 0.5f, 1.9f);
    g.setColour (kCream);
    g.fillPath (ind, AffineTransform::rotation (ang).translated (c.x, c.y));
}

//==============================================================================
void FootSwitch::paint (Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    const float a = isEnabled() ? 1.f : 0.4f;
    const float d = jmin (b.getWidth(), b.getHeight()) - 6.f;
    auto pad = Rectangle<float> (d, d).withCentre (b.getCentre());
    if (down) pad = pad.translated (0.f, 1.5f);
    g.setOpacity (a);
    g.setColour (Colours::black.withAlpha (0.6f * a)); g.fillEllipse (pad.translated (0.f, 5.f).expanded (1.5f));
    g.setGradientFill (ColourGradient (Colour (0xff9a9fa9), pad.getX(), pad.getY(), Colour (0xff25272c), pad.getX(), pad.getBottom(), false));
    g.fillEllipse (pad);
    auto dome = pad.reduced (d * 0.08f);
    g.setGradientFill (down ? ColourGradient (Colour (0xff2b2d33), dome.getX(), dome.getY(), Colour (0xff484b54), dome.getX(), dome.getBottom(), false)
                            : ColourGradient (Colour (0xff565a64), dome.getX(), dome.getY(), Colour (0xff2b2d33), dome.getX(), dome.getBottom(), false));
    g.fillEllipse (dome);
    g.setColour (Colours::black.withAlpha (0.75f * a)); g.drawEllipse (pad, 1.6f);
    g.setColour (Colours::white.withAlpha (0.10f * a)); g.drawEllipse (dome.reduced (1.f), 1.f);
}

void FootSwitch::mouseDown (const MouseEvent&) { if (!isEnabled()) return; down = true; if (onDown) onDown(); repaint(); }
void FootSwitch::mouseUp (const MouseEvent&)   { if (!down) return;        down = false; if (onUp) onUp(); repaint(); }

//==============================================================================
void SlideSwitch::paintButton (Graphics& g, bool over, bool)
{
    auto b = getLocalBounds().toFloat();
    const float a = isEnabled() ? 1.f : 0.4f;
    const bool first = (getToggleState() == onIsFirst);   // la poignée est-elle côté « première légende » ?
    const Colour lit = kCream.withMultipliedAlpha (a), dim = kCream.withAlpha (0.38f * a);

    Rectangle<float> track, legA, legB;
    if (orient == Horizontal)
    {
        const float tw = b.getWidth() * 0.34f;
        legA = b.withWidth ((b.getWidth() - tw) * 0.5f);
        legB = b.withTrimmedLeft (legA.getWidth() + tw);
        track = Rectangle<float> (tw, jmin (b.getHeight(), 18.f)).withCentre (b.getCentre());
    }
    else
    {
        const float th = b.getHeight() * 0.50f;
        legA = b.withHeight ((b.getHeight() - th) * 0.5f);
        legB = b.withTrimmedTop (legA.getHeight() + th);
        track = Rectangle<float> (jmin (b.getWidth(), 20.f), th).withCentre (b.getCentre());
    }

    g.setFont (bold (11.f));
    g.setColour (first ? lit : dim);  g.drawText (firstText,  legA, Justification::centred);
    g.setColour (first ? dim : lit);  g.drawText (secondText, legB, Justification::centred);

    // glissière
    g.setColour (Colour (0xff050506)); g.fillRoundedRectangle (track, 6.f);
    g.setColour (Colours::white.withAlpha (over ? 0.22f : 0.10f)); g.drawRoundedRectangle (track, 6.f, 1.f);
    Rectangle<float> thumb;
    if (orient == Horizontal)
    {
        thumb = Rectangle<float> (track.getWidth() * 0.5f, track.getHeight() - 4.f).withCentre ({ first ? track.getX() + track.getWidth() * 0.27f : track.getRight() - track.getWidth() * 0.27f, track.getCentreY() });
    }
    else
    {
        thumb = Rectangle<float> (track.getWidth() - 4.f, track.getHeight() * 0.5f).withCentre ({ track.getCentreX(), first ? track.getY() + track.getHeight() * 0.27f : track.getBottom() - track.getHeight() * 0.27f });
    }
    g.setGradientFill (ColourGradient (Colour (0xffd9dbe0).withMultipliedAlpha (a), thumb.getX(), thumb.getY(), Colour (0xff7d8189).withMultipliedAlpha (a), thumb.getX(), thumb.getBottom(), false));
    g.fillRoundedRectangle (thumb, 3.f);
    g.setColour (Colours::black.withAlpha (0.55f)); g.drawRoundedRectangle (thumb, 3.f, 1.f);
}

//==============================================================================
RevoloEditor::RevoloEditor (RevoloProcessor& p) : AudioProcessorEditor (&p), proc (p)
{
    setLookAndFeel (&look);

    addKnob (reverb,    "reverb",    "REVERB",    kColX[0], kRow1);
    addKnob (character, "character", "CHARACTER", kColX[1], kRow1);
    addKnob (length,    "length",    "LENGTH",    kColX[2], kRow1);
    addKnob (air,       "air",       "AIR",       kColX[3], kRow1);
    addKnob (noise,     "noise",     "NOISE",     kColX[1], kRow2);
    addKnob (tremolo,   "tremolo",   "TREMOLO",   kColX[2], kRow2);
    addKnob (tempo,     "tempo",     "TEMPO",     kColX[3], kRow2);
    addKnob (filter,    "filter",    "FILTER",    320, kFilterY, true);

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

    tapBtn.onDown = [this] { proc.tap(); };
    addAndMakeVisible (tapBtn);

    attachTempoKnob (proc.apvts.getRawParameterValue ("sync")->load() > 0.5f);

    // grain de l'aluminium sablé (anodisé noir mat)
    grain = Image (Image::ARGB, 160, 160, true);
    Random rnd (42);
    for (int y = 0; y < grain.getHeight(); ++y)
        for (int x = 0; x < grain.getWidth(); ++x)
            grain.setPixelAt (x, y, Colour::fromFloatRGBA (1.f, 1.f, 1.f, rnd.nextFloat() * 0.045f));

    setResizeLimits (400, 400, 1600, 1600);
    getConstrainer()->setFixedAspectRatio ((double) ui::kW / (double) ui::kH);
    setResizable (true, true);
    setSize (ui::kW, ui::kH);
    startTimerHz (30);
}

RevoloEditor::~RevoloEditor() { stopTimer(); setLookAndFeel (nullptr); }

void RevoloEditor::addKnob (Knob& k, const String& id, const String& name, int cx, int cy, bool labelBelow)
{
    k.name = name; k.cx = cx; k.cy = cy; k.labelBelow = labelBelow;
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
    const bool onfLed = proc.apvts.getRawParameterValue ("active")->load() > 0.5f;
    if (onfLed != lastOnfLed) { lastOnfLed = onfLed; repaint (S (int (kOnfLed.x) - 14, int (kOnfLed.y) - 14, 28, 28)); }
    const bool tapLed = proc.lfoPhase.load() < 0.5f;
    if (tapLed != lastTapLed) { lastTapLed = tapLed; repaint (S (int (kTapLed.x) - 14, int (kTapLed.y) - 14, 28, 28)); }

    const bool sync = proc.apvts.getRawParameterValue ("sync")->load() > 0.5f;
    if (sync != tempoIsSync) attachTempoKnob (sync);

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

    mixSwitch.setBounds  (S (kColX[0] - 62, 238, 124, 24));
    momSwitch.setBounds  (S (kColX[0] - 30, 292, 60, 86));
    syncSwitch.setBounds (S (kColX[3] - 62, 408, 124, 24));
    onf.setBounds    (S (kOnfX - kPadD / 2, kPadY - kPadD / 2, kPadD, kPadD));
    tapBtn.setBounds (S (kTapX - kPadD / 2, kPadY - kPadD / 2, kPadD, kPadD));
}

//==============================================================================
void RevoloEditor::drawLed (Graphics& g, Point<float> c, Colour colour, bool on)
{
    g.setColour (Colour (0xff050506)); g.fillEllipse (c.x - 8.f, c.y - 8.f, 16.f, 16.f);
    if (on)
    {
        g.setGradientFill (ColourGradient (colour.withAlpha (0.5f), c.x, c.y, colour.withAlpha (0.f), c.x + 18.f, c.y, true));
        g.fillEllipse (c.x - 18.f, c.y - 18.f, 36.f, 36.f);
    }
    g.setGradientFill (ColourGradient (on ? colour.brighter (0.7f) : colour.darker (0.9f), c.x - 2.f, c.y - 2.f,
                                       on ? colour.darker (0.2f) : colour.darker (1.5f), c.x + 5.f, c.y + 5.f, true));
    g.fillEllipse (c.x - 5.5f, c.y - 5.5f, 11.f, 11.f);
}

// Logo « REVOLO » : lettres rondes, légèrement désaxées, avec ombre portée (comme sur le manuel)
void RevoloEditor::drawLogo (Graphics& g, Point<float> c)
{
    const String letters = "REVOLO";
    const float yOff[6] = { -4.f, 3.f, -2.f, 4.f, -3.f, 2.f };
    const float rot[6]  = { -0.06f, 0.05f, -0.04f, 0.06f, -0.05f, 0.04f };
    const Font f = bold (58.f);
    float widths[6], total = 0.f;
    for (int i = 0; i < 6; ++i) { widths[i] = f.getStringWidthFloat (letters.substring (i, i + 1)); total += widths[i] + 3.f; }
    float x = c.x - total * 0.5f;
    for (int i = 0; i < 6; ++i)
    {
        GlyphArrangement ga; ga.addLineOfText (f, letters.substring (i, i + 1), 0.f, 0.f);
        Path p; ga.createPath (p);
        const auto bnd = p.getBounds();
        const auto t = AffineTransform::rotation (rot[i], bnd.getCentreX(), bnd.getCentreY()).translated (x, c.y + yOff[i]);
        g.setColour (Colour (0xff4b4e58)); g.fillPath (p, t.translated (3.f, 3.f));                       // ombre portée
        g.setColour (Colour (0xff050506)); g.strokePath (p, PathStrokeType (4.5f, PathStrokeType::curved, PathStrokeType::rounded), t);
        g.setColour (kCream);              g.fillPath (p, t);
        x += widths[i] + 3.f;
    }
}

void RevoloEditor::paint (Graphics& g)
{
    g.addTransform (AffineTransform::scale (scale));
    const Rectangle<float> full (0.f, 0.f, (float) ui::kW, (float) ui::kH);

    g.setColour (Colour (0xff0b0c0e)); g.fillRect (full);

    // face avant : aluminium anodisé noir mat, sablé
    const Rectangle<float> face (20.f, 64.f, 600.f, 560.f);
    g.setGradientFill (ColourGradient (Colour (0xff1b1c20), face.getX(), face.getY(), Colour (0xff121316), face.getX(), face.getBottom(), false));
    g.fillRoundedRectangle (face, 24.f);
    {
        Graphics::ScopedSaveState ss (g);
        Path clip; clip.addRoundedRectangle (face, 24.f); g.reduceClipRegion (clip);
        g.setTiledImageFill (grain, 0, 0, 1.f); g.fillRect (face);
    }
    g.setColour (Colours::black);                    g.drawRoundedRectangle (face, 24.f, 2.f);
    g.setColour (Colours::white.withAlpha (0.07f));  g.drawRoundedRectangle (face.reduced (2.f), 22.f, 1.f);

    // en-tête (hors face avant) : titre + afficheur LCD
    g.setColour (kCream); g.setFont (bold (22.f));
    g.drawText ("REVOLO-STYLE", Rectangle<float> (26.f, 12.f, 340.f, 26.f), Justification::centredLeft);
    g.setColour (kCream.withAlpha (0.5f)); g.setFont (bold (9.5f));
    g.drawText ("DJ REVERB + TREMOLO  -  UNOFFICIAL PLUGIN", Rectangle<float> (27.f, 38.f, 340.f, 12.f), Justification::centredLeft);

    const auto lcd = Rectangle<float> (396.f, 14.f, 224.f, 32.f);
    g.setColour (Colour (0xff09100c)); g.fillRoundedRectangle (lcd, 6.f);
    g.setColour (Colour (0xff2d3f33)); g.drawRoundedRectangle (lcd, 6.f, 1.3f);
    String title, value;
    if (lcdUntil != 0)                                 { title = lcdTitle; value = lcdValue; }
    else if (tempoIsSync && proc.hostBpm.load() > 0.f) { title = "HOST";   value = String (proc.hostBpm.load(), 1) + " BPM"; }
    else                                               { title = "READY"; }
    g.setColour (kLcdOn.withAlpha (0.55f)); g.setFont (mono (12.f));
    g.drawText (title, lcd.reduced (11.f, 0.f), Justification::centredLeft);
    g.setColour (kLcdOn); g.setFont (mono (15.f, true));
    g.drawText (value, lcd.reduced (11.f, 0.f), Justification::centredRight);

    // noms des boutons (sérigraphie)
    g.setColour (kCream.withAlpha (0.92f)); g.setFont (bold (12.5f));
    for (auto* k : knobs)
    {
        const float ly = k->labelBelow ? (float) k->cy + 52.f : (float) k->cy - 76.f;
        g.drawText (k->name, Rectangle<float> ((float) k->cx - 70.f, ly, 140.f, 16.f), Justification::centred);
    }

    // trait TEMPO → TAP, comme sur le dessin du manuel
    g.setColour (kCream.withAlpha (0.5f));
    g.drawLine ((float) kColX[3], (float) kRow2 + 50.f, (float) kColX[3], kTapLed.y - 11.f, 1.4f);
    g.setFont (bold (12.5f)); g.setColour (kCream.withAlpha (tapBtn.isEnabled() ? 0.92f : 0.35f));
    g.drawText ("TAP", Rectangle<float> (kTapLed.x - 56.f, kTapLed.y - 8.f, 40.f, 16.f), Justification::centredRight);

    drawLed (g, kOnfLed, kRed, lastOnfLed);
    drawLed (g, kTapLed, kGreen, lastTapLed && tapBtn.isEnabled());

    // logo
    drawLogo (g, { 320.f, 408.f });
    g.setColour (kCream); g.setFont (bold (21.f));
    g.drawText ("TEIL2", Rectangle<float> (220.f, 436.f, 200.f, 26.f), Justification::centred);

    g.setColour (kCream.withAlpha (0.28f)); g.setFont (plain (8.5f));
    g.drawText ("unofficial - inspired by the TEIL2 Revolo - not affiliated with Teile", Rectangle<float> (0.f, 626.f, (float) ui::kW, 12.f), Justification::centred);
}
