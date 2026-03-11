# frozen_string_literal: true

require_relative 'lib/ruby_dsp/version'

Gem::Specification.new do |s|
  s.name        = 'ruby_dsp'
  s.version     = RubyDSP::VERSION
  s.summary     = 'A fast, zero-dependency audio processing and synthesis hobby gem built on Rice and miniaudio (C++).'
  s.description = 'RubyDSP is a small gem for rudimentary audio processing, DSP, and synthesis. It aims to have basically zero dependencies (AND WARRANTIES)! See Documentation for more.' # rubocop:disable Layout/LineLength
  s.authors     = ['Radek C.']
  s.email       = 'cichrrad@cvut.cz'
  s.homepage    = 'https://github.com/cichrrad/rubyDSP'
  s.license     = 'MIT'

  s.required_ruby_version = '>= 3.0.0'

  s.require_paths = ['lib']
  s.extensions    = ['ext/ruby_dsp/extconf.rb']

  s.files         = `git ls-files -z`.split("\x0").reject { |f| f.match(%r{^(test|spec|features)/}) }

  s.add_dependency 'rice', '~> 4.11.2'

  s.add_development_dependency 'benchmark', '~> 0.5.0'
  s.add_development_dependency 'benchmark-ips', '~> 2.14'
  s.add_development_dependency 'memory_profiler', '~> 1.1'
  s.add_development_dependency 'minitest', '~> 6.0'
  s.add_development_dependency 'rack', '~> 3.2'
  s.add_development_dependency 'rackup', '~> 2.3'
  s.add_development_dependency 'rake', '~> 13.0'
  s.add_development_dependency 'rake-compiler', '~> 1.3'
  s.add_development_dependency 'rubocop', '~> 1.85'
  s.add_development_dependency 'webrick', '~> 1.9'
  s.add_development_dependency 'yard', '~> 0.9'
end
