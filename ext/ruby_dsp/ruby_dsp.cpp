#include <rice/rice.hpp>
#include <rice/stl.hpp>

#include <string>
#include <stdexcept>
#include <cmath>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <cstdlib>

#define MINIAUDIO_IMPLEMENTATION
#include "vendor/miniaudio.h"

using namespace Rice;

std::string get_extension(const std::string &filename)
{
    size_t dot_pos = filename.find_last_of('.');
    if (dot_pos == std::string::npos)
        return ""; // No dot found

    std::string ext = filename.substr(dot_pos + 1);
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    return ext;
}

struct AudioTrack
{
    std::string filename;
    int sample_rate = -1;
    int channels = -1;
    bool is_mono = false;
    std::vector<float> samples;
    unsigned long long sample_count = 0;

    AudioTrack(std::string f, unsigned int target_channels = 0, unsigned int target_sample_rate = 0) : filename(f)
    {

        // empty constructor
        if (filename.empty())
        {
            sample_rate = target_sample_rate > 0 ? target_sample_rate : 44100;
            channels = target_channels > 0 ? target_channels : 1;
            is_mono = (channels == 1);
            sample_count = 0;
            return;
        }

        ma_decoder decoder;
        ma_result result;

        ma_decoder_config config = ma_decoder_config_init(ma_format_f32, (ma_uint32)target_channels, (ma_uint32)target_sample_rate);

        result = ma_decoder_init_file(filename.c_str(), &config, &decoder);

        if (result != MA_SUCCESS)
        {
            throw std::runtime_error("RubyDSP: Could not process audio file: " + filename);
        }

        sample_rate = decoder.outputSampleRate;
        channels = decoder.outputChannels;
        is_mono = (channels == 1);

        ma_uint64 totalFrames;
        if (ma_decoder_get_length_in_pcm_frames(&decoder, &totalFrames) != MA_SUCCESS)
        {
            ma_decoder_uninit(&decoder);
            throw std::runtime_error("RubyDSP: Could not determine track length.");
        }

        sample_count = totalFrames * channels;
        samples.resize(sample_count);

        ma_uint64 framesRead;
        if (ma_decoder_read_pcm_frames(&decoder, samples.data(), totalFrames, &framesRead) != MA_SUCCESS)
        {
            ma_decoder_uninit(&decoder);
            throw std::runtime_error("RubyDSP: Failed to read PCM data.");
        }

        ma_decoder_uninit(&decoder);
    }

    bool save_track(const std::string &outFile, Rice::Symbol format_sym = Rice::Symbol("auto"))
    {
        std::string final_path = outFile;
        std::string format = format_sym.str();
        std::string ext = get_extension(final_path);

        if (format == "auto")
        {
            if (ext.empty())
            {
                format = "wav";
                final_path += ".wav";
            }
            else
            {
                format = ext;
            }
        }
        else
        {
            if (ext.empty())
            {
                final_path += "." + format;
            }
        }

        if (format == "wav")
        {
            if (samples.empty() || channels <= 0 || sample_rate <= 0)
            {
                throw std::runtime_error("RubyDSP: Cannot save an empty or invalid audio track.");
            }

            ma_encoder_config config = ma_encoder_config_init(
                ma_encoding_format_wav,
                ma_format_f32,
                (ma_uint32)channels,
                (ma_uint32)sample_rate);

            ma_encoder encoder;

            ma_result result = ma_encoder_init_file(final_path.c_str(), &config, &encoder);

            if (result != MA_SUCCESS)
            {
                throw std::runtime_error("RubyDSP: Failed to initialize WAV encoder for: " + final_path);
            }

            ma_uint64 framesToWrite = samples.size() / channels;
            ma_uint64 framesWritten = 0;

            result = ma_encoder_write_pcm_frames(&encoder, samples.data(), framesToWrite, &framesWritten);

            ma_encoder_uninit(&encoder);

            if (result != MA_SUCCESS)
            {
                throw std::runtime_error("RubyDSP: Failed to write PCM data to: " + final_path);
            }

            if (framesWritten != framesToWrite)
            {
                throw std::runtime_error("RubyDSP: Incomplete file write to: " + final_path);
            }

            return true;
        }

        // TODO add unsupported formats
        if (format == "flac" || format == "mp3" || format == "vorbis" || format == "ogg")
        {
            throw std::runtime_error("RubyDSP: " + format + " encoding is not yet supported. Only WAV is available.");
        }

        throw std::runtime_error("RubyDSP: Unknown format '" + format + "'");
    }

