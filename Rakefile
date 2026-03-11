# Rakefile
require 'bundler/gem_tasks'
require 'rake/extensiontask'
require 'rake/testtask'
require 'securerandom'

namespace :bench do
  desc 'Run benchmark, save to UUID log, and display a parsed summary'
  task :summary do
    log_filename = "benchmark/logs/bench_#{SecureRandom.uuid}.log"

    puts 'Running benchmarks... This will take a moment (~1 min).'
    system("bundle exec ruby benchmark/bench.rb > #{log_filename}")

    puts "Done! Parsing results from #{log_filename}...\n\n"

    system("bundle exec ruby benchmark/parse_log.rb #{log_filename}")
  end
end

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
