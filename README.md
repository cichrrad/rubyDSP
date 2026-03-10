# RubyDSP | [Documentation](https://www.rubydoc.info/gems/ruby_dsp/0.0.6)

[![Ruby CI](https://github.com/cichrrad/rubyDSP/actions/workflows/test.yml/badge.svg)](https://github.com/cichrrad/rubyDSP/actions/workflows/test.yml)

---

> 🚧 **Status:** This (***HOBBY***) project is currently in early development. It is hopefully functional, but API changes are expected. There is no warranty regarding anything 🗿.

---

**RubyDSP** is an audio processing/synthesis and DSP Ruby gem made mostly for fun. It uses C++ under the hood, utilizing [miniaudio](https://miniaud.io/) and [Rice](https://github.com/jasonroelofs/rice). 

I made this gem to try [Rice](https://github.com/jasonroelofs/rice), as I would like to be able to bring some C++ speed to Ruby, oh my beloved....

## Features

* **Fast:** Basically all of the code is written in C++. While not extremely optimized currently, it still absolutely shreds native Ruby.

* **Fluent API:** Mutating methods return `self`, allowing for beautiful, highly readable method chaining (e.g., `track.to_mono!.normalize!.fade_in!(0.5)`).

* **Audio Synthesis & Sequencing:** Build multitrack, polyphonic audio from scratch using a (very limited) set of built-in mathematically generated waveforms (sine, square, sawtooth, and white noise).

* **Format Agnostic Loading:** Automatically decodes standard audio formats (WAV, MP3, FLAC) via `miniaudio`.

> **Note:** While the loading of these formats is supported, `miniaudio` encodes only in `.wav`. While other encodings might be considered in the future, they would require more dependencies and thus are not available right now.

* **Zero-Dependency Native Build:** No need to install `ffmpeg` or `libsndfile` on your system.

* **YARD Support:** Includes pure-Ruby stubs (in `stubs`, duh) for IDE autocomplete and inline documentation.

## Installation

Add this line to your application's `Gemfile`:

```ruby
gem 'ruby_dsp'

```

And then execute:

```bash
$ bundle install

```

Or install it yourself directly via:

```bash
$ gem install ruby_dsp

```

*(Note: Installing this gem requires a modern C++ compiler, as it builds the native extensions directly on your machine upon installation. It requires Ruby 3.0+).*

## Quick Start: Audio Processing

Here is a quick look at what you can do with a loaded `AudioTrack`. Thanks to the fluent API, you can process audio in a single readable chain:

```ruby
require 'ruby_dsp'

# Load an audio file
track = RubyDSP::AudioTrack.new("raw_vocals.wav")

puts track 
# => ['raw_vocals.wav', 12.450s duration, 2 channel(s), 48000Hz sample rate]

# Process, edit, and save in one chain
track.to_mono!                       # Averages channels into mono
     .resample!(44100)               # Linearly resamples to target rate
     .trim_silence!(-60.0)           # Strips leading/trailing silence below -60dB
     .normalize!(-1.0)               # Scales audio to target peak dBFS
     .pad_to_duration!(15.0)         # Centers audio evenly into a 15s window
     .fade_in!(0.5)                  # Adds a 0.5s linear fade-in
     .fade_out!(0.5)                 # Adds a 0.5s linear fade-out
     .save_track("processed.wav")    # Export the final result

# Analysis & Math (Still works!)
puts "Peak Amp: #{track.peak_amp}"
puts "Overall RMS: #{track.rms}"
puts "Overall ZCR: #{track.zcr}"

# You can also get framed analysis for time-series data:
framed_rms_data = track.framed_rms(frame_length: 2048, hop_length: 512)

```

## Quick Start: Synthesis & Sequencing

Initialize an empty track and generate your own jam using `add_wave!`:

```ruby
require 'ruby_dsp'

# Create a blank canvas (Mono, 44.1kHz)
track = RubyDSP::AudioTrack.new("", 1, 44100)

track.add_wave!("sine", 261.63, 3.0, 0.0)
     .add_wave!("sine", 329.63, 3.0, 1.0)
     .add_wave!("sine", 392.00, 3.0, 2.0)

# Polish and export
track.normalize!(-30.0)  # This goes a long way for your hearing
     .fade_out!(0.5)     # Smooth tail
     .save_track("c_major_chord.wav")

```

### Demos

> You can find scripts for these in `/demo` directory. Outputs were converted from `.wav` to `.mp4` to trick Github to show it in this README.

#### **TWINKLE**

* <video src="./demo/twinkle_twinkle.mp4" controls title="Twinkle"></video>

#### **TETRIS**

* <video src="./demo/tetris_theme.mp4" controls title="Tetris"></video>

#### **SUPER MARIO BROS**

* <video src="./demo/mario_intro.mp4" controls title="SMB"></video>

## Development

If you want to clone the repo and work on C++ guts, start with:

1. Clone the repo and run `bundle install` to grab the development dependencies.
2. Run `rake test` — this will automatically compile the C++ `extconf.rb` and run the Minitest suite.
3. Run `rake doc:generate ; rake doc:server` — this will compile the YARD stubs into HTML and boot a live-reloading local web server at `http://localhost:8808` so you can read the docs!

## License

The gem is available as open source under the terms of the **MIT License**.

---

> Cheers! - RC