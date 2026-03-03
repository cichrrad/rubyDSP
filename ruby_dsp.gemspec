# frozen_string_literal: true

require_relative 'lib/ruby_dsp/version'

Gem::Specification.new do |s|
  s.name        = 'ruby_dsp'
  s.version     = RubyDSP::VERSION
  s.summary     = 'Very basic audio/DSP gem with C++ guts for speed'
  s.description = 'Basically just a Ruby wrapper for miniaudio + some audio/DSP features.'
  s.authors     = ['Radek C.']
  s.email       = 'cichrrad@cvut.cz'
  s.homepage    = 'https://github.com/cichrrad/ruby_dsp'
  s.license     = 'MIT'

  s.required_ruby_version = '>= 3.0.0'

  s.require_paths = ['lib']
  s.extensions    = ['ext/ruby_dsp/extconf.rb']

  s.files         = `git ls-files -z`.split("\x0").reject { |f| f.match(%r{^(test|spec|features)/}) }

  s.add_dependency 'rice', '~> 4.11.2'

  s.add_development_dependency 'rack'
  s.add_development_dependency 'rackup'
  s.add_development_dependency 'rake-compiler'
  s.add_development_dependency 'webrick'
  s.add_development_dependency 'yard'
end
