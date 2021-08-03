#!/usr/bin/ruby

RZSZ_EXEC="../build/tools/rzsz"
#RZSZ_EXEC="../build-linux-release/tools/rzsz"
LOG_FILE = "tests.log"

$pts = Array.new

def launch_socat_process

  $socat_process = fork do
    puts "socat -d -d pty,raw,echo=0 pty,raw,echo=0 1>socat.log 2>&1"
    exec "socat -d -d pty,raw,echo=0 pty,raw,echo=0 1>socat.log 2>&1"
  end

  Process.detach $socat_process
  sleep(1)
  socat_file = File.open('socat.log')
  socat_file.each_line do |line|
    p line
    if line =~ /N PTY is (.+)\n/
      $pts.append $1
    end
  end
end

def delete_socat_process
  `killall socat`
end

def process_test(options_tx, options_rx, send_file, expected_file, result_file)

  bResult = false
  puts 'launch emission process'
  $emission_process = fork do
    puts "#{RZSZ_EXEC} --device #{$pts[0]} --speed 115200 --nb-stop 1 #{options_tx} --tx --file #{send_file}  >> emission.log 2>&1"
    exec "#{RZSZ_EXEC} --device #{$pts[0]} --speed 115200 --nb-stop 1 #{options_tx} --tx --file #{send_file}  >> emission.log 2>&1"
  end
  Process.detach $emission_process

  sleep(1)
  puts 'launch reception'
  puts "#{RZSZ_EXEC} --device #{$pts[1]} --speed 115200 --nb-stop 1 #{options_rx} --rx --file #{result_file} >> reception.log 2>&1"
  reception = `#{RZSZ_EXEC} --device #{$pts[1]} --speed 115200 --nb-stop 1 #{options_rx} --rx --file #{result_file} >> reception.log 2>&1`

  reception_status_code = $?
  if reception_status_code.exitstatus.zero?
    diff = `diff #{expected_file} #{result_file} >> #{LOG_FILE} 2>&1`
    diff_status_code = $?
    if (diff_status_code.exitstatus.zero?)
      puts "test ok"
      bResult = true
    else
      puts "test failed, result file differs, see log file"
    end
  else
    puts "test failed, reception status not zero"
  end

  bResult
end


def delete_all_previous_log_file
  `rm -f socat.log emission.log reception.log #{LOG_FILE} `
  `rm -f tests_results/*`
  `touch tests_results/KEEP`
end

$nominal_tests_xmodem = Array.new
$nominal_tests_xmodem = [
{ options_tx: "--protocol 0 ", options_rx: "--protocol 0",
  send_file: "files/test_00128bytes.txt", expected_file: "files/test_00128bytes.txt", result_file: "tests_results/1-test_00128bytes.txt" },
{ options_tx: "--protocol 0", options_rx: "--protocol 0",
  send_file: "files/test_01254bytes.txt", expected_file: "expected_results/test_blksize_128_01280bytes.txt", result_file: "tests_results/2-test_01254bytes.txt" },
{ options_tx: "--protocol 0 ", options_rx: "--protocol 0 ",
  send_file: "files/test_32800bytes.txt", expected_file: "expected_results/test_blksize_128_32800bytes.txt", result_file: "tests_results/3-test_32800bytes.txt" },
{ options_tx: "--protocol 0 ", options_rx: "--protocol 0 ",
  send_file: "files/test_263000bytes.bin", expected_file: "expected_results/test_blksize_128_263000bytes.bin", result_file: "tests_results/4-test_263000bytes.bin" },

{ options_tx: "--protocol 0 --crc", options_rx: "--protocol 0 --crc",
  send_file: "files/test_00128bytes.txt", expected_file: "files/test_00128bytes.txt", result_file: "tests_results/5-test_00128bytes.txt" },
{ options_tx: "--protocol 0 --crc", options_rx: "--protocol 0 --crc",
  send_file: "files/test_01254bytes.txt", expected_file: "expected_results/test_blksize_128_01280bytes.txt", result_file: "tests_results/6-test_01254bytes.txt" },
{ options_tx: "--protocol 0 --crc", options_rx: "--protocol 0 --crc",
  send_file: "files/test_32800bytes.txt", expected_file: "expected_results/test_blksize_128_32800bytes.txt", result_file: "tests_results/7-test_32800bytes.txt" },
{ options_tx: "--protocol 0 --crc", options_rx: "--protocol 0 --crc",
  send_file: "files/test_263000bytes.bin", expected_file: "expected_results/test_blksize_128_263000bytes.bin", result_file: "tests_results/8-test_263000bytes.bin" },

{ options_tx: "--protocol 0 --1k", options_rx: "--protocol 0 --1k",
  send_file: "files/test_00128bytes.txt", expected_file: "files/test_00128bytes.txt", result_file: "tests_results/9-test_00128bytes.txt" },
{ options_tx: "--protocol 0 --1k", options_rx: "--protocol 0 --1k",
  send_file: "files/test_01254bytes.txt", expected_file: "expected_results/test_blksize_1024_01254bytes.txt", result_file: "tests_results/10-test_01254bytes.txt" },
{ options_tx: "--protocol 0 --1k", options_rx: "--protocol 0 --1k",
  send_file: "files/test_32800bytes.txt", expected_file: "expected_results/test_blksize_128_32800bytes.txt", result_file: "tests_results/11-test_32800bytes.txt" },
{ options_tx: "--protocol 0 --1k", options_rx: "--protocol 0 --1k",
  send_file: "files/test_263000bytes.bin", expected_file: "expected_results/test_blksize_1024_263000bytes.bin", result_file: "tests_results/12-test_263000bytes.bin" }
]

$nominal_tests_ymodem = [
{ options_tx: "--protocol 1", options_rx: "--protocol 1",
  send_file: "files/test_00128bytes.txt", expected_file: "files/test_00128bytes.txt", result_file: "tests_results/13-test_00128bytes.txt" },
{ options_tx: "--protocol 1", options_rx: "--protocol 1",
  send_file: "files/test_01254bytes.txt", expected_file: "files/test_01254bytes.txt", result_file: "tests_results/14-test_01254bytes.txt" },
{ options_tx: "--protocol 1", options_rx: "--protocol 1",
  send_file: "files/test_32800bytes.txt", expected_file: "files/test_32800bytes.txt", result_file: "tests_results/15-test_32800bytes.txt" },
{ options_tx: "--protocol 1", options_rx: "--protocol 1",
  send_file: "files/test_263000bytes.bin", expected_file: "files/test_263000bytes.bin", result_file: "tests_results/16-test_263000bytes.bin" }
]

def main

  delete_all_previous_log_file
  launch_socat_process

  sleep(1)

  s = true
  $nominal_tests_xmodem.each do |test|
    s = process_test(test[:options_tx], test[:options_rx], test[:send_file], test[:expected_file], test[:result_file]) if (s)
  end

  $nominal_tests_ymodem.each do |test|
    s = process_test(test[:options_tx], test[:options_rx], test[:send_file], test[:expected_file], test[:result_file]) if (s)
  end

  delete_socat_process

  if (s)
    puts "all tests ok"
    exit(0)
  end

  puts "at least one test failed"
  exit(1)
end


main
