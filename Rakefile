require 'fileutils'
require 'shellwords'

module Production
  def self.server
    'bindfs.org'
  end
  
  def self.dir
    '/srv/www/bindfs.org'
  end
end

module Utils
  def self.rsync(src, dest, *extra_options)
    cmd = [
      'rsync',
      '-avv',
      '--progress',
      *extra_options,
      src,
      dest
    ]
    system(Shellwords.join(cmd))
  end
end

task :default => :build

desc "Builds the site under _site/"
task :build do
  puts
  system("jekyll build")
  puts
end

desc "Removes _site/"
task :clean do
  FileUtils.rm_rf('_site')
end

namespace :downloads do
  desc "Gets downloads from the server"
  task :get do
    Utils.rsync(
      Production.server + ':' +  Production.dir + '/downloads/',
      'downloads/',
      '--ignore-existing'
    )
  end
end

namespace :deploy do
  desc "Deploys to the production environment"
  task :production => :build do
    Utils.rsync(
      '_site/',
      Production.server + ':' + Production.dir + '/'
    )
  end
end

