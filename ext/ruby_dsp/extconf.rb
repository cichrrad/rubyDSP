# ext/ruby_dsp/extconf.rb
require 'mkmf-rice'

# You can add flags here, like turning on C++17 or optimization flags
$CXXFLAGS += ' -std=c++17 -O3'

# This actually generates the Makefile
create_makefile('ruby_dsp/ruby_dsp')
