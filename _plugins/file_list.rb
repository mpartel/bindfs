
require 'version'

module Jekyll
  module FileListCommon
    def initialize(tag_name, dir, tokens)
      super
      @dir = dir.strip
    end
    
    private
  
    def file_names
      entries = Dir.entries(@dir)
      entries.reject! {|e| ignore_file? e }
      entries.sort_by! {|e| File.mtime(@dir + '/' + e) }
      entries.sort_by! {|e| get_version(e) }
      entries.reverse!
    end
    
    def get_version(filename)
      if filename =~ /[-_]([0-9]+(?:\.[0-9]+)+)/
        $1.to_version
      else
        "0.0.0".to_version
      end
    end
    
    def ignore_file?(filename)
      filename.start_with?('.') ||
      filename.end_with?('.html')
    end
  end

  class FileListBlock < Liquid::Block
    include FileListCommon
    
    def render(context)
      file_names.map do |filename|
        context.stack('filename' => filename) do
          super
        end
      end.join('')
    end
  end
  
  class FirstFileTag < Liquid::Tag
    include FileListCommon
    
    def render(context)
      file_names.first
    end
  end
end

Liquid::Template.register_tag('file_list', Jekyll::FileListBlock)
Liquid::Template.register_tag('first_file', Jekyll::FirstFileTag)