    float duration()
    {
        return (float)sample_count / (sample_rate * channels);
    }

    float peak_amplitude()
    {
        float max_val = 0.0f;
        for (const auto &sample : samples)
        {
            max_val = std::max(max_val, std::fabs(sample));
        }
        return max_val;
    }

    AudioTrack &to_mono_bang()
    {
        if (is_mono)
        {
            return *this; // no-op
        }

        if (channels < 1)
        {
            throw std::runtime_error("RubyDSP: Wrong number of channels (" + std::to_string(channels) + ")");
        }

        unsigned long long new_size = sample_count / channels;
        std::vector<float> mono_samples;
        mono_samples.reserve(new_size);

        // mean calculation pass
        for (unsigned long long i = 0; i < new_size; ++i)
        {
            float sum = 0.0f;
            // frame pass
            for (int c = 0; c < channels; ++c)
            {
                sum += samples[i * channels + c];
            }
            mono_samples.push_back(sum / channels);
        }

        // replace samples with mono
        samples = std::move(mono_samples);
        channels = 1;
        is_mono = true;
        sample_count = samples.size();
        return *this;
    }

    AudioTrack &resample_bang(unsigned int target_rate = 0)
    {
        if (target_rate == 0 || target_rate == sample_rate)
        {
            return *this; // no-op
        }

        // TODO: add better, linear will have to do for now
        ma_resampler_config config = ma_resampler_config_init(
            ma_format_f32,
            (ma_uint32)channels,
            (ma_uint32)sample_rate,
            (ma_uint32)target_rate,
            ma_resample_algorithm_linear);

        ma_resampler resampler;
        if (ma_resampler_init(&config, NULL, &resampler) != MA_SUCCESS)
        {
            throw std::runtime_error("RubyDSP: Failed to initialize resampler.");
        }

        // Calculate input/output frame counts
        ma_uint64 input_frames = sample_count / channels;
        ma_uint64 expected_output_frames = 0;

        if (ma_resampler_get_expected_output_frame_count(&resampler, input_frames, &expected_output_frames) != MA_SUCCESS)
        {
            ma_resampler_uninit(&resampler, NULL);
            throw std::runtime_error("RubyDSP: Failed to get expected output frame count.");
        }

        std::vector<float> resampled_data(expected_output_frames * channels);

        // Process the audio
        ma_uint64 frames_in = input_frames;
        ma_uint64 frames_out = expected_output_frames;

        if (ma_resampler_process_pcm_frames(&resampler, samples.data(), &frames_in, resampled_data.data(), &frames_out) != MA_SUCCESS)
        {
            ma_resampler_uninit(&resampler, NULL);
            throw std::runtime_error("RubyDSP: Resampling failed during processing.");
        }

        ma_resampler_uninit(&resampler, NULL);

        // Shrink buffer if the resampler output slightly fewer frames than expected
        resampled_data.resize(frames_out * channels);

        // Update internals
        samples = std::move(resampled_data);
        sample_rate = target_rate;
        sample_count = samples.size();

        return *this;
    }

    std::vector<float> rms()
    {
        if (samples.empty())
        {
            return {}; // should not happen
        }

        std::vector<float> result(channels, 0.0f);
        unsigned long long per_channel_samples = sample_count / channels;

        if (per_channel_samples == 0)
        {
            return result;
        }

        // Process each channel
        for (int c = 0; c < channels; ++c)
        {
            double sum_sq = 0.0;

            for (unsigned long long i = 0; i < per_channel_samples; ++i)
            {
                // Access the correct sample in the interleaved array
                float s = samples[i * channels + c];
                sum_sq += s * s;
            }

            result[c] = (float)std::sqrt(sum_sq / per_channel_samples);
        }

        return result;
    }

