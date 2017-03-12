#!/usr/bin/env ruby
HELP = <<EOS
Runs the bindfs test suite in all vagrant boxes in parallel.

Usage: test.rb [--nohalt] [vm1 [...]]
Options:
  -h      --help            Print this and exit.
  --nohalt                  Don't halt VMs when done.

If VM names are given, only those are tested.

Bugs:
  - Spurious "Connection to 127.0.0.1 closed." messages can appear, and the
    terminal may need a `reset` after this command finishes.
EOS

require 'fileutils'

Dir.chdir(File.dirname(__FILE__))

halt_vms = true
specifically_selected_vms = []
ARGV.each do |arg|
  if arg == '-h' || arg == '--help'
    puts HELP
    exit
  elsif arg == '--nohalt'
    halt_vms = false
  elsif !arg.start_with?('-')
    specifically_selected_vms << arg
  else
    raise "Unknown option: #{arg}"
  end
end

dirs = Dir.glob('*/Vagrantfile').map { |path| File.dirname(path) }
unless specifically_selected_vms.empty?
  dirs = dirs.select { |dir| ARGV.include?(dir) }
end

puts "Running #{dirs.size} VMs in parallel: #{dirs.join(' ')}"
puts "Note: if your terminal goes wonky after this command, type 'reset'"
mutex = Thread::Mutex.new  # protects `$stdout` and `errors`
errors = []
threads = dirs.map do |dir|
  Thread.new do
    begin
      File.open(dir + "/test.log", "wb") do |logfile|
        run_and_log = -> (command) do
          logfile.puts ""
          logfile.puts "##### #{command} #####"
          logfile.flush
          pid = Process.spawn(command, chdir: dir, out: logfile, err: :out)
          Process.waitpid(pid)
          $?.success?
        end
        unless run_and_log.call "vagrant up"
          raise "vagrant up failed"
        end
        unless run_and_log.call "vagrant rsync"
          raise "vagrant rsync failed"
        end
        unless run_and_log.call "vagrant ssh -c 'cd /bindfs && sudo rm -Rf tests/tmp_test_bindfs && ./configure && make clean && make && make check && sudo make check'"
          mutex.synchronize do
            errors << "VM #{dir} tests failed."
          end
        end
        if halt_vms
          unless run_and_log.call "vagrant halt"
            raise "vagrant halt failed"
          end
        end
      end
    rescue
      mutex.synchronize do
        errors << "VM #{dir} error: #{$!}"
      end
    ensure
      mutex.synchronize do
        puts "Finished VM: #{dir}"
      end
    end
  end
end

threads.each(&:join)

if errors.empty?
  puts "OK"
else
  unless errors.empty?
    puts
    puts "Errors:"
    errors.each { |err| puts "  #{err}" }
    puts
    puts "See test.log in a failed VM's directory for more information"
    puts
  end
end
