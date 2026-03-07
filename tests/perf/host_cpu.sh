#!/usr/bin/env bash

vn_perf_sanitize_field() {
  printf '%s' "$1" \
    | tr '\r\n' '  ' \
    | sed -e 's/[[:space:]][[:space:]]*/ /g' \
          -e 's/^ //' \
          -e 's/ $//' \
          -e 's/,/;/g'
}

vn_perf_detect_host_cpu() {
  local uname_s
  local cpu

  uname_s="$(uname -s 2>/dev/null || echo unknown)"
  cpu=""

  case "$uname_s" in
    Linux*)
      if [[ -r /proc/cpuinfo ]]; then
        cpu="$(awk -F: '
          /model name[[:space:]]*:/ {
            sub(/^[[:space:]]+/, "", $2);
            print $2;
            exit;
          }
          /Hardware[[:space:]]*:/ {
            sub(/^[[:space:]]+/, "", $2);
            print $2;
            exit;
          }
        ' /proc/cpuinfo 2>/dev/null || true)"
      fi
      ;;
    Darwin*)
      cpu="$(sysctl -n machdep.cpu.brand_string 2>/dev/null || true)"
      if [[ -z "$cpu" ]]; then
        cpu="$(sysctl -n hw.model 2>/dev/null || true)"
      fi
      ;;
    MINGW*|MSYS*|CYGWIN*)
      if command -v powershell.exe >/dev/null 2>&1; then
        cpu="$(powershell.exe -NoProfile -Command '$cpu = Get-CimInstance Win32_Processor | Select-Object -First 1 -ExpandProperty Name; if ($cpu) { [Console]::Out.Write($cpu) }' 2>/dev/null || true)"
      fi
      if [[ -z "$cpu" ]] && command -v wmic >/dev/null 2>&1; then
        cpu="$(wmic cpu get name 2>/dev/null | awk 'NR > 1 && NF { sub(/^[[:space:]]+/, ""); print; exit }' || true)"
      fi
      ;;
  esac

  if [[ -z "$cpu" ]]; then
    cpu="$(uname -m 2>/dev/null || echo unknown)"
  fi

  vn_perf_sanitize_field "$cpu"
}