    std::vector<std::vector<float>> framed_rms(unsigned int frame_length = 2048, unsigned int hop_length = 512)
    {
        if (frame_length == 0 || hop_length == 0 || samples.empty())
        {
            return {};
        }

        unsigned long long per_channel_samples = sample_count / channels;

        // Either SUPER SHORT track or SUPER LONG frame_length
        // --> will be less than single full frame per channel
        // --> fallback to rms wrapped to be 2D
        if (per_channel_samples < frame_length)
        {
            std::vector<float> overall_rms = rms();

            // wrap
            std::vector<std::vector<float>> fallback_result(channels, std::vector<float>(1, 0.0f));

            for (int c = 0; c < channels; ++c)
            {
                fallback_result[c][0] = overall_rms[c];
            }

            return fallback_result;
        }

        // more than single full frame per channel (usual)
        unsigned long long expected_frames = ((per_channel_samples - frame_length) / hop_length) + 1;
        std::vector<std::vector<float>> result(channels, std::vector<float>(expected_frames, 0.0f));

        for (int c = 0; c < channels; ++c)
        {
            for (unsigned long long i = 0; i < expected_frames; ++i)
            {
                unsigned long long start_sample = (i * hop_length) * channels + c;
                double sum_sq = 0.0;

                for (unsigned int j = 0; j < frame_length; ++j)
                {
                    float s = samples[start_sample + (j * channels)];
                    // ^2 to flip all to positive
                    sum_sq += s * s;
                }

                result[c][i] = (float)std::sqrt(sum_sq / frame_length);
            }
        }

        return result;
    }

    std::vector<float> zcr()
    {
        if (samples.empty())
            return {};

        std::vector<float> result(channels, 0.0f);
        unsigned long long per_channel_samples = sample_count / channels;

        if (per_channel_samples < 2)
            return result;

        for (int c = 0; c < channels; ++c)
        {
            unsigned int crossings = 0;
            for (unsigned long long j = 1; j < per_channel_samples; ++j)
            {
                float curr = samples[j * channels + c];
                float prev = samples[(j - 1) * channels + c];

                if ((curr >= 0.0f) != (prev >= 0.0f))
                {
                    crossings++;
                }
            }
            result[c] = (float)crossings / per_channel_samples;
        }
        return result;
    }

    std::vector<std::vector<float>> framed_zcr(unsigned int frame_length = 2048, unsigned int hop_length = 512)
    {

        if (frame_length == 0 || hop_length == 0 || samples.empty())
        {
            return {};
        }

        unsigned long long per_channel_samples = sample_count / channels;

        if (per_channel_samples < frame_length)
        {
            std::vector<float> overall_zcr = zcr();

            // wrap
            std::vector<std::vector<float>> fallback_result(channels, std::vector<float>(1, 0.0f));

            for (int c = 0; c < channels; ++c)
            {
                fallback_result[c][0] = overall_zcr[c];
            }

            return fallback_result;
        }

        // Calculate number of frames
        unsigned long long expected_frames = ((per_channel_samples - frame_length) / hop_length) + 1;

        std::vector<std::vector<float>> result(channels, std::vector<float>(expected_frames, 0.0f));

        for (int c = 0; c < channels; ++c)
        {
            for (unsigned long long i = 0; i < expected_frames; ++i)
            {
                unsigned long long start_sample = (i * hop_length) * channels + c;
                unsigned int crossings = 0;

                for (unsigned int j = 1; j < frame_length; ++j)
                {
                    unsigned long long curr = start_sample + (j * channels);
                    unsigned long long prev = start_sample + ((j - 1) * channels);

                    if ((samples[curr] >= 0.0f) != (samples[prev] >= 0.0f))
                    {
                        crossings++;
                    }
                }
                // Normalize
                result[c][i] = (float)crossings / frame_length;
            }
        }
        return result;
    }

