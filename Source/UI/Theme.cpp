#include "Theme.h"

#include <BinaryData.h>

namespace pianolume::Theme
{

namespace
{
    juce::Typeface::Ptr load (const char* data, int size)
    {
        return juce::Typeface::createSystemTypefaceFor (data, (size_t) size);
    }

    struct Typefaces
    {
        juce::Typeface::Ptr regular  = load (PianolumeBinaryData::InterRegular_ttf,  PianolumeBinaryData::InterRegular_ttfSize);
        juce::Typeface::Ptr medium   = load (PianolumeBinaryData::InterMedium_ttf,   PianolumeBinaryData::InterMedium_ttfSize);
        juce::Typeface::Ptr semiBold = load (PianolumeBinaryData::InterSemiBold_ttf, PianolumeBinaryData::InterSemiBold_ttfSize);
        juce::Typeface::Ptr bold     = load (PianolumeBinaryData::InterBold_ttf,     PianolumeBinaryData::InterBold_ttfSize);
        juce::Typeface::Ptr mono     = load (PianolumeBinaryData::JetBrainsMonoRegular_ttf,
                                             PianolumeBinaryData::JetBrainsMonoRegular_ttfSize);
    };

    const Typefaces& getTypefaces()
    {
        static const Typefaces typefaces;
        return typefaces;
    }
}

juce::Typeface::Ptr getTypeface (int weight)
{
    const auto& typefaces = getTypefaces();

    if (weight >= 700) return typefaces.bold;
    if (weight >= 600) return typefaces.semiBold;
    if (weight >= 500) return typefaces.medium;
    return typefaces.regular;
}

juce::Typeface::Ptr getMonoTypeface()
{
    return getTypefaces().mono;
}

juce::Font uiFont (float height, int weight)
{
    return juce::Font (juce::FontOptions (getTypeface (weight)).withPointHeight (height));
}

juce::Font monoFont (float height)
{
    return juce::Font (juce::FontOptions (getMonoTypeface()).withPointHeight (height));
}

} // namespace pianolume::Theme
