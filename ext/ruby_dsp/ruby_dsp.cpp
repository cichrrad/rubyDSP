#include <rice/rice.hpp>
#include <rice/stl.hpp>

#include <string>
#include <stdexcept>
#include <cmath>
#include <sstream>
#include <iomanip>
#include <algorithm>

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

    bool to_mono_bang()
    {
        if (is_mono)
        {
            return false; // no-op
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
        return true;
    }

    bool resample_bang(unsigned int target_rate = 0)
    {
        if (target_rate == 0 || target_rate == sample_rate)
        {
            return false; // no-op
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

        return true;
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

    bool trim_silence_bang(float threshold_db = -60.0f, unsigned int frame_length = 2048, unsigned int hop_length = 512)
    {
        if (samples.empty())
            return false;

        std::vector<unsigned long long> bounds = silence_bounds(threshold_db, frame_length, hop_length);
        unsigned long long start_sample = bounds[0];
        unsigned long long end_sample = bounds[1];

        unsigned long long per_channel_samples = sample_count / channels;

        // No-op checks
        if (start_sample == 0 && end_sample >= per_channel_samples)
            return false;

        // If the file is entirely silent, clear everything
        if (start_sample == 0 && end_sample == 0)
        {
            samples.clear();
            sample_count = 0;
            return true;
        }

        // Slice the interleaved sample array
        unsigned long long start_idx = start_sample * channels;
        unsigned long long end_idx = end_sample * channels;

        std::vector<float> trimmed_samples(samples.begin() + start_idx, samples.begin() + end_idx);
        samples = std::move(trimmed_samples);
        sample_count = samples.size();

        return true;
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
                                                                   Arg("file_name") = (std::string) "default.wav",
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
                                               .define_method("to_s", &AudioTrack::to_s);
}