#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

#include <vector>

/**
 * Una wavetable se guarda como una serie de "frames" (ciclos) de 2048 muestras.
 * Un WAV de un solo ciclo produce 1 frame. Un WAV de Serum-style (varios
 * ciclos consecutivos de 2048 muestras) produce tantos frames como ciclos, y se
 * pueden barrer con el parametro Wavetable Position.
 */
class Wavetable : public juce::ReferenceCountedObject
{
public:
    using Ptr = juce::ReferenceCountedObjectPtr<Wavetable>;

    static constexpr int frameSize = 2048;

    Wavetable() = default;
    explicit Wavetable (const juce::File& file) { loadFrom (file); }

    bool loadFrom (const juce::File& file);

    bool isEmpty() const noexcept        { return frames <= 0 || data.empty(); }
    int getNumFrames() const noexcept    { return frames; }
    juce::String getFileName() const     { return fileName; }

    /** Lee con interpolacion lineal dentro del frame y crossfade entre frames. */
    float read (float phase, float position) const noexcept;

private:
    std::vector<float> data;
    int frames = 0;
    juce::String fileName;
};
