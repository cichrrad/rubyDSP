require 'ruby_dsp'

track = RubyDSP::AudioTrack.new('', 1, 44_100)

# Frequencies for Tetris (Hz)
e5 = 659.25
b4 = 493.88
c5 = 523.25
d5 = 587.33
a4 = 440.00

puts 'Sequencing Tetris (Korobeiniki)...'

track.add_wave!('sawtooth', e5, 0.30, 0.0)
     .add_wave!('sawtooth', b4, 0.15, 0.4)
     .add_wave!('sawtooth', c5, 0.15, 0.6)
     .add_wave!('sawtooth', d5, 0.30, 0.8)
     .add_wave!('sawtooth', c5, 0.15, 1.2)
     .add_wave!('sawtooth', b4, 0.15, 1.4)
     .add_wave!('sawtooth', a4, 0.30, 1.6)
     .add_wave!('sawtooth', a4, 0.15, 2.0)
     .add_wave!('sawtooth', c5, 0.15, 2.2)
     .add_wave!('sawtooth', e5, 0.30, 2.4)
     .normalize!(-35.0)
     .save_track('tetris_theme.wav')

puts 'Done! Saved to tetris_theme.wav'
