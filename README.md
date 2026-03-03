# RubyDSP

> 🚧 **Status:** This project is currently in early development. It is fully functional, but API changes are expected.

**RubyDSP** is an audio processing and DSP Ruby gem. Ultimately, it aims to be `librosa`-wannabe for Ruby. It uses C++ under the hood, utilizing [miniaudio](https://miniaud.io/) and [Rice](https://github.com/jasonroelofs/rice)

## Features

* **Fast:** Basically all of the code is written in C++. While not extremely optimized currently, it still absolutely shreds native Ruby.
* **Format Agnostic Loading:** Automatically decodes standard audio formats (WAV, MP3, FLAC) via `miniaudio`.
* **Zero-Dependency Native Build:** No need to install `ffmpeg` or `libsndfile` on your system.
* **YARD Support:** Includes pure-Ruby stubs (in `stubs`, duh) for IDE autocomplete and inline documentation.

## Installation

Since the gem is currently in pre-release, you can install it directly from GitHub by adding this line to your application's `Gemfile`:

```ruby
gem 'ruby_dsp', github: 'cichrrad/ruby_dsp'

```

And then execute:

```bash
$ bundle install

```

*(Note: Installing this gem requires a modern C++ compiler, as it builds the native extensions directly on your machine upon installation. It requires Ruby 3.0+).*

## Quick Start

Here is a quick look at what you can do with a loaded `AudioTrack`:

```ruby
require 'ruby_dsp'

# Load an audio file
track = RubyDSP::AudioTrack.new("raw_vocals.wav")

puts track 
# => ['raw_vocals.wav', 12.450s duration, 2 channel(s), 48000Hz sample rate]

# Do stuff!
track.to_mono!             # Averages channels into mono
track.resample!(44100)     # Linearly resamples to target rate
track.trim_silence!(-60.0) # Strips leading/trailing silence below -60dB

# Analysis & Math
puts "Peak Amp: #{track.peak_amp}"
puts "Overall RMS: #{track.rms}"
puts "Overall ZCR: #{track.zcr}"

# You can also get framed analysis for time-series data:
# framed_rms_data = track.framed_rms(2048, 512) also works
framed_rms_data = track.framed_rms(frame_length: 2048, hop_length: 512)


# Save the results
track.save_track("processed_vocals.wav")

```

## Development

If you want to clone the repo and hack on the C++ guts, we have fully automated the compilation and testing steps.

1. Clone the repo and run `bundle install` to grab the development dependencies.
2. Run `rake test` — this will automatically compile the C++ `extconf.rb` and run the Minitest suite.
3. Run `rake doc:generate ; rake doc:server` — this will compile the YARD stubs into HTML and boot a live-reloading local web server at `http://localhost:8808` so you can read the docs!

## License

The gem is available as open source under the terms of the **MIT License**.
