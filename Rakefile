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

namespace :doc do
  desc 'Generate static HTML documentation in the doc/ folder'
  task :generate do
    puts 'Generating YARD documentation...'
    sh 'yard doc'
  end

  desc 'Start the YARD documentation server with live reload'
  task :server do
    puts 'Starting YARD live server...'
    puts 'Open http://localhost:8808 in your browser.'
    puts 'Press Ctrl+C to stop.'
    exec 'yard server --reload'
  end
end