    std::vector<unsigned long long> silence_bounds(float threshold_db = -60.0f, unsigned int frame_length = 2048, unsigned int hop_length = 512)
    {
        if (samples.empty())
            return {0, 0};

        // Get framed RMS
        std::vector<std::vector<float>> rms_frames = framed_rms(frame_length, hop_length);
        if (rms_frames.empty() || rms_frames[0].empty())
        {
            return {0, sample_count / channels};
        }

        unsigned long long num_frames = rms_frames[0].size();

        // Find the global peak RMS across all frames and all channels
        float max_rms = 0.0f;
        for (int c = 0; c < channels; ++c)
        {
            for (unsigned long long i = 0; i < num_frames; ++i)
            {
                if (rms_frames[c][i] > max_rms)
                {
                    max_rms = rms_frames[c][i];
                }
            }
        }

        // Prevent errors on pure silence
        if (max_rms < 1e-10f)
            return {0, 0};

        // Scan from the left to find the start frame
        unsigned long long start_frame = 0;
        bool found_start = false;

        for (unsigned long long i = 0; i < num_frames; ++i)
        {
            float frame_max_rms = 0.0f;
            for (int c = 0; c < channels; ++c)
            {
                if (rms_frames[c][i] > frame_max_rms)
                {
                    frame_max_rms = rms_frames[c][i];
                }
            }

            // Convert to decibels relative to the peak RMS
            float db = 20.0f * std::log10((frame_max_rms / max_rms) + 1e-10f);

            if (db > threshold_db)
            {
                start_frame = i;
                found_start = true;
                break;
            }
        }

        // Scan from the right to find the end frame
        unsigned long long end_frame = num_frames > 0 ? num_frames - 1 : 0;
        if (found_start)
        {
            for (long long i = num_frames - 1; i >= 0; --i)
            {
                float frame_max_rms = 0.0f;
                for (int c = 0; c < channels; ++c)
                {
                    if (rms_frames[c][i] > frame_max_rms)
                    {
                        frame_max_rms = rms_frames[c][i];
                    }
                }

                float db = 20.0f * std::log10((frame_max_rms / max_rms) + 1e-10f);

                if (db > threshold_db)
                {
                    end_frame = i;
                    break;
                }
            }
        }
        else
        {
            return {0, 0}; // Track was entirely below threshold
        }

        // Convert frame indices back to sample indices
        unsigned long long start_sample = start_frame * hop_length;
        unsigned long long end_sample = end_frame * hop_length + frame_length;

        unsigned long long per_channel_samples = sample_count / channels;

        if (start_frame == 0)
        {
            start_sample = 0;
        }

        if (end_frame == num_frames - 1)
        {
            end_sample = per_channel_samples;
        }
        else if (end_sample > per_channel_samples)
        {
            end_sample = per_channel_samples;
        }

        return {start_sample, end_sample};
    }

    AudioTrack &trim_silence_bang(float threshold_db = -60.0f, unsigned int frame_length = 2048, unsigned int hop_length = 512)
    {
        if (samples.empty())
            return *this;

        std::vector<unsigned long long> bounds = silence_bounds(threshold_db, frame_length, hop_length);
        unsigned long long start_sample = bounds[0];
        unsigned long long end_sample = bounds[1];

        unsigned long long per_channel_samples = sample_count / channels;

        // No-op checks
        if (start_sample == 0 && end_sample >= per_channel_samples)
            return *this;

        // If the file is entirely silent, clear everything
        if (start_sample == 0 && end_sample == 0)
        {
            samples.clear();
            sample_count = 0;
            return *this;
        }

        // Slice the interleaved sample array
        unsigned long long start_idx = start_sample * channels;
        unsigned long long end_idx = end_sample * channels;

        std::vector<float> trimmed_samples(samples.begin() + start_idx, samples.begin() + end_idx);
        samples = std::move(trimmed_samples);
        sample_count = samples.size();

        return *this;
    }

    AudioTrack &normalize_bang(float target_db = -1.0f)
    {
        if (samples.empty())
            return *this;

        float current_peak = peak_amplitude();
        if (current_peak <= 0.0f)
            return *this; // silent track, nothing to scale

        // convert target dB to a linear multiplier
        float target_linear = std::pow(10.0f, target_db / 20.0f);
        float scale_factor = target_linear / current_peak;

        // already at the target peak -- do nothing
        if (std::abs(scale_factor - 1.0f) < 1e-5f)
            return *this;

        for (auto &sample : samples)
        {
            sample *= scale_factor;
        }

        return *this;
    }

    AudioTrack &fade_in_bang(float duration_sec)
    {
        if (samples.empty() || duration_sec <= 0.0f)
            return *this;

        unsigned long long fade_frames = (unsigned long long)(duration_sec * sample_rate);
        unsigned long long total_frames = sample_count / channels;

        // Prevent fading longer than the track itself
        if (fade_frames > total_frames)
            fade_frames = total_frames;

        for (unsigned long long i = 0; i < fade_frames; ++i)
        {
            float multiplier = (float)i / (float)fade_frames;
            for (int c = 0; c < channels; ++c)
            {
                samples[i * channels + c] *= multiplier;
            }
        }
        return *this;
    }

