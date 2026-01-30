#!/usr/bin/env bash
set -eu
#
# 自动化延迟验证脚本
# 用途：按步骤运行 1-4 项验证实验并收集 FRAME_INTERVAL / FRAME_LATENCY 数据
#
# 使用说明：
#   cd endo_v4l_cv
#   ./tools/run_latency_tests.sh --outdir logs --duration 10
# 可选：增加 --set-60hz 来尝试把显示器切换为 60Hz（需 xrandr 权限）

ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
BUILD_DIR="$ROOT_DIR/build"
LOG_DIR_DEFAULT="$ROOT_DIR/logs"
DURATION=10
SET_60HZ=0
OUT_DIR="$LOG_DIR_DEFAULT"

usage() {
    cat <<EOF
Usage: $0 [--outdir DIR] [--duration S] [--set-60hz]
  --outdir DIR   保存日志和解析结果 (默认: $LOG_DIR_DEFAULT)
  --duration S   每个测试运行秒数 (默认: $DURATION)
  --set-60hz     尝试将主显示切换到 60Hz（可选，需要 xrandr）
EOF
    exit 1
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --outdir) OUT_DIR="$2"; shift 2;;
        --duration) DURATION="$2"; shift 2;;
        --set-60hz) SET_60HZ=1; shift;;
        -h|--help) usage;;
        *) echo "Unknown arg: $1"; usage;;
    esac
done

mkdir -p "$OUT_DIR"
mkdir -p "$BUILD_DIR"

log() { echo "[$(date +%T)] $*"; }

cmake_configure_and_build() {
    local use_vulkan="$1"   # 0 or 1
    log "Configuring CMake (USE_VULKAN=$use_vulkan)..."
    # replace target_compile_definitions line in CMakeLists.txt temporarily
    sed -i.bak -E "s/target_compile_definitions\\(([^)]+)USE_VULKAN=[01]\\)/target_compile_definitions(\\1USE_VULKAN=${use_vulkan})/" "$ROOT_DIR/CMakeLists.txt"

    (cd "$BUILD_DIR" && cmake .. >/dev/null)
    log "Building..."
    (cd "$BUILD_DIR" && make -j4 >/dev/null)
    log "Build done."
}

run_and_capture() {
    local name="$1"
    local envs="$2"
    local timeout_s="$3"
    local logfile="$OUT_DIR/${name}.log"

    log "Running test: $name (timeout ${timeout_s}s) env=[$envs] -> $logfile"
    # run with timeout, capture stdout/stderr
    bash -lc "cd '$BUILD_DIR' && $envs timeout $timeout_s ./endo_viewer_v4l" > "$logfile" 2>&1 || true
}

parse_and_summary() {
    local logfile="$1"
    local base="$2"
    # extract FRAME_INTERVAL and FRAME_LATENCY values (microseconds)
    grep -E 'FRAME_INTERVAL:' "$logfile" || true
    grep -E 'FRAME_LATENCY:' "$logfile" || true

    # generate CSVs
    grep -E 'FRAME_INTERVAL:' "$logfile" | awk '{print $2}' | sed 's/us//' > "$OUT_DIR/${base}_frame_interval_us.csv" || true
    grep -E 'FRAME_LATENCY:' "$logfile" | awk '{print $6}' | sed 's/us//' > "$OUT_DIR/${base}_frame_latency_us.csv" || true

    # compute simple stats using awk
    if [[ -s "$OUT_DIR/${base}_frame_interval_us.csv" ]]; then
        awk 'BEGIN{sum=0;sum2=0;cnt=0} {x=$1; sum+=x; sum2+=x*x; cnt++} END{ if(cnt>0){mean=sum/cnt; sd=sqrt(sum2/cnt-mean*mean); printf("INTERVAL: count=%d mean=%.2fus sd=%.2fus\\n",cnt,mean,sd)} }' "$OUT_DIR/${base}_frame_interval_us.csv" >> "$OUT_DIR/${base}_summary.txt"
    fi
    if [[ -s "$OUT_DIR/${base}_frame_latency_us.csv" ]]; then
        awk 'BEGIN{sum=0;sum2=0;cnt=0} {x=$1; sum+=x; sum2+=x*x; cnt++} END{ if(cnt>0){mean=sum/cnt; sd=sqrt(sum2/cnt-mean*mean); printf("LATENCY: count=%d mean=%.2fus sd=%.2fus\\n",cnt,mean,sd)} }' "$OUT_DIR/${base}_frame_latency_us.csv" >> "$OUT_DIR/${base}_summary.txt"
    fi
}

