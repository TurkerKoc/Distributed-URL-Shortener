#!/usr/bin/env bash
set -euo pipefail
source bashUtilFunctions.sh

pids=()
dataset_url="https://db.in.tum.de/teaching/ws2223/clouddataprocessing/data/clickbench.00.csv"
write_urls=()

function prepare_urls_for_workload() {
  # Download data
  curl --silent -o dataset.csv "$dataset_url"

  counter=0
  # Fill write_urls array
  while read line; do
    if [ $counter -eq 500 ]; then
      break
    fi
    write_urls+=("$line")
    counter=$((counter + 1))
  done <dataset.csv
}

function cleanup {
  # Kill all background processes
  for pid in "${pids[@]}"; do
    kill -s TERM "$pid" >/dev/null 2>&1
  done

  # Remove files
  rm -f loadBalancerOutput.txt
  rm -f raft*Output.txt
  rm -f raft_*.db
  rm -f dataset.csv

  if [[ $? -ne 0 ]]; then
    echo "incorrect"
  else
    echo "correct"
  fi
}

function check_correctness() {
  output=$1
  if [ $(echo "$output" | wc -l) -ne 2 ]; then
    exit 1
  fi
}

trap cleanup EXIT

# Spawn raft nodes
for i in {0..3}; do
  cmake-build-debug/src/raft "$i" >"raft${i}Output.txt" &
  pids+=($!)
  sleep 2
done

# Spawn the load balancer
cmake-build-debug/src/loadBalancer >"loadBalancerOutput.txt" &
pids+=($!)

prepare_urls_for_workload

sleep 25

# Write (should return success)
for url in "${write_urls[@]}"; do
  elapsed_time=$({ time {
    client_output=$(cmake-build-debug/src/client "write" "$url")
    if [ $(echo "$client_output" | wc -l) -ne 2 ]; then
      exit 1
    fi
  }; } 2>&1 1>>/dev/null)
  echo "$elapsed_time" >>writeTime.txt
done

echo "write bitt"
# Wait leader to replicate log to other raft nodes
sleep 20

# Read
for url in "${write_urls[@]}"; do
  elapsed_time=$({ time {
    output=$(cmake-build-debug/src/client "read" "$url")
    if [ $(echo "$output" | wc -l) -ne 2 ]; then
      exit 1
    fi
  }; } 2>&1 1>>/dev/null)
  echo "$elapsed_time" >>readTime.txt
done
