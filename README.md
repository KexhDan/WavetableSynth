# Dual Wavetable Synth (VST3, JUCE)

Synth VST3 de 2 osciladores con wavetables importadas, ADSR, filtro LP simple y un LFO con matriz de modulación de 4 ranuras.

> No es un clon de Serum: es una versión mínima y funcional con la idea central (wavetables + modulación asignable).

## Características

- **2 osciladores**, cada uno con su propia wavetable WAV.
  - Wavetable Position, Octave (-2..2), Semitone, Fine (cents).
  - Unisono de 1 a 8 voces con Spread en cents.
  - Level y Pan (equal-power).
- **Carga de WAV** por oscilador (`Cargar WAV 1/2`).
  - WAV de un solo ciclo → 1 frame.
  - WAV con varios ciclos consecutivos de **2048 muestras** → varios frames; `WT Pos` hace crossfade entre frames (formato típico de wavetables de Serum).
  - Archivos más cortos o colas parciales se reescalan a 2048 muestras.
  - La ruta del WAV se guarda con el preset del DAW.
- **ADSR** de amplitudes compartido por las voces.
- **Filtro lowpass** (cutoff, resonance) con cutoff modulable.
- **1 LFO** (sine, triangle, saw, square) con rate 0.05..20 Hz.
- **Matriz de modulación**: 4 ranuras, cada una asigna el LFO a un destino con cantidad bipolar (-1..1, negativo invierte la fase):
  - OSC 1 / OSC 2: WT Position, Pitch (±12 semitonos), Level, Pan, Unison Detune.
  - Filtro: Cutoff (±4 octavas).
- **Polifonía** de 8 voces con voice-stealing.
- Salida con soft-clip (`tanh`).

## Notas de uso

- El plugin **arranca sin wavetables**: hasta que cargues un WAV, ese oscilador no produce sonido. Esto es intencional (solo wavetables importadas).
- Envía MIDI desde tu DAW/teclado; los eventos se procesan dentro del bloque de audio.
- Si usas un WAV estéreo, se toma el canal izquierdo.
- Sin anti-aliasing: con notas muy agudos y wavetables con muchos armónicos puede aparecer alias.

## Estructura

```
WavetableSynth/
  CMakeLists.txt
  Source/
    Wavetable.h/.cpp        carga y lectura interpolada de frames
    PluginProcessor.h/.cpp  DSP, parámetros, voces, modulación, estado
    PluginEditor.h/.cpp     interfaz y file choosers
```

## Compilar (Windows)

Requisitos: CMake >= 3.22, compilador C++17 (Visual Studio 2022 con "Desktop development with C++" o MinGW-w64), Git e internet la primera vez (FetchContent descarga JUCE 8.0.4).

### Visual Studio 2022

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

### MinGW-w64

```powershell
cmake -S . -B build -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

El `.vst3` queda en `build/WavetableSynth_artefacts/Release/VST3/` (o `.../VST3/` con MinGW). Cópialo a `C:\Program Files\Common Files\VST3` para que lo vean todos los DAWs. `COPY_PLUGIN_AFTER_BUILD` está desactivado para evitar problemas de permisos.

Si ya tienes JUCE descargado:

```powershell
cmake -S . -B build -DFETCHCONTENT_SOURCE_DIR_JUCE="C:\ruta\a\JUCE" ...
```

## Probarlo

Si tienes el host de prueba `VSTPluginTester/` (host JUCE) en tu workspace, compílalo y carga el `.vst3` desde ahí: tiene teclado MIDI en pantalla para probar el synth sin DAW.

## Licencia

Código del proyecto: uso libre para aprendizaje/modificación. JUCE se descarga de su repositorio oficial y tiene su propia licencia.
