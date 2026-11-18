# src/ notes

## Device Modes

Each device operates in one of two modes, set via `mode = "multichannel"` (default) or `mode = "scan"` in config.

**`R_MULTICHANNEL`** — The SDR is tuned to a fixed center frequency and multiple channels are demodulated simultaneously from the same wideband capture. Each channel has a single `freq` value that must fall within the SDR's bandwidth. This is the common case for monitoring several frequencies at once.

**`R_SCAN`** — The device has exactly one channel, but that channel holds a `freqs` list of frequencies to cycle through. A controller thread monitors the squelch: after ~2 seconds of no signal (10 × 200 ms polls), it retunes the SDR hardware to the next frequency via `input_set_centerfreq()`. When a signal is detected, it stays on the current frequency. Per-frequency settings (labels, `squelch_threshold`/`squelch_snr_threshold`, `modulations`, `notch`/`notch_q`, `ctcss`, `bandwidth`, `ampfactor`) are **parallel lists** to `freqs` (same index = same frequency), not a list of objects — `freqs` is parsed as numbers. Only one channel per device in scan mode. (`rtl_airband.cpp:101-140`, `config.cpp:312-729`)

Samples already buffered or in flight at a retune still belong to the old frequency (`mark_retune()`, `stale_samples_after_retune()`). A retune usually lands mid-batch, so the demod loop records where those stale samples fall in the current `WAVE_BATCH` (`stale_window_t`) and `plan_retune_batch()` decides what to drop and when to switch `freq_idx`: everything up to the end of the stale samples is dropped in the batch where the switch happens, so no old-frequency audio reaches the new frequency's squelch. Measured on an RTL-SDR (R820T, 2.56 Msps), a retune shows up in the stream at most ~50 ms after the samples delivered when `rtlsdr_set_center_freq()` returns, within the one-USB-buffer (62.5 ms) allowance in `latency_bytes`.

## Threading Model

- **RX thread** (1 per device, always) — reads SDR samples into the circular buffer (`input-common.cpp`)
- **Controller thread** (1 per device, `R_SCAN` mode only) — scanning/squelch state machine for devices that scan across frequencies; not created for `R_MULTICHANNEL` devices (`rtl_airband.cpp:1005-1013`)
- **Demod thread** (1 total by default; 1 per device if `multiple_demod_threads=true` in config) — FFT, demodulation, filter, CTCSS, squelch, AGC for all assigned devices (`rtl_airband.cpp:1044`)
- **Output thread** (1 total by default; 1 per device + 1 for mixers if `multiple_output_threads=true`) — MP3 encoding and streaming
- **Mixer thread** (1 total, only if any mixers are configured) — processes all mixers; not per-mixer (`rtl_airband.cpp:1091-1092`)

## Configuration Format

Config files use libconfig++ syntax. Sample configs in `config/`. Top-level sections:

```
devices: ( { type = "rtlsdr"; centerfreq = 120.0; gain = 25;
             channels: ( { freq = 119.5; modulation = "AM";
                           outputs: ( { type = "icecast"; ... } ); } ); } );
mixers: { mix1: { outputs: ( { type = "icecast"; ... } ); } };
```

Mixers have **no `inputs` section** — a channel connects to a mixer implicitly via an output `type = "mixer"` + `name` (+ optional `ampfactor`, `balance`), see `mixer_connect_input` (`src/config.cpp:168-193`, `src/mixer.cpp:57`).

Output types: `icecast`, `file`, `rawfile`, `udp_stream`, `mixer`, `pulse`.

`file`/`rawfile` outputs with `split_on_transmission = true` can set `split_include_transmission_start` (per output > global > default `false`; the global is parsed in `main()` in `src/rtl_airband.cpp`, the per-output value in `parse_split_file_times` in `src/config.cpp`). It changes only file names, never when files close. A transmission cut by `split_max_file_time` is named `<template>_<tx start>_<part start>[_<freq>]<suffix>`; part 1 repeats the tx start, and unsplit transmissions keep `<template>_<start>`. Names are built by `build_output_filename` (`src/output.cpp`). Part 1 is written under the standalone `.tmp` name and renamed to the part name in `close_if_necessary` when it is split; parts 2+ open under the part name, in the tx start's dated folder. A file that reaches max time while squelch is closed ends the transmission, as it always closed there. Examples: `config/split_include_transmission_start*.conf`.

