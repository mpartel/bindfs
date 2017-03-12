# Backwards-compatibility hacks for old systems running Ruby 1.8

$ruby_18_hacks_enabled = (RUBY_VERSION =~ /1\.8\..*/)

if $ruby_18_hacks_enabled
  require 'fileutils'
  require 'pathname'

  module FileUtils
    alias_method :original_chmod, :chmod
    def chmod(perms, path)
      if perms.is_a?(String)
        system("chmod " + Shellwords.escape(perms) + " " + Shellwords.escape(path))
      else
        original_chmod(perms, path)
      end
    end
  end

  class File
    def self.realpath(path)
      Pathname.new(path).realpath.to_s
    end

    def self.write(path, contents)
      File.open(path, "wb") do |f|
        f.write(contents)
      end
    end
  end

  module Shellwords
    # Copied from http://svn.ruby-lang.org/repos/ruby/trunk/lib/shellwords.rb (GPLv2)
    # on 2017-03-11
    def self.escape(str)
      str = str.to_s

      # An empty argument will be skipped, so return empty quotes.
      return "''".dup if str.empty?

      str = str.dup

      # Treat multibyte characters as is.  It is the caller's responsibility
      # to encode the string in the right encoding for the shell
      # environment.
      str.gsub!(/([^A-Za-z0-9_\-.,:\/@\n])/, "\\\\\\1")

      # A LF cannot be escaped with a backslash because a backslash + LF
      # combo is regarded as a line continuation and simply ignored.
      str.gsub!(/\n/, "'\n'")

      return str
    end
  end
end