    AudioTrack &fade_out_bang(float duration_sec)
    {
        if (samples.empty() || duration_sec <= 0.0f)
            return *this;

        unsigned long long fade_frames = (unsigned long long)(duration_sec * sample_rate);
        unsigned long long total_frames = sample_count / channels;

        if (fade_frames > total_frames)
            fade_frames = total_frames;

        unsigned long long start_frame = total_frames - fade_frames;

        for (unsigned long long i = 0; i < fade_frames; ++i)
        {
            float multiplier = 1.0f - ((float)i / (float)fade_frames);
            unsigned long long frame_idx = start_frame + i;
            for (int c = 0; c < channels; ++c)
            {
                samples[frame_idx * channels + c] *= multiplier;
            }
        }
        return *this;
    }

    AudioTrack &pad_bang(float head_sec = 0.0f, float tail_sec = 0.0f)
    {
        if (head_sec <= 0.0f && tail_sec <= 0.0f)
            return *this;

        unsigned long long head_frames = (unsigned long long)(head_sec * sample_rate);
        unsigned long long tail_frames = (unsigned long long)(tail_sec * sample_rate);

        unsigned long long head_samples = head_frames * channels;
        unsigned long long tail_samples = tail_frames * channels;

        // pad the beginning
        if (head_samples > 0)
        {
            samples.insert(samples.begin(), head_samples, 0.0f);
        }

        // pad the end
        if (tail_samples > 0)
        {
            samples.insert(samples.end(), tail_samples, 0.0f);
        }

        sample_count = samples.size();

        return *this;
    }

    AudioTrack &pad_to_duration_bang(float target_duration_sec)
    {
        if (target_duration_sec <= 0.0f)
            return *this;

        unsigned long long current_frames = sample_count / channels;
        unsigned long long target_frames = (unsigned long long)(target_duration_sec * sample_rate);

        // track is already long enough, do nothing
        if (target_frames <= current_frames)
            return *this;

        unsigned long long diff_frames = target_frames - current_frames;

        // split the difference `evenly`
        unsigned long long head_frames = diff_frames / 2;
        unsigned long long tail_frames = diff_frames - head_frames;

        unsigned long long head_samples = head_frames * channels;
        unsigned long long tail_samples = tail_frames * channels;

        // pad the beginning
        if (head_samples > 0)
        {
            samples.insert(samples.begin(), head_samples, 0.0f);
        }

        // pad the end
        if (tail_samples > 0)
        {
            samples.insert(samples.end(), tail_samples, 0.0f);
        }

        sample_count = samples.size();

        return *this;
    }

    AudioTrack &add_wave_bang(std::string wave_type, float frequency, float duration_sec, float start_sec = -1.0f, float amplitude = 1.0f)
    {
        if (duration_sec <= 0.0f)
            return *this;

        // If no start time is provided (-1.0), append to the very end of the track
        if (start_sec < 0.0f)
        {
            start_sec = duration();
        }

        unsigned long long start_sample = (unsigned long long)(start_sec * sample_rate);
        unsigned long long wave_samples = (unsigned long long)(duration_sec * sample_rate);
        unsigned long long end_sample = start_sample + wave_samples;

        // Dynamically grow the track if this wave pushes past the current end
        unsigned long long required_samples = end_sample * channels;
        if (required_samples > samples.size())
        {
            samples.resize(required_samples, 0.0f);
            sample_count = samples.size();
        }

        // Generate and MIX the wave
        for (unsigned long long i = 0; i < wave_samples; ++i)
        {
            float t = (float)i / sample_rate; // Time in seconds
            float sample_val = 0.0f;

            if (wave_type == "sine")
            {
                sample_val = std::sin(2.0f * M_PI * frequency * t);
            }
            else if (wave_type == "square")
            {
                sample_val = std::sin(2.0f * M_PI * frequency * t) >= 0.0f ? 1.0f : -1.0f;
            }
            else if (wave_type == "sawtooth")
            {
                sample_val = 2.0f * std::fmod(t * frequency, 1.0f) - 1.0f;
            }
            else if (wave_type == "noise")
            {
                sample_val = ((float)std::rand() / RAND_MAX) * 2.0f - 1.0f;
            }
            sample_val *= amplitude;

            // Mix into all channels at the correct offset
            unsigned long long current_idx = start_sample + i;
            for (int c = 0; c < channels; ++c)
            {
                samples[current_idx * channels + c] += sample_val;
            }
        }

        return *this;
    }

