#!/usr/bin/env sh
set -eu

bin="${1:-./build/modern-screenshot}"
limit_kb="${MODERN_SCREENSHOT_RSS_LIMIT_KB:-5120}"

if [ -z "${DISPLAY:-}" ]; then
  echo "DISPLAY is required for memory-check" >&2
  exit 77
fi

"$bin" >/tmp/modern-screenshot-memory.log 2>&1 &
pid="$!"

cleanup() {
  kill "$pid" 2>/dev/null || true
}
trap cleanup EXIT INT TERM

sleep 0.4
rss_kb="$(ps -o rss= -p "$pid" | tr -d ' ')"

if [ -z "$rss_kb" ]; then
  echo "unable to read process RSS" >&2
  exit 1
fi

echo "idle RSS: ${rss_kb} KB"

if [ "$rss_kb" -gt "$limit_kb" ]; then
  echo "RSS exceeds ${limit_kb} KB" >&2
  exit 1
fi
