# test/ruby_dsp_test.rb
require_relative 'test_helper'

class RubyDSPTest < Minitest::Test # rubocop:disable Style/Documentation
  def setup
    # ASSUMING valid stereo, 44100Hz wav file here
    @fixture_path = File.expand_path('fixtures/test_audio.wav', __dir__)
  end

  def test_that_it_has_a_version_number
    refute_nil ::RubyDSP::VERSION
  end

  def test_initialization_with_missing_file
    error = assert_raises(RuntimeError) do
      RubyDSP::AudioTrack.new('ghost_track.wav')
    end
    assert_match(/Could not process audio file/, error.message)
  end

  def test_initialization_with_default_args
    track = RubyDSP::AudioTrack.new(@fixture_path)

    assert_equal @fixture_path, track.file_name
    assert_equal 2, track.channels
    assert_equal 44_100, track.sample_rate
    refute track.is_mono?
  end

  def test_initialization_with_target_overrides
    # Testing miniaudio's built-in decoding conversion
    track = RubyDSP::AudioTrack.new(@fixture_path, 1, 22_050)

    assert_equal 1, track.channels
    assert_equal 22_050, track.sample_rate
    assert track.is_mono?
  end

  def test_duration_is_calculated_correctly
    track = RubyDSP::AudioTrack.new(@fixture_path)
    assert track.duration > 0.0
    assert_kind_of Float, track.duration
  end

  def test_peak_amp_returns_valid_float
    track = RubyDSP::AudioTrack.new(@fixture_path)
    peak = track.peak_amp

    assert peak >= 0.0
    assert peak <= 1.0
  end

  def test_to_s_formats_correctly
    track = RubyDSP::AudioTrack.new(@fixture_path)
    output = track.to_s

    assert_match(/test_audio\.wav/, output)
    assert_match(/2 channel\(s\)/, output)
    assert_match(/44100Hz/, output)
  end

  def test_to_mono_bang_converts_stereo_to_mono
    track = RubyDSP::AudioTrack.new(@fixture_path)

    # Should perform the conversion and return true
    assert_equal true, track.to_mono!

    # State should be updated
    assert_equal 1, track.channels
    assert track.is_mono?

    # Running it again should be a no-op and return false
    assert_equal false, track.to_mono!
  end

  def test_resample_bang_changes_sample_rate
    track = RubyDSP::AudioTrack.new(@fixture_path)

    # Should perform resampling and return true
    assert_equal true, track.resample!(48_000)

    # State should be updated
    assert_equal 48_000, track.sample_rate

    # Running it with the same target rate should be a no-op
    assert_equal false, track.resample!(48_000)

    # Running it with 0 should be a no-op
    assert_equal false, track.resample!(0)
  end

  def test_overall_rms_returns_array_per_channel
    track = RubyDSP::AudioTrack.new(@fixture_path)
    rms_data = track.rms

    assert rms_data.respond_to?(:to_a)
    assert_equal 2, rms_data.length

    assert rms_data[0] >= 0.0
    assert rms_data[1] >= 0.0
  end

  def test_framed_rms_returns_2d_array
    track = RubyDSP::AudioTrack.new(@fixture_path)
    framed_data = track.framed_rms(2048, 512)

    # Outer vector
    assert_equal 2, framed_data.length

    # Inner vector
    assert framed_data[0].respond_to?(:to_a)
    assert framed_data[0][0] >= 0.0
  end

  def test_framed_rms_edge_cases # rubocop:disable Metrics/AbcSize
    track = RubyDSP::AudioTrack.new(@fixture_path)

    assert_equal [], track.framed_rms(0, 512).to_a
    assert_equal [], track.framed_rms(2048, 0).to_a

    large_frame = track.sample_rate * 600
    fallback_data = track.framed_rms(large_frame, 512)

    assert_equal 2, fallback_data.length
    assert_equal 1, fallback_data[0].length
    assert_equal 1, fallback_data[1].length
  end

  def test_zcr_returns_array_per_channel
    track = RubyDSP::AudioTrack.new(@fixture_path)
    zcr_data = track.zcr

    assert zcr_data.respond_to?(:to_a)
    assert_equal track.channels, zcr_data.length

    assert zcr_data[0] >= 0.0
    assert zcr_data[0] <= 1.0
    assert zcr_data[1] >= 0.0
    assert zcr_data[1] <= 1.0
  end

  def test_framed_zcr_returns_2d_array
    track = RubyDSP::AudioTrack.new(@fixture_path)
    framed_data = track.framed_zcr(2048, 512)

    assert_equal track.channels, framed_data.length

    assert framed_data[0].respond_to?(:to_a)

    assert framed_data[0][0] >= 0.0
    assert framed_data[0][0] <= 1.0
  end

  def test_framed_zcr_edge_cases
    track = RubyDSP::AudioTrack.new(@fixture_path)

    assert_equal [], track.framed_zcr(0, 512).to_a
    assert_equal [], track.framed_zcr(2048, 0).to_a

    large_frame = track.sample_rate * 600
    fallback_data = track.framed_zcr(large_frame, 512)

    assert_equal track.channels, fallback_data.length
    assert_equal 1, fallback_data[0].length
    assert_equal 1, fallback_data[1].length
  end

  def test_exact_zero_crossing_logic
    zero_fixture = File.expand_path('fixtures/exact_zero.wav', __dir__)

    track = RubyDSP::AudioTrack.new(zero_fixture)
    zcr_data = track.zcr

    refute_equal 0.0, zcr_data[0], 'ZCR missed the crossings! Check your 0.0f logic.'
  end

  def test_silence_bounds_returns_array_of_indices
    track = RubyDSP::AudioTrack.new(@fixture_path)

    # Using the default -60.0 dB threshold
    bounds = track.silence_bounds(-60.0)

    assert_kind_of Array, bounds.to_a
    assert_equal 2, bounds.length
    assert_kind_of Integer, bounds[0]
    assert_kind_of Integer, bounds[1]

    assert bounds[0] <= bounds[1], 'Start sample cannot be after end sample'
    assert bounds[1] <= (track.sample_count / track.channels), 'End sample exceeds track length'
  end

  def test_trim_silence_bang_mutates_track
    track = RubyDSP::AudioTrack.new(@fixture_path)
    original_duration = track.duration

    result = track.trim_silence!(-10.0)

    assert_equal true, result
    assert track.duration < original_duration, 'Track duration should decrease after trimming'
  end

  def test_trim_silence_bang_no_op_on_low_threshold
    track = RubyDSP::AudioTrack.new(@fixture_path)
    original_duration = track.duration

    # this is so down it should not trim anything
    result = track.trim_silence!(-999.0)

    assert_equal false, result
    assert_equal original_duration, track.duration, 'Track duration should not change on no-op'
  end

  def test_silence_bounds_on_exact_fixture
    # 3-second mono file with silence on the sides
    fixture = File.expand_path('fixtures/padded_beep.wav', __dir__)
    track = RubyDSP::AudioTrack.new(fixture)

    # Track is 44100 Hz.
    # Silence: 0 to 44100
    # Sine Wave: 44100 to 88200
    # Silence: 88200 to 132300
    bounds = track.silence_bounds(-60.0)

    # within bounds, are we finding it ?
    assert_in_delta 44_100, bounds[0], 2048, 'Start bound should be right at 1.0s'
    assert_in_delta 88_200, bounds[1], 2048, 'End bound should be right at 2.0s'
  end

  def test_save_track_with_explicit_wav_extension
    track = RubyDSP::AudioTrack.new(@fixture_path)

    Dir.mktmpdir do |dir|
      out_path = File.join(dir, 'output.wav')
      result = track.save_track(out_path)

      assert_equal true, result
      assert File.exist?(out_path), "File should be created at #{out_path}"

      # Load it back to verify the data survived the round trip
      saved_track = RubyDSP::AudioTrack.new(out_path)
      assert_equal track.channels, saved_track.channels
      assert_equal track.sample_rate, saved_track.sample_rate
      assert_equal track.samples.size, saved_track.samples.size
      # miniaudio might pad a tiny bit depending on the encoder
      assert_in_delta track.duration, saved_track.duration, 0.01
    end
  end

  def test_save_track_auto_appends_wav_extension
    track = RubyDSP::AudioTrack.new(@fixture_path)

    Dir.mktmpdir do |dir|
      base_path = File.join(dir, 'auto_appended_output')

      # Should return true and append .wav
      assert_equal true, track.save_track(base_path)

      expected_path = "#{base_path}.wav"
      assert File.exist?(expected_path), 'The .wav extension should have been appended'
    end
  end

  def test_save_track_with_forced_format_symbol
    track = RubyDSP::AudioTrack.new(@fixture_path)

    Dir.mktmpdir do |dir|
      out_path = File.join(dir, 'weird_extension.data')

      # Force it to save as WAV and keep .data
      result = track.save_track(out_path, :wav)

      assert_equal true, result
      assert File.exist?(out_path), 'File should be saved exactly as requested'
      refute File.exist?("#{out_path}.wav"), 'It should not double-append extensions if user forces format'

      saved_track = RubyDSP::AudioTrack.new(out_path)
      assert_equal track.sample_rate, saved_track.sample_rate
    end
  end

  def test_save_track_raises_on_unsupported_formats
    track = RubyDSP::AudioTrack.new(@fixture_path)

    Dir.mktmpdir do |dir|
      # Test string extension inference
      error = assert_raises(RuntimeError) do
        track.save_track(File.join(dir, 'output.mp3'))
      end
      assert_match(/mp3 encoding is not yet supported/, error.message)

      # Test symbol forcing
      error2 = assert_raises(RuntimeError) do
        track.save_track(File.join(dir, 'output'), :flac)
      end
      assert_match(/flac encoding is not yet supported/, error2.message)

      # Test total gibberish
      error3 = assert_raises(RuntimeError) do
        track.save_track(File.join(dir, 'output'), :potato)
      end
      assert_match(/Unknown format/, error3.message)
    end
  end
end
