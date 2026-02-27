#include <rice/rice.hpp>
#include <rice/stl.hpp>
#include <string>

using namespace Rice;

struct AudioTrack
{
  std::string filename;
  std::string trackname;
  int sample_rate;

  AudioTrack(std::string f, std::string t, int s)
      : filename(f), trackname(t), sample_rate(s) {}

  std::string play()
  {
    return "Playing '" + trackname + "' from file '" + filename + "' at sample rate of " + std::to_string(sample_rate) + "Hz.";
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
                                             .define_constructor(Constructor<AudioTrack, std::string, std::string, int>(),
                                                                 Arg("file_name") = (std::string) "default.wav",
                                                                 Arg("track_name") = (std::string) "Untitled",
                                                                 Arg("sample_rate") = 44100)
                                             .define_attr("file_name", &AudioTrack::filename)
                                             .define_attr("track_name", &AudioTrack::trackname)
                                             .define_attr("sample_rate", &AudioTrack::sample_rate)
                                             .define_method("play", &AudioTrack::play);
}