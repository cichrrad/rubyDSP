require 'ruby_dsp'

track = RubyDSP::AudioTrack.new('', 1, 44_100)

# Frequencies for Mario (Hz)
e6 = 1318.51
c6 = 1046.50
g6 = 1567.98
g5 = 783.99

puts 'Sequencing Super Mario Bros Intro...'

track.add_wave!('square', e6, 0.1, 0.00)
     .add_wave!('square', e6, 0.1, 0.15)
     .add_wave!('square', e6, 0.1, 0.45)
     .add_wave!('square', c6, 0.1, 0.75)
     .add_wave!('square', e6, 0.1, 0.90)
     .add_wave!('square', g6, 0.1, 1.20)
     .add_wave!('square', g5, 0.1, 1.80)
     .normalize!(-35.0)
     .save_track('mario_intro.wav')

puts 'Done! Saved to mario_intro.wav'