    std::string to_s()
    {
        std::ostringstream stream;
        stream << "['" << filename << "', "
               << std::fixed << std::setprecision(3) << duration() << "s duration, "
               << channels << " channel(s), "
               << sample_rate << "Hz sample rate]";
        return stream.str();
    }
};

extern "C"
#if defined(_WIN32)
    __declspec(dllexport)
#else
    __attribute__((visibility("default")))
#endif
    void Init_ruby_dsp()
{
    Module rb_mRubyDSP = define_module("RubyDSP");
    Data_Type<AudioTrack> rb_cAudioTrack = define_class_under<AudioTrack>(rb_mRubyDSP, "AudioTrack")
                                               .define_constructor(Constructor<AudioTrack, std::string, unsigned int, unsigned int>(),
                                                                   Arg("file_name") = (std::string) "",
                                                                   Arg("target_channels") = (unsigned int)0,
                                                                   Arg("target_sample_rate") = (unsigned int)0)
                                               // attributes
                                               .define_attr("file_name", &AudioTrack::filename, Rice::AttrAccess::Read)
                                               .define_attr("channels", &AudioTrack::channels, Rice::AttrAccess::Read)
                                               .define_attr("samples", &AudioTrack::samples, Rice::AttrAccess::Read)
                                               .define_attr("sample_count", &AudioTrack::sample_count, Rice::AttrAccess::Read)
                                               .define_attr("sample_rate", &AudioTrack::sample_rate, Rice::AttrAccess::Read)
                                               .define_attr("is_mono?", &AudioTrack::is_mono, Rice::AttrAccess::Read)
                                               // methods
                                               .define_method("duration", &AudioTrack::duration)
                                               .define_method("peak_amp", &AudioTrack::peak_amplitude)
                                               .define_method("to_mono!", &AudioTrack::to_mono_bang)
                                               .define_method("resample!", &AudioTrack::resample_bang,
                                                              Arg("target_rate") = (unsigned int)0)
                                               .define_method("rms", &AudioTrack::rms)
                                               .define_method("framed_rms", &AudioTrack::framed_rms,
                                                              Arg("frame_length") = (unsigned int)2048,
                                                              Arg("hop_length") = (unsigned int)512)
                                               .define_method("zcr", &AudioTrack::zcr)
                                               .define_method("framed_zcr", &AudioTrack::framed_zcr,
                                                              Arg("frame_length") = (unsigned int)2048,
                                                              Arg("hop_length") = (unsigned int)512)
                                               .define_method("silence_bounds", &AudioTrack::silence_bounds,
                                                              Arg("threshold_db") = -60.0f,
                                                              Arg("frame_length") = (unsigned int)2048,
                                                              Arg("hop_length") = (unsigned int)512)
                                               .define_method("trim_silence!", &AudioTrack::trim_silence_bang,
                                                              Arg("threshold_db") = -60.0f,
                                                              Arg("frame_length") = (unsigned int)2048,
                                                              Arg("hop_length") = (unsigned int)512)
                                               .define_method("save_track", &AudioTrack::save_track,
                                                              Arg("out_file"), // (no default -- duh)
                                                              Arg("format") = Symbol("auto"))
                                               .define_method("normalize!", &AudioTrack::normalize_bang,
                                                              Arg("target_db") = -10.0f)
                                               .define_method("fade_in!", &AudioTrack::fade_in_bang,
                                                              Arg("duration_sec"))
                                               .define_method("fade_out!", &AudioTrack::fade_out_bang,
                                                              Arg("duration_sec"))
                                               .define_method("pad!", &AudioTrack::pad_bang,
                                                              Arg("head_sec") = 0.0f,
                                                              Arg("tail_sec") = 0.0f)
                                               .define_method("pad_to_duration!", &AudioTrack::pad_to_duration_bang,
                                                              Arg("target_duration_sec"))
                                               .define_method("add_wave!", &AudioTrack::add_wave_bang,
                                                              Arg("wave_type"),
                                                              Arg("frequency"),
                                                              Arg("duration_sec"),
                                                              Arg("start_sec") = -1.0f,
                                                              Arg("amplitude") = 1.0f)
                                               .define_method("to_s", &AudioTrack::to_s);
}