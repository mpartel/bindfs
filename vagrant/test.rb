#!/usr/bin/env ruby
HELP = <<EOS
Runs the bindfs test suite in all vagrant boxes in parallel.

Usage: test.rb [--nohalt] [vm1 [...]]
Options:
  -h      --help            Print this and exit.
  --nohalt                  Don't halt VMs when done.
  --print-logs              Prints each VM's logs when it completes.

If VM names are given, only those are tested.

Bugs:
  - Spurious "Connection to 127.0.0.1 closed." messages can appear, and the
    terminal may need a `reset` after this command finishes.
EOS

require 'fileutils'

Dir.chdir(File.dirname(__FILE__))

halt_vms = true
print_logs = false
specifically_selected_vms = []
ARGV.each do |arg|
  if arg == '-h' || arg == '--help'
    puts HELP
    exit
  elsif arg == '--nohalt'
    halt_vms = false
  elsif arg == '--print-logs'
    print_logs = true
  elsif !arg.start_with?('-')
    specifically_selected_vms << arg
  else
    raise "Unknown option: #{arg}"
  end
end

dirs = Dir.glob('*/Vagrantfile').map { |path| File.dirname(path) }
unless specifically_selected_vms.empty?
  dirs = dirs.select { |dir| specifically_selected_vms.include?(dir) }
end

def with_retries(n, options = {}, &block)
  options = {
    sleep: 0,
    sleep_jitter: 0,
  }.merge options
  loop do
    begin
      return block.call
    rescue
      raise $! if n <= 0
      puts "Retrying #{n} more times after catching: #{$!}"
    end
    sleep(options[:sleep] + (Random.rand - 0.5) * options[:sleep_jitter])
    n -= 1
  end
end

puts "Running #{dirs.size} VMs in parallel: #{dirs.join(' ')}"
puts "You can follow the progress of each VM by tailing vagrant/*/test.log"
puts "Note: if your terminal goes wonky after this command, type 'reset'"
mutex = Thread::Mutex.new  # protects `$stdout` and `errors`
errors = []
threads = dirs.map do |dir|
  Thread.new do
    begin
      File.open(dir + "/test.log", "w+b") do |logfile|
        run_and_log = -> (command) do
          logfile.puts ""
          logfile.puts "##### #{command} #####"
          logfile.flush
          pid = Process.spawn(command, chdir: dir, out: logfile, err: logfile)
          Process.waitpid(pid)
          $?.success?
        end
        # parallel `vagrant up` commands can be flaky with VirtualBox due to lock contention,
        # so give it a few retries.
        with_retries(3, sleep: 1.0, sleep_jitter: 0.5) do
          unless run_and_log.call "vagrant up"
            raise "vagrant up failed"
          end
        end
        unless run_and_log.call "vagrant rsync"
          raise "vagrant rsync failed"
        end
        unless run_and_log.call "vagrant ssh -c 'cd /bindfs && sudo rm -Rf tests/tmp_test_bindfs && ./autogen.sh && ./configure && make distclean && ./configure && make && make check && sudo make check'"
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
      if print_logs
        puts File.read(dir + "/test.log")
        puts
      end
    end
  end
end

threads.each(&:join)

if errors.empty?
  puts "All OK"
else
  unless errors.empty?
    puts
    puts "Errors:"
    errors.each { |err| puts "  #{err}" }
    puts
    puts "See test.log in a failed VM's directory for more information"
    puts
    exit 1
  end
end

