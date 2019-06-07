namespace "docker" do
  task "build" do
    sh "docker build -t 9cc ."
  end

  task "sh" => "build" do
    require "shellwords"
    sh "docker run --mount type=bind,src=#{__dir__.shellescape},dst=/9cc -ti 9cc"
  end
end

task "doc" do
  sh "open https://www.sigbus.info/compilerbook"
end

task "format" do
  sh "clang-format -i 9cc.c"
end

task "default" => "docker:sh"
