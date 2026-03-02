
# RubyDSP

## 1. Project Overview

**RubyDSP** is a high-performance Ruby gem for audio feature extraction and processing. It bridges the gap between Ruby's elegant syntax and C++'s raw computational speed. The goal is to provide a "Librosa-lite" experience for the Ruby ecosystem, enabling audio analysis, DSP (Digital Signal Processing), and machine learning preprocessing directly in Ruby without needing a Python microservice.

## 2. Architecture & Tech Stack

The project uses the "Ruby for API, C++ for compute" paradigm. All heavy data arrays and mathematical loops remain safely in C++ memory, avoiding Ruby's Global VM Lock (GVL) and Garbage Collector overhead.

* **Top Level (API):** Ruby 3.x
* **Binding Layer:** [Rice](https://github.com/ruby-rice/rice) (C++ API for Ruby)
* **Bottom Level (Compute):** C++17
* **Dependencies (Vendored in `ext/ruby_dsp/vendor/`):**
* [miniaudio](https://miniaud.io/): Single-file C/C++ library for decoding audio files (`.wav`, `.mp3`) into raw PCM float arrays.
* [KissFFT](https://github.com/mborgerding/kissfft): Lightweight C library for Fast Fourier Transforms (FFTs) to convert time-domain audio into frequency-domain data (not yet present).


* **Build System:** `rake-compiler` with `extconf.rb` (generating Makefiles via `mkmf-rice`).
* **Development Environment:** VS Code Devcontainer (Microsoft Ruby image + C++ build tools).

## 3. Core Mental Model & Memory Boundaries

The most critical architectural rule of this project: **Ruby never sees the raw audio arrays.** If a 3-minute song contains 8 million float samples, passing that array into Ruby will cause massive memory bloat and slow execution. Instead, the C++ layer acts as a vault:

1. Ruby asks C++ to load a file.
2. C++ loads the file, decodes it, and holds a `std::vector<float>` in memory.
3. Ruby asks C++ for a calculation (e.g., `track.rms_energy`).
4. C++ runs a fast SIMD/optimized loop over the vector and returns only the final, lightweight result (a single float or a small array of aggregates) back to Ruby.

## 4. Primary Data Structures

The core of the system is the `AudioTrack` C++ struct, which is exposed to Ruby as `RubyDSP::AudioTrack`.

```cpp
// Mental model of the C++ AudioTrack struct
struct AudioTrack {
    std::string filename;
    int sample_rate;
    int channels;
    std::vector<float> samples; // The heavy PCM audio data (Hidden from Ruby)

    // Constructor: Uses miniaudio to decode the file and fill the variables
    AudioTrack(std::string path); 

    // Methods exposed to Ruby via Rice:
    float duration();
    float zcr(); // Zero-crossing rate
    // ... etc
};

```

## 5. Project Directory Layout

```text
ruby_dsp/
├── .devcontainer/
│   └── devcontainer.json
├── lib/
│   ├── ruby_dsp/
│   │   └── version.rb
│   └── ruby_dsp.rb          # Ruby entry point (requires the compiled .so)
├── ext/
│   └── ruby_dsp/            
│       ├── extconf.rb       # Uses mkmf-rice to generate the Makefile
│       ├── ruby_dsp.cpp     # Main C++ file containing Init_ruby_dsp()
│       └── vendor/          # miniaudio.h (potentially kissFFT)
├── ruby_dsp.gemspec         
└── Rakefile                 # Configured with rake-compiler

```

## 6. Implementation Roadmap

* **Phase 1: I/O & Memory (Current)**
* Integrate `miniaudio.h`.
* Implement `AudioTrack` C++ constructor to read `.wav`/`.mp3` into a `std::vector<float>`.
* Expose basic metadata to Ruby (`sample_rate`, `channels`, `duration`).


* **Phase 2: Time-Domain Features**
* Implement RMS Energy (loudness over time).
* Implement Zero-Crossing Rate (ZCR).


* **Phase 3: Frequency-Domain (FFT)**
* Integrate `KissFFT`.
* Implement Spectral Centroid (brightness).
* Implement dominant pitch/frequency detection.


* **Phase 4: Advanced ML Features**
* Implement Mel-Frequency Cepstral Coefficients (MFCCs) for machine learning pipelines.
