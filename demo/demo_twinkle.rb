require 'ruby_dsp'

track = RubyDSP::AudioTrack.new('', 1, 44_100)

# frequencies for Twinkle (Hz)
c4 = 261.63
d4 = 293.66
e4 = 329.63
f4 = 349.23
g4 = 392.00
a4 = 440.00

puts 'Sequencing 8-bit Twinkle Twinkle...'

track.add_wave!('square', c4, 0.4, 0.0)
     .add_wave!('square', c4, 0.4, 0.5)
     .add_wave!('square', g4, 0.4, 1.0)
     .add_wave!('square', g4, 0.4, 1.5)
     .add_wave!('square', a4, 0.4, 2.0)
     .add_wave!('square', a4, 0.4, 2.5)
     .add_wave!('square', g4, 0.9, 3.0)
     .add_wave!('square', f4, 0.4, 4.0)
     .add_wave!('square', f4, 0.4, 4.5)
     .add_wave!('square', e4, 0.4, 5.0)
     .add_wave!('square', e4, 0.4, 5.5)
     .add_wave!('square', d4, 0.4, 6.0)
     .add_wave!('square', d4, 0.4, 6.5)
     .add_wave!('square', c4, 0.9, 7.0)
     .normalize!(-35.0)
     .fade_out!(0.5)
     .save_track('twinkle_twinkle.wav')

puts 'Done! Saved to twinkle_8bit.wav'
