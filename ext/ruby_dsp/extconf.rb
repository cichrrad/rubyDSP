# ext/ruby_dsp/extconf.rb
require 'mkmf-rice'

$CXXFLAGS += ' -std=c++17 -O3'

create_makefile('ruby_dsp/ruby_dsp')