`split_tx_drop_shorter_than` (seconds, global or per output, default `0` = off, must be `< split_max_file_time`) deletes the `.tmp` file instead of renaming it when a transmission that fit in one file spans (first to last write) less than the limit; every part of a split transmission is kept. The check is in `close_if_necessary` (`should_drop_split_tx`), so files closed at shutdown are never dropped. Drops are counted per channel frequency in the stats file as `channel_split_tx_dropped_counter`.

## Stats / Metrics

Histograms, per-frequency transmission stats, per-device 8-bit input stats and the Prometheus text writers live in `src/metrics.cpp/h`.

With `stats_filepath` set, the first output thread rewrites a Prometheus text-format file every `stats_update_interval` seconds (default 15) and once more at exit (`write_stats_file()` in `output.cpp`). It is written to `<path>.tmp` and renamed into place. Rendering is table-driven (`freq_metrics[]`, `transmission_metrics[]`), so a new per-frequency metric is one row in `freq_metrics[]`; the original metric names, help texts and label formatting must stay unchanged so existing dashboards keep working.

- Per-transmission stats: `Squelch` tracks each transmission from OPEN to CLOSED and flags `transmission_ended()` for one sample; the demod loop then records SNR (peak level vs. the noise floor when squelch opened) and duration into `freq_t::transmission_stats` (histograms).
- Per-device input stats: the demod loop feeds only the newly consumed 8-bit bytes of each iteration to `device_t::input_stats` (clipped samples, sample count, power sum). Not collected for S16/F32 inputs.
- Stats are written by the demod thread only and read by the output thread; counters are atomics updated with relaxed load + store.
- `freq_t` and `device_t` are `calloc`ed, so the stats members are constructed with placement new in `config.cpp`.

`write_metrics()` in `output.cpp` produces the whole text; the stats file and the HTTP endpoint both use it, so they always contain the same metrics.

### HTTP endpoint (`metrics_listen`)

With `metrics_listen = "host:port";` (also `"[::1]:9101"`, or `":9101"` for all interfaces, IPv6 dual-stack where available) rtl_airband serves the metrics at `GET /metrics` (and `HEAD`), independent of `stats_filepath`. The code is in `src/metrics_server.cpp/h`:

- `MetricsServer` is a deliberately minimal HTTP/1.1 server on its own `std::thread`: one connection at a time, one request per connection (`Connection: close`), an 8 KiB request-head limit (431) and a 5 s per-request deadline. It is bound after daemonizing; a bind failure aborts startup. `main()` stops it before joining the output threads.
- The HTTP thread never touches channel state. `MetricsExchange` hands the request to the first output thread (`device_start == 0`, the same one that writes the stats file), which checks the lock-free `pending()` once per loop (~125 ms), renders with `open_memstream()` and publishes. A render counts only if it started after the request (`begin_render()` generation), so a scrape never gets data older than itself. If the output thread does not answer within 2 s the request gets 503.
- `handle_metrics_request()` and `parse_listen_address()` are pure functions, unit tested in `src/test_metrics_server.cpp`; `system_tests/tests/test_metrics_endpoint.py` scrapes a running binary and parses the response with `prometheus_client`.

`channel_squelch_open` is written by `output_squelch_open_metric()` rather than a `freq_metrics[]` row, because in scan mode it needs the channel's `freq_idx`: a frequency the scanner has left keeps its squelch's last state, so only the tuned one may report 1.

Non-finite values are written as `NaN`, `+Inf` and `-Inf` (printf's `-nan` is rejected by Prometheus parsers).
