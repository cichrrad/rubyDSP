# Rakefile
require 'bundler/gem_tasks'
require 'rake/extensiontask'
require 'rake/testtask'

Rake::ExtensionTask.new('ruby_dsp') do |ext|
  ext.lib_dir = 'lib/ruby_dsp'
end

Rake::TestTask.new do |t|
  t.libs << 'test'
end

desc 'Run tests'
task test: :compile
