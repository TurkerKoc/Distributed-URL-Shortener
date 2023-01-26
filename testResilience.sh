#!/usr/bin/env bash
set -euo pipefail

pids=()
write_urls=("https://mattermost.db.in.tum.de/cbdp202223/channels/url-shortener" "https://www.tum.de/en/about-tum" "https://chat.openai.com/")
write_urls_results=()
write_urls_after_reboot=("https://www.youtube.com/watch?v=5GhhVHpPR_M" "https://www.imdb.com/title/tt1375666" "https://github.com/TUM-DSE/cloud-lab")
write_urls_after_reboot_results=()
read_urls_fail=("www.read.operation/will/fail" "https://gitlab.db.in.tum.de/" "www.google.com")

function cleanup {
  if [[ $? -ne 0 ]]; then
    echo "incorrect"
  else
    echo "correct"
  fi

  kill_pids

  # Remove log files
  rm -f loadBalancerOutput.txt
  rm -f raft*Output.txt
  rm -f raft_*.db
}

trap cleanup EXIT

function kill_pids() {
  # Kill all background processes
  for pid in "${pids[@]}"; do
    kill -s TERM "$pid" >/dev/null 2>&1
  done
}

function run_raft_cluster() {
  sleep 1
  # Spawn raft nodes
  for i in {0..3}; do
    cmake-build-debug/src/raft "$i" >"raft${i}Output.txt" &
    pids+=($!)
    sleep 2
  done

  # Spawn the load balancer
  cmake-build-debug/src/loadBalancer >"loadBalancerOutput.txt" &
  pids+=($!)
  sleep 1
}

function reboot_raft_cluster() {
  kill_pids
  pids=()
  run_raft_cluster
}

function check_write_request_output() {
  output=$1
  url=$2
  if [ $(echo "$output" | wc -l) -ne 2 ]; then
    exit 1
  fi

  first_line=$(echo "$output" | head -n 1)
  second_line=$(echo "$output" | tail -n 1)

  IFS=':' read -ra line <<<"$first_line"
  rest_of_string="${line[1]}"
  for j in "${line[@]:2}"; do
    rest_of_string+=":$j"
  done
  rest_of_string="${rest_of_string:1}"
  if [ "${line[0]}" != "Long URL" ] || [ "$rest_of_string" != "$url" ]; then
    exit 1
  fi

  IFS=':' read -ra line <<<"$second_line"
  rest_of_string="${line[1]}"
  for j in "${line[@]:2}"; do
    rest_of_string+=":$j"
  done
  rest_of_string="${rest_of_string:1}"
  if [ "${line[0]}" != "Short URL" ]; then
    exit 1
  fi

  echo "$rest_of_string"
}

function check_read_request_output() {
  output=$1
  url=$2
  result_url=$3
  # Check if output has two rows
  if [ $(echo "$output" | wc -l) -ne 2 ]; then
    exit 1
  fi

  first_line=$(echo "$output" | head -n 1)
  second_line=$(echo "$output" | tail -n 1)

  IFS=':' read -ra line <<<"$first_line"
  rest_of_string="${line[1]}"
  for j in "${line[@]:2}"; do
    rest_of_string+=":$j"
  done
  rest_of_string="${rest_of_string:1}"
  if [ "${line[0]}" != "URL" ] || [ "$rest_of_string" != "$url" ]; then
    exit 1
  fi

  IFS=':' read -ra line <<<"$second_line"
  rest_of_string="${line[1]}"
  for j in "${line[@]:2}"; do
    rest_of_string+=":$j"
  done
  rest_of_string="${rest_of_string:1}"
  if [ "${line[0]}" != "Result URL" ] || [ "$rest_of_string" != "$result_url" ]; then
    exit 1
  fi
}

run_raft_cluster

# Wait for them to select leader
sleep 15

# Write (should return success)
for url in "${write_urls[@]}"; do
  client_output=$(cmake-build-debug/src/client "write" "$url")
  short_url=$(check_write_request_output "$client_output" "$url")
  write_urls_results+=("$short_url")
done

# Wait leader to replicate log to other raft nodes
sleep 20

# Read (should return success)
for ((i = 0; i < ${#write_urls[@]}; i++)); do
  output=$(cmake-build-debug/src/client "read" "${write_urls[i]}")
  check_read_request_output "$output" "${write_urls[i]}" "${write_urls_results[i]}"

  output=$(cmake-build-debug/src/client "read" "${write_urls_results[i]}")
  check_read_request_output "$output" "${write_urls_results[i]}" "${write_urls[i]}"
done

# Read (should return failed)
for url in "${read_urls_fail[@]}"; do
  output=$(cmake-build-debug/src/client "read" "$url")

  # Check if output has two rows
  if [ $(echo "$output" | wc -l) -ne 1 ]; then
    exit 1
  fi
done

reboot_raft_cluster

# Wait for them to select leader
sleep 15

# Read (should return success)
for ((i = 0; i < ${#write_urls[@]}; i++)); do
  output=$(cmake-build-debug/src/client "read" "${write_urls[i]}")
  check_read_request_output "$output" "${write_urls[i]}" "${write_urls_results[i]}"

  output=$(cmake-build-debug/src/client "read" "${write_urls_results[i]}")
  check_read_request_output "$output" "${write_urls_results[i]}" "${write_urls[i]}"
done

# Write 2 (should return success)
for url in "${write_urls_after_reboot[@]}"; do
  client_output=$(cmake-build-debug/src/client "write" "$url")
  short_url=$(check_write_request_output "$client_output" "$url")
  write_urls_after_reboot_results+=("$short_url")
done

# Wait leader to replicate log to other raft nodes
sleep 20

# Read 2 (should return success)
for ((i = 0; i < ${#write_urls_after_reboot[@]}; i++)); do
  output=$(cmake-build-debug/src/client "read" "${write_urls_after_reboot[i]}")
  check_read_request_output "$output" "${write_urls_after_reboot[i]}" "${write_urls_after_reboot_results[i]}"

  output=$(cmake-build-debug/src/client "read" "${write_urls_after_reboot_results[i]}")
  check_read_request_output "$output" "${write_urls_after_reboot_results[i]}" "${write_urls_after_reboot[i]}"
done