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
end
