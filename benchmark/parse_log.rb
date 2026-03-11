# frozen_string_literal: true

def parse_benchmark_log(filename)
  results = {}
  current_mem_test = nil
  parsing_target = nil

  File.foreach(filename) do |line|
    # Parse Speedup (e.g., Read-Only (RMS) - Ruby:       71.3 i/s - 48.69x  slower)
    if match = line.match(%r{^\s*(.*?)\s*-\s*Ruby:\s+[\d.]+\s+i/s\s+-\s+([\d.]+)x\s+slower})
      test_name = match[1].strip
      results[test_name] ||= {}
      results[test_name][:speedup] = match[2]
    end

    # Parse Memory Profiling sections headers
    if match = line.match(/^MEMORY PROFILING:\s*(.*)/)
      current_mem_test = match[1].strip
      results[current_mem_test] ||= {}
    end

    # Determine if we are looking at Ruby or C++ memory
    if line.include?('[ Profiling Ruby')
      parsing_target = :ruby
    elsif line.include?('[ Profiling C++')
      parsing_target = :cpp
    end

    # Extract allocation numbers
    if (match = line.match(/Total allocated:\s+(\d+)\s+bytes\s+\((\d+)\s+objects\)/)) && current_mem_test && parsing_target
      results[current_mem_test][:"#{parsing_target}_bytes"] = format_number(match[1])
      results[current_mem_test][:"#{parsing_target}_objs"] = format_number(match[2])
      parsing_target = nil # reset after capturing
    end
  end

  results
end

def format_number(num_str)
  # Adds commas to large numbers for readability
  num_str.to_s.reverse.gsub(/(\d{3})(?=\d)/, '\\1,').reverse
end

def print_summary_table(results, log_filename)
  puts '-' * 95
  puts format('| %-25s | %-12s | %-22s | %-22s |', 'Benchmark', 'C++ Speedup', 'Ruby Allocation', 'C++ Allocation') # rubocop:disable Style/FormatStringToken
  puts '-' * 95

  results.each do |name, data|
    speedup    = data[:speedup] ? "#{data[:speedup]}x" : 'N/A'
    ruby_alloc = data[:ruby_bytes] ? "#{data[:ruby_bytes]} B (#{data[:ruby_objs]} obj)" : 'N/A'
    cpp_alloc  = data[:cpp_bytes] ? "#{data[:cpp_bytes]} B (#{data[:cpp_objs]} obj)" : 'N/A'

    puts format('| %-25s | %-12s | %-22s | %-22s |', name, speedup, ruby_alloc, cpp_alloc) # rubocop:disable Style/FormatStringToken
  end

  puts '-' * 95
  puts "\nFull detailed log saved to: #{log_filename}"
end

# Execute only if run directly
if __FILE__ == $0
  log_file = ARGV[0]

  unless log_file && File.exist?(log_file)
    puts 'Usage: ruby parse_log.rb <path_to_log_file>'
    exit 1
  end

  results = parse_benchmark_log(log_file)
  print_summary_table(results, log_file)
end
