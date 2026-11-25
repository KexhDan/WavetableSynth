#include "Wavetable.h"

#include <juce_audio_formats/juce_audio_formats.h>

#include <algorithm>
#include <cmath>

bool Wavetable::loadFrom (const juce::File& file)
{
    juce::AudioFormatManager manager;
    manager.registerBasicFormats();

    std::unique_ptr<juce::AudioFormatReader> reader (manager.createReaderFor (file));

    if (reader == nullptr || reader->lengthInSamples <= 0)
        return false;

    juce::AudioBuffer<float> buffer;
    buffer.setSize (1, (int) reader->lengthInSamples);

    if (! reader->read (&buffer, 0, (int) reader->lengthInSamples, 0, true, true))
        return false;

    const int numSamples = buffer.getNumSamples();
    const float* src = buffer.getReadPointer (0);

    frames = juce::jmax (1, (numSamples + frameSize - 1) / frameSize);
    data.assign ((size_t) frames * (size_t) frameSize, 0.0f);

    for (int f = 0; f < frames; ++f)
    {
        const int start = f * frameSize;
        const int available = juce::jmin (frameSize, numSamples - start);

        if (available <= 0)
            continue;

        float* dst = data.data() + (size_t) f * (size_t) frameSize;

        if (available == frameSize)
        {
            std::copy (src + start, src + start + frameSize, dst);
        }
        else
        {
            // El ultimo frame (o un WAV corto) se reescala a frameSize.
            for (int i = 0; i < frameSize; ++i)
            {
                const float p = (float) i / (float) (frameSize - 1) * (float) (available - 1);
                const int a = (int) p;
                const int b = juce::jmin (a + 1, available - 1);
                const float t = p - (float) a;
                dst[i] = src[start + a] + (src[start + b] - src[start + a]) * t;
            }
        }
    }

    fileName = file.getFileName();
    return true;
}

float Wavetable::read (float phase, float position) const noexcept
{
    if (isEmpty())
        return 0.0f;

    const float pos = juce::jlimit (0.0f, 1.0f, position) * (float) (frames - 1);
    const int f0 = juce::jmin ((int) pos, frames - 1);
    const int f1 = juce::jmin (f0 + 1, frames - 1);
    const float fb = pos - (float) f0;

    const float wrapped = phase - std::floor (phase); // siempre en [0, 1)
    const float idx = wrapped * (float) (frameSize - 1);

    const int i0 = (int) idx;
    const int i1 = juce::jmin (i0 + 1, frameSize - 1);
    const float frac = idx - (float) i0;

    const float* p0 = data.data() + (size_t) f0 * (size_t) frameSize;
    const float s0 = p0[i0] + (p0[i1] - p0[i0]) * frac;

    if (f0 == f1)
        return s0;

    const float* p1 = data.data() + (size_t) f1 * (size_t) frameSize;
    const float s1 = p1[i0] + (p1[i1] - p1[i0]) * frac;

    return s0 + (s1 - s0) * fb;
}
