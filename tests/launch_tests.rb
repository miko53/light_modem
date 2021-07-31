#!/usr/bin/ruby

RZSZ_EXEC="../build/tools/rzsz"
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

def process_test(options_tx, options_rx, send_file, expected_file, result_file)
 
  bResult = false
  puts 'launch emission process'
  $emission_process = fork do 
    puts "#{RZSZ_EXEC} --protocol 0 --device #{$pts[0]} --speed 115200 --nb-stop 1 #{options_tx} --tx --file #{send_file}  >> emission.log 2>&1"
    exec "#{RZSZ_EXEC} --protocol 0 --device #{$pts[0]} --speed 115200 --nb-stop 1 #{options_tx} --tx --file #{send_file}  >> emission.log 2>&1"
  end
  Process.detach $emission_process

  puts 'launch reception'
  sleep(1)
  puts "#{RZSZ_EXEC} --protocol 0 --device #{$pts[1]} --speed 115200 --nb-stop 1 #{options_rx} --rx --file #{result_file} >> reception.log 2>&1"
  reception = `#{RZSZ_EXEC} --protocol 0 --device #{$pts[1]} --speed 115200 --nb-stop 1 #{options_rx} --rx --file #{result_file} >> reception.log 2>&1`
  
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

`rm -f socat.log emission.log reception.log #{LOG_FILE} `
`rm -rf tests_results/*`

launch_socat_process
sleep(1)


s = true
s = process_test("", "", "files/test_00128bytes.txt", "files/test_00128bytes.txt", "tests_results/1-test_00128bytes.txt") if (s)
s = process_test("", "", "files/test_01254bytes.txt", "expected_results/test_blksize_128_01280bytes.txt", "tests_results/2-test_01254bytes.txt") if (s)
s = process_test("", "", "files/test_32800bytes.txt", "expected_results/test_blksize_128_32800bytes.txt", "tests_results/3-test_32800bytes.txt")  if (s)
s = process_test("", "", "files/test_263000bytes.bin", "expected_results/test_blksize_128_263000bytes.bin", "tests_results/4-test_263000bytes.bin") if (s)

s = process_test("--crc", "--crc", "files/test_00128bytes.txt", "files/test_00128bytes.txt", "tests_results/5-test_00128bytes.txt") if (s)
s = process_test("--crc", "--crc", "files/test_01254bytes.txt", "expected_results/test_blksize_128_01280bytes.txt", "tests_results/6-test_01254bytes.txt") if (s)
s = process_test("--crc", "--crc", "files/test_32800bytes.txt", "expected_results/test_blksize_128_32800bytes.txt", "tests_results/7-test_32800bytes.txt")  if (s)
s = process_test("--crc", "--crc", "files/test_263000bytes.bin", "expected_results/test_blksize_128_263000bytes.bin", "tests_results/8-test_263000bytes.bin") if (s)

s = process_test("--1k", "--1k", "files/test_00128bytes.txt", "files/test_00128bytes.txt", "tests_results/9-test_00128bytes.txt") if (s)
s = process_test("--1k", "--1k", "files/test_01254bytes.txt", "expected_results/test_blksize_1024_01254bytes.txt", "tests_results/10-test_01254bytes.txt") if (s)
s = process_test("--1k", "--1k", "files/test_32800bytes.txt", "expected_results/test_blksize_128_32800bytes.txt", "tests_results/11-test_32800bytes.txt")  if (s)
s = process_test("--1k", "--1k", "files/test_263000bytes.bin", "expected_results/test_blksize_1024_263000bytes.bin", "tests_results/12-test_263000bytes.bin") if (s)

`killall socat`

if (s)
  exit(0)
end

exit(1)
