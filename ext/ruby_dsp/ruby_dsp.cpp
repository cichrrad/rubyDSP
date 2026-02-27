#include <rice/rice.hpp>
#include <rice/stl.hpp>
#include <string>
#include <stdexcept>
#include <cmath>

#define MINIAUDIO_IMPLEMENTATION
#include "vendor/miniaudio.h"

using namespace Rice;

struct AudioTrack
{
    std::string filename;
    int sample_rate = -1;
    int channels = -1;
    bool is_mono = false;

    // Hidden from Ruby =================
    std::vector<float> samples;
    unsigned long long sample_count = 0;
    // ==================================


    AudioTrack(std::string f, unsigned int target_channels = 0) : filename(f)
    {
        ma_decoder decoder;
        ma_result result;

        ma_decoder_config config = ma_decoder_config_init(ma_format_f32, (ma_uint32)target_channels, 0);

        result = ma_decoder_init_file(filename.c_str(), &config, &decoder);

        if (result != MA_SUCCESS)
        {
            throw std::runtime_error("RubyDSP: Could not open audio file: " + filename);
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

    float duration()
    {
        return (float)samples.size() / (sample_rate * channels);
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

    std::string to_s()
    {
        return "['"+filename+"', "+std::to_string(channels)+" channel(s), "+std::to_string(sample_rate)+"Hz sample rate]";
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
                                               .define_constructor(Constructor<AudioTrack, std::string, bool>(),
                                                                   Arg("file_name") = (std::string) "default.wav",
                                                                   Arg("mono") = (bool)false)
                                               // attributes
                                               .define_attr("file_name", &AudioTrack::filename, Rice::AttrAccess::Read)
                                               .define_attr("channels", &AudioTrack::channels, Rice::AttrAccess::Read)
                                               .define_attr("sample_rate", &AudioTrack::sample_rate, Rice::AttrAccess::Read)
                                               .define_attr("is_mono?", &AudioTrack::is_mono, Rice::AttrAccess::Read)
                                               // methods
                                               .define_method("duration", &AudioTrack::duration)
                                               .define_method("peak_amp", &AudioTrack::peak_amplitude)
                                               .define_method("to_s", &AudioTrack::to_s);
}