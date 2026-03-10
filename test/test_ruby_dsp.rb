# test/ruby_dsp_test.rb
require_relative 'test_helper'

class RubyDSPTest < Minitest::Test
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

    track.to_mono!

    # State should be updated
    assert_equal 1, track.channels
    assert track.is_mono?
  end

  def test_resample_bang_changes_sample_rate
    track = RubyDSP::AudioTrack.new(@fixture_path)

    track.resample!(48_000)

    # State should be updated
    assert_equal 48_000, track.sample_rate

    # Running it with the same target rate should be a no-op
    track.resample!(48_000)

    assert_equal 48_000, track.sample_rate
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

    track.trim_silence!(-10.0)

    assert track.duration < original_duration, 'Track duration should decrease after trimming'
  end

  def test_trim_silence_bang_no_op_on_low_threshold
    track = RubyDSP::AudioTrack.new(@fixture_path)
    original_duration = track.duration

    # this is so down it should not trim anything
    track.trim_silence!(-999.0)

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
      track.save_track(out_path)

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

      # Should append .wav
      track.save_track(base_path)

      expected_path = "#{base_path}.wav"
      assert File.exist?(expected_path), 'The .wav extension should have been appended'
    end
  end

  def test_save_track_with_forced_format_symbol
    track = RubyDSP::AudioTrack.new(@fixture_path)

    Dir.mktmpdir do |dir|
      out_path = File.join(dir, 'weird_extension.data')

      # Force it to save as WAV and keep .data
      track.save_track(out_path, :wav)

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

  def test_normalize_bang_scales_peak_amplitude
    track = RubyDSP::AudioTrack.new(@fixture_path)

    # Normalize to 0 -> 1.0 peak
    track.normalize!(0.0)

    assert_in_delta 1.0, track.peak_amp, 0.0001, 'Peak amplitude should be exactly 1.0 after normalizing to 0 dB'
  end

  def test_fade_in_bang_applies_fade
    track = RubyDSP::AudioTrack.new(@fixture_path)

    track.fade_in!(0.5)

    # very first sample of a fade-in should be multiplied by 0.0
    assert_equal 0.0, track.samples.first
  end

  def test_fade_out_bang_applies_fade
    track = RubyDSP::AudioTrack.new(@fixture_path)

    track.fade_out!(0.5)

    # very last sample of a fade-out should be multiplied by 0.0
    assert_in_delta 0.0, track.samples.last, 0.0001
  end

  def test_pad_bang_adds_silence_to_ends
    track = RubyDSP::AudioTrack.new(@fixture_path)
    original_duration = track.duration

    # Pad 1 second to the head, 2 seconds to the tail
    track.pad!(1.0, 2.0)

    assert_in_delta original_duration + 3.0, track.duration, 0.001

    # Calling with 0 should be a no-op
    track.pad!(0.0, 0.0)
    assert_in_delta original_duration + 3.0, track.duration, 0.001
  end

  def test_pad_to_duration_bang_centers_audio
    track = RubyDSP::AudioTrack.new(@fixture_path)
    target_duration = track.duration + 2.0

    track.pad_to_duration!(target_duration)

    assert_in_delta target_duration, track.duration, 0.001

    # first and last samples should now be padding (silence)
    assert_equal 0.0, track.samples.first
    assert_equal 0.0, track.samples.last

    # calling with a shorter duration should be a safe no-op
    track.pad_to_duration!(1.0)
    assert track.duration, target_duration
  end

  def test_pad_bang_preserves_original_data_integrity
    track = RubyDSP::AudioTrack.new(@fixture_path)

    original_count = track.sample_count
    channels = track.channels
    sample_rate = track.sample_rate

    # sequence of 10 samples from the very beginning and end
    original_head_sequence = track.samples[0, 10]
    original_tail_sequence = track.samples[-10, 10]

    # pad 1 second to the head, 0.5 seconds to the tail
    head_pad_sec = 1.0
    tail_pad_sec = 0.5
    track.pad!(head_pad_sec, tail_pad_sec)

    # calculate the exact number of samples pushed to the front
    head_pad_samples = (head_pad_sec * sample_rate).to_i * channels

    # check that the absolute beginning is now pure silence
    assert_equal Array.new(10, 0.0), track.samples[0, 10]

    # check that the original head sequence was shifted perfectly
    shifted_head_sequence = track.samples[head_pad_samples, 10]
    assert_equal original_head_sequence, shifted_head_sequence, 'Original start data was corrupted during head pad'

    # check that the original tail sequence is perfectly intact just before the new tail padding
    shifted_tail_start_idx = head_pad_samples + original_count - 10
    shifted_tail_sequence = track.samples[shifted_tail_start_idx, 10]
    assert_equal original_tail_sequence, shifted_tail_sequence, 'Original end data was corrupted during tail pad'

    # check that the absolute end is now pure silence
    assert_equal Array.new(10, 0.0), track.samples[-10, 10]
  end

  def test_pad_to_duration_bang_centers_data_accurately
    track = RubyDSP::AudioTrack.new(@fixture_path)

    original_count = track.sample_count
    original_head = track.samples[0, 5]
    original_tail = track.samples[-5, 5]

    target_duration = track.duration + 2.0
    track.pad_to_duration!(target_duration)

    # pad_to_duration splits the difference evenly, we expect exactly 1 second on the head
    expected_head_samples = (1.0 * track.sample_rate).to_i * track.channels

    # original start data is perfectly preserved at the new offset
    assert_equal original_head, track.samples[expected_head_samples, 5], 'Original start data was corrupted'

    # original end data is perfectly preserved just before the tail padding
    expected_tail_start_idx = expected_head_samples + original_count - 5
    assert_equal original_tail, track.samples[expected_tail_start_idx, 5], 'Original end data was corrupted'
  end

  def test_fade_in_bang_mutates_samples_on_linear_curve
    track = RubyDSP::AudioTrack.new(@fixture_path)

    fade_sec = 0.5
    fade_frames = (fade_sec * track.sample_rate).to_i

    # frame exactly halfway through the fade
    mid_frame = fade_frames / 2
    sample_index = mid_frame * track.channels

    original_val = track.samples[sample_index]

    # linear multiplier at exactly halfway should be ~0.5
    expected_multiplier = mid_frame.to_f / fade_frames
    expected_val = original_val * expected_multiplier

    track.fade_in!(fade_sec)

    assert_equal 0.0, track.samples.first, 'First sample should be fully faded (0.0)'
    assert_in_delta expected_val, track.samples[sample_index], 0.0001, 'Mid-fade sample did not scale correctly'
  end

  def test_fade_out_bang_mutates_samples_on_linear_curve
    track = RubyDSP::AudioTrack.new(@fixture_path)

    fade_sec = 0.5
    total_frames = track.sample_count / track.channels
    fade_frames = (fade_sec * track.sample_rate).to_i

    # find the starting frame of the fade out, then step halfway into it
    start_frame = total_frames - fade_frames
    mid_frame = start_frame + (fade_frames / 2)
    sample_index = mid_frame * track.channels

    original_val = track.samples[sample_index]

    expected_multiplier = 1.0 - ((mid_frame - start_frame).to_f / fade_frames)
    expected_val = original_val * expected_multiplier

    track.fade_out!(fade_sec)

    assert_in_delta expected_val, track.samples[sample_index], 0.0001, 'Mid-fade sample did not scale correctly'
    assert_in_delta 0.0, track.samples.last, 0.0001, 'Last sample should be fully faded (0.0)'
  end

  def test_method_chaining_works_fluently
    track = RubyDSP::AudioTrack.new(@fixture_path)

    # Store the result of a chain
    result = track.to_mono!.normalize!(0.0).fade_in!(0.5)

    # Prove that the final result is the exact same object in memory
    assert_same track, result, 'Method chain did not return the original track instance'

    # Verify the mutations actually happened through the chain
    assert track.is_mono?
    assert_in_delta 1.0, track.peak_amp, 0.0001
    assert_equal 0.0, track.samples.first
  end

  def test_blank_constructor_initializes_empty_track
    # Create a blank canvas
    track = RubyDSP::AudioTrack.new('', 1, 44_100)

    assert_equal 0.0, track.duration
    assert_equal 0, track.sample_count
    assert_equal 1, track.channels
    assert_equal 44_100, track.sample_rate
  end

  def test_add_wave_bang_implicit_append
    track = RubyDSP::AudioTrack.new('', 1, 44_100)

    # Add a 1-second wave (no start time provided)
    track.add_wave!('sine', 440.0, 1.0)
    assert_in_delta 1.0, track.duration, 0.001

    # Add a 0.5-second wave (should append to the end)
    track.add_wave!('square', 220.0, 0.5)
    assert_in_delta 1.5, track.duration, 0.001
  end

  def test_add_wave_bang_dynamic_resizing
    track = RubyDSP::AudioTrack.new('', 1, 44_100)

    # Insert a 1-second wave starting at exactly 2.0 seconds
    track.add_wave!('sawtooth', 440.0, 1.0, 2.0)

    # The track should now be exactly 3.0 seconds long
    assert_in_delta 3.0, track.duration, 0.001

    # The first two seconds should be pure silence (0.0)
    silence_samples = 2 * 44_100
    assert_equal 0.0, track.samples.first
    assert_equal 0.0, track.samples[silence_samples - 1]

    # But right at the 2.0 second mark, we should have audio data
    refute_equal 0.0, track.samples[silence_samples + 10]
  end

  def test_add_wave_bang_polyphony_mixing
    track = RubyDSP::AudioTrack.new('', 1, 44_100)

    # Add a sine wave with exactly 0.4 amplitude
    track.add_wave!('sine', 440.0, 1.0, 0.0, 0.4)
    assert_in_delta 0.4, track.peak_amp, 0.0001

    # Add a square wave directly on top of it, also at 0.0 timestamp, with 0.4 amplitude
    track.add_wave!('square', 220.0, 1.0, 0.0, 0.4)

    # If polyphony works (+=), the peak amplitude should now be higher than 0.4
    # (In this specific case, it should max out around 0.8)
    peak = track.peak_amp
    assert peak > 0.4, 'Wave was overwritten instead of mixed!'
    assert_in_delta 0.8, peak, 0.0001
  end

  def test_add_wave_bang_returns_self_for_chaining
    track = RubyDSP::AudioTrack.new('', 1, 44_100)

    # Prove the fluent interface works for the generator
    result = track.add_wave!('noise', 0.0, 1.0)

    assert_same track, result, 'add_wave! must return the track instance for chaining'
  end
end
