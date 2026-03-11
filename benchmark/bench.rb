# frozen_string_literal: false

require 'benchmark/ips'
require 'ruby_dsp'
require 'memory_profiler'

# MOCK NATIVE RUBY IMPLEMENTATION for 1 channel audio
class PureRubyAudioTrack
  attr_accessor :samples, :channels, :sample_rate

  def initialize(channels = 1, sample_rate = 44_100)
    @channels = channels
    @sample_rate = sample_rate
    @samples = []
  end

  # Pure Ruby RMS calculation
  def rms
    return [0.0] if @samples.empty?

    sum_sq = @samples.sum { |s| s * s }
    [Math.sqrt(sum_sq / @samples.size)]
  end

  # Pure Ruby array mutation
  def normalize!(target_db = -1.0)
    return self if @samples.empty?

    peak = @samples.map(&:abs).max
    return self if peak <= 0.0

    target_linear = 10.0**(target_db / 20.0)
    scale_factor = target_linear / peak

    @samples.map! { |s| s * scale_factor }
    self
  end

  def duration
    @samples.size.to_f / @sample_rate
  end

  # optimized Pure Ruby framed RMS
  def framed_rms(frame_length = 2048, hop_length = 512)
    return [] if @samples.empty?

    return [rms] if @samples.size < frame_length

    expected_frames = ((@samples.size - frame_length) / hop_length) + 1
    result = Array.new(expected_frames)

    expected_frames.times do |i|
      start_idx = i * hop_length
      # Array slicing optimized in MRI
      frame = @samples[start_idx, frame_length]
      # .sum is implemented in C is faster than manual iteration
      sum_sq = frame.sum { |s| s * s }
      result[i] = Math.sqrt(sum_sq / frame_length)
    end

    [result] # Wrap in 2D array to match our API
  end

  # optimized Pure Ruby add_wave (just sine for the benchmark -- discards wave_type)
  def add_wave!(_wave_type, frequency, duration_sec, start_sec = -1.0, amplitude = 1.0)
    start_sec = duration if start_sec < 0.0

    start_sample = (start_sec * @sample_rate).to_i
    wave_samples = (duration_sec * @sample_rate).to_i
    end_sample = start_sample + wave_samples

    # memory pre-allocation using MRI's internal C fill
    @samples.fill(0.0, @samples.size...end_sample) if end_sample > @samples.size

    two_pi_freq = 2.0 * Math::PI * frequency

    wave_samples.times do |i|
      t = i.to_f / @sample_rate
      val = Math.sin(two_pi_freq * t) * amplitude

      @samples[start_sample + i] += val
    end

    self
  end
end

# TEST DATA
DURATION = 10.0
FREQ = 440.0

puts 'Generating test data...'

# Setup C++ Track
cpp_track = RubyDSP::AudioTrack.new('', 1, 44_100)
cpp_track.add_wave!('sine', FREQ, DURATION)

# Setup Native Ruby Track
ruby_track = PureRubyAudioTrack.new(1, 44_100)
ruby_track.add_wave!('sine', FREQ, DURATION)

puts "Data generated! Starting benchmarks...\n\n"

# RUNTIME

Benchmark.ips do |x|
  x.config(time: 5, warmup: 2)

  x.report('Read-Only (RMS) - Ruby') { ruby_track.rms }
  x.report('Read-Only (RMS) - C++ ') { cpp_track.rms }

  x.compare!
end

puts "\n#{'-' * 50}\n\n"

Benchmark.ips do |x|
  x.config(time: 5, warmup: 2)

  x.report('Mutation (Normalize) - Ruby') { ruby_track.normalize!(-3.0) }
  x.report('Mutation (Normalize) - C++ ') { cpp_track.normalize!(-3.0) }

  x.compare!
end

Benchmark.ips do |x|
  x.config(time: 5, warmup: 2)

  x.report('Complex (Framed RMS) - Ruby') { ruby_track.framed_rms(2048, 512) }
  x.report('Complex (Framed RMS) - C++ ') { cpp_track.framed_rms(2048, 512) }

  x.compare!
end

puts "\n#{'-' * 50}\n\n"

Benchmark.ips do |x|
  x.config(time: 5, warmup: 2)

  # Adding a 2-second wave into the middle of the track
  x.report('Dynamic (Add Wave) - Ruby') { ruby_track.add_wave!('sine', 880.0, 2.0, 4.0, 0.5) }
  x.report('Dynamic (Add Wave) - C++ ') { cpp_track.add_wave!('sine', 880.0, 2.0, 4.0, 0.5) }

  x.compare!
end

# MEMORY

puts "\n#{'=' * 50}"
puts 'MEMORY PROFILING: Read-Only (RMS)'
puts '=' * 50

# Profile Ruby RMS
puts "\n[ Profiling Ruby RMS... ]"
report_rms_ruby = MemoryProfiler.report { ruby_track.rms }
report_rms_ruby.pretty_print(scale_bytes: false, color_output: false)

# Profile C++ RMS
puts "\n[ Profiling C++ RMS... ]"
report_rms_cpp = MemoryProfiler.report { cpp_track.rms }
report_rms_cpp.pretty_print(scale_bytes: false, color_output: false)

puts "\n#{'=' * 50}"
puts 'MEMORY PROFILING: Mutation (Normalize)'
puts '=' * 50

# Profile Ruby Normalize
puts "\n[ Profiling Ruby Normalize... ]"
report_norm_ruby = MemoryProfiler.report { ruby_track.normalize!(-3.0) }
report_norm_ruby.pretty_print(scale_bytes: false, color_output: false)

# Profile C++ Normalize
puts "\n[ Profiling C++ Normalize... ]"
report_norm_cpp = MemoryProfiler.report { cpp_track.normalize!(-3.0) }
report_norm_cpp.pretty_print(scale_bytes: false, color_output: false)

puts "\n#{'=' * 50}"
puts 'MEMORY PROFILING: Complex (Framed RMS)'
puts '=' * 50

# Profile Ruby Framed RMS
puts "\n[ Profiling Ruby Framed RMS... ]"
report_framed_ruby = MemoryProfiler.report { ruby_track.framed_rms }
report_framed_ruby.pretty_print(scale_bytes: false, color_output: false)

# Profile C++ Framed RMS
puts "\n[ Profiling C++ Framed RMS... ]"
report_framed_cpp = MemoryProfiler.report { cpp_track.framed_rms }
report_framed_cpp.pretty_print(scale_bytes: false, color_output: false)
