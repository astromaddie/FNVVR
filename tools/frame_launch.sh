#!/usr/bin/env bash
# Keep Steam's compatibility wrapper and arguments, bypassing the desktop launcher.
set -euo pipefail
args=("$@")
for index in "${!args[@]}"; do
  case "${args[$index]}" in
    *FalloutNVLauncher.exe)
      args[$index]="${args[$index]%FalloutNVLauncher.exe}FalloutNV.exe"
      ;;
  esac
done
exec "${args[@]}"