maybe_set_60hz() {
    if [[ $SET_60HZ -eq 0 ]]; then return; fi
    if ! command -v xrandr >/dev/null 2>&1; then
        log "xrandr not found; cannot change refresh rate. Skipping."
        return
    fi
    local out=$(xrandr --query | sed -n 's/^[^ ]* connected //p' | awk '{print $1; exit}')
    if [[ -z "$out" ]]; then
        log "Cannot detect connected output via xrandr. Skipping set-60hz."
        return
    fi
    # find a 60Hz mode
    local mode=$(xrandr --query | awk -v o="$out" 'BEGIN{p=0} $0 ~ "^"o{p=1} p && /60.00/ {print $1; exit}')
    if [[ -z "$mode" ]]; then
        log "No 60Hz mode found for $out. Skipping."
        return
    fi
    log "Setting output $out to mode $mode (60Hz)."
    xrandr --output "$out" --mode "$mode" || log "xrandr failed to set mode"
}

restore_cmake() {
    # restore original CMakeLists backup if exists
    if [[ -f "$ROOT_DIR/CMakeLists.txt.bak" ]]; then
        mv -f "$ROOT_DIR/CMakeLists.txt.bak" "$ROOT_DIR/CMakeLists.txt"
    fi
}

##### TEST SUITE #####
log "Starting latency test suite. Output dir: $OUT_DIR"

# Optional: try to set 60Hz if requested
maybe_set_60hz

####################
# Test A: Baseline OpenGL (USE_VULKAN=0) at current refresh
cmake_configure_and_build 0
run_and_capture "opengl_baseline" "" "$DURATION"
parse_and_summary "$OUT_DIR/opengl_baseline.log" "opengl_baseline"

####################
# Test B: Baseline Vulkan (USE_VULKAN=1)
cmake_configure_and_build 1
run_and_capture "vulkan_baseline" "" "$DURATION"
parse_and_summary "$OUT_DIR/vulkan_baseline.log" "vulkan_baseline"

####################
# Test C: Vulkan without vkDeviceWaitIdle (requires source edit, done via sed patch)
log "Patching VkDisplay.cpp to skip vkDeviceWaitIdle (temporary)..."
VKCPP="$ROOT_DIR/src/VkDisplay.cpp"
VKCPP_BAK="$VKCPP.bak_run_tests"
if [[ ! -f "$VKCPP_BAK" ]]; then
    cp "$VKCPP" "$VKCPP_BAK"
fi
sed -i '/vkDeviceWaitIdle(device);/c\#ifndef SKIP_VK_DEVICE_WAIT_IDLE\n    vkDeviceWaitIdle(device);\n#endif' "$VKCPP"
cmake_configure_and_build 1
run_and_capture "vulkan_no_waitidle" "" "$DURATION"
parse_and_summary "$OUT_DIR/vulkan_no_waitidle.log" "vulkan_no_waitidle"
# restore VkDisplay.cpp
mv -f "$VKCPP_BAK" "$VKCPP"

####################
# Test D: OpenGL + __GL_MaxFramesAllowed=1
cmake_configure_and_build 0
run_and_capture "opengl_gl_maxframes_1" "__GL_MaxFramesAllowed=1" "$DURATION"
parse_and_summary "$OUT_DIR/opengl_gl_maxframes_1.log" "opengl_gl_maxframes_1"

####################
# Test E: OpenGL VSync disabled
# rebuild OpenGL (no change) and run with env to disable swap interval? We rely on code using glfwSwapInterval(1) — user must recompile to change.
log "Running OpenGL baseline again for stability check..."
run_and_capture "opengl_baseline_repeat" "" "$DURATION"
parse_and_summary "$OUT_DIR/opengl_baseline_repeat.log" "opengl_baseline_repeat"

restore_cmake

log "Tests completed. Summaries saved in $OUT_DIR/*.txt and raw CSVs."
echo "Done."


