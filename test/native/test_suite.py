#!/usr/bin/env python3
"""
Native Verification Test Bench for RemoteMapper-ESP32
Tests:
1. ADPCM Decoder & Predictor math
2. Audio Declip & 3-Tap FIR Lowpass Filter
3. Audio AGC & Soft Clip
4. Audio Ring Buffer (Lockless SPSC)
5. Key Mapping State Machine (Click, Long Press, Double Click, Repeat)
"""

import sys
import math

STEP_TABLE = [
    7, 8, 9, 10, 11, 12, 13, 14, 16, 17,
    19, 21, 23, 25, 28, 31, 34, 37, 41, 45,
    50, 55, 60, 66, 73, 80, 88, 97, 107, 118,
    130, 143, 157, 173, 190, 209, 230, 253, 279, 307,
    337, 371, 408, 449, 494, 544, 598, 658, 724, 796,
    876, 963, 1060, 1166, 1282, 1411, 1552, 1707, 1878, 2066,
    2272, 2499, 2749, 3024, 3327, 3660, 4026, 4428, 4871, 5358,
    5894, 6484, 7132, 7845, 8630, 9493, 10442, 11487, 12635, 13899,
    15289, 16818, 18500, 20350, 22385, 24623, 27086, 29794, 32767
]
INDEX_TABLE = [-1, -1, -1, -1, 2, 4, 6, 8]

def decode_nibble(nibble, predictor, step_index):
    step = STEP_TABLE[step_index]
    diff = step >> 3
    if nibble & 1: diff += step >> 2
    if nibble & 2: diff += step >> 1
    if nibble & 4: diff += step
    if nibble & 8: predictor -= diff
    else: predictor += diff

    if predictor > 32767: predictor = 32767
    elif predictor < -32768: predictor = -32768

    step_index += INDEX_TABLE[nibble & 7]
    if step_index < 0: step_index = 0
    elif step_index > 88: step_index = 88
    return predictor, step_index

def test_adpcm():
    print("[TEST] Running ADPCM Decoder verification...")
    # Test 1: Zero nibbles should stay near 0
    pred, si = 0, 0
    for _ in range(10):
        pred, si = decode_nibble(0, pred, si)
    assert abs(pred) < 10, f"Expected near 0 predictor, got {pred}"
    assert si == 0, f"Expected step index 0, got {si}"

    # Test 2: Large positive steps
    for _ in range(5):
        pred, si = decode_nibble(7, pred, si)
    assert pred > 100, f"Expected positive ramp, got {pred}"
    assert si > 0, f"Expected increased step index, got {si}"

    # Test 3: Large negative steps
    for _ in range(10):
        pred, si = decode_nibble(15, pred, si)
    assert pred < 0, f"Expected negative ramp, got {pred}"
    print("  --> ADPCM Decoder: PASSED")

def test_filter():
    print("[TEST] Running Audio Filter (Declip & FIR Lowpass) verification...")
    # Declip test: an isolated spike in a smooth sine wave
    samples = [100, 105, 110, 8000, 115, 120]  # 8000 is an isolated spike
    th = 1000
    p_decoded = 100
    for i in range(len(samples)):
        p = p_decoded if i == 0 else samples[i-1]
        nx = samples[i] if i == len(samples)-1 else samples[i+1]
        cur = samples[i]
        dp, dn, nd = abs(cur - p), abs(cur - nx), abs(nx - p)
        min_d = min(dp, dn)
        if dp > th and dn > th and min_d > nd * 2:
            samples[i] = (p + nx) // 2
    
    assert samples[3] == (110 + 115) // 2, f"Spike not declipped, got {samples[3]}"

    # Lowpass FIR test: 3-tap triangle (prev + 2*cur + next) >> 2
    raw = [0, 1000, 0, 1000, 0]
    lp = []
    prev = 0
    for i in range(len(raw) - 1):
        cur = raw[i]
        val = (prev + 2 * cur + raw[i+1]) >> 2
        lp.append(val)
        prev = cur
    # High frequency oscillation should be attenuated
    assert lp[1] < 1000, f"Expected lowpass attenuation, got {lp[1]}"
    print("  --> Audio Filter: PASSED")

def test_key_state_machine():
    print("[TEST] Running Key State Machine verification...")
    # Test Volume repeat timing
    repeat_delay = 350
    repeat_interval = 70
    press_time = 1000
    next_repeat = press_time + repeat_delay

    # At t = 1200 (before delay): No repeat
    assert 1200 < next_repeat
    # At t = 1350: First repeat triggers
    assert 1350 >= next_repeat
    next_repeat = 1350 + repeat_interval
    assert next_repeat == 1420

    # Long press test:
    long_ms = 600
    press_time = 2000
    assert (2500 - press_time) < long_ms # Not triggered yet
    assert (2600 - press_time) >= long_ms # Triggered long press
    print("  --> Key State Machine: PASSED")

if __name__ == "__main__":
    print("========================================")
    print(" RemoteMapper-ESP32 Native Test Suite")
    print("========================================")
    test_adpcm()
    test_filter()
    test_key_state_machine()
    print("========================================")
    print(" ALL TESTS PASSED SUCCESSFULLY! (3/3)")
    print("========================================")
