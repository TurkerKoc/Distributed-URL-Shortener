#!/usr/bin/env bash
set -euo pipefail


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