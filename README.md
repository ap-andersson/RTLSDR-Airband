# RTLSDR-Airband

> [!NOTE]
> **This is a fork** of [rtl-airband/RTLSDR-Airband](https://github.com/rtl-airband/RTLSDR-Airband).
> It follows upstream closely (upstream is merged in regularly) and adds the changes below. Apart
> from this section, the README and the [documentation](https://github.com/rtl-airband/RTLSDR-Airband/wiki)
> are upstream's. The fork is the radio half of [airband-hub](https://github.com/ap-andersson/airband-hub).

## How this fork differs from upstream

### New features

- **Prometheus metrics over HTTP.** A top-level `metrics_listen` setting serves the same
  Prometheus text as the stats file at `GET /metrics`, current at each scrape and independent of
  `stats_filepath`:

  ```
  metrics_listen = "0.0.0.0:9101";   # ":9101" for all interfaces, "[::1]:9101" for IPv6
  ```

  An invalid address or a port that cannot be bound stops startup. Non-finite values are written
  as `NaN`/`+Inf`/`-Inf`, which Prometheus parsers accept.
- **More metrics**, in both the stats file and `/metrics`. The existing metric names are unchanged.
  - `channel_transmission_snr_db` and `channel_transmission_duration_seconds`: histograms recorded
    for every transmission when the squelch closes, so none are missed between scrapes. The
    duration's `_sum` is the total airtime.
  - `channel_dbfs_squelch_level`: the squelch threshold on the same dBFS scale as the noise and
    signal levels.
  - `channel_squelch_open`: 1 while a frequency's squelch is open. In scan mode only the tuned
    frequency can be 1.
  - `device_samples_total`, `device_clipped_samples_total` and `device_input_power_sum_total`:
    raw I/Q sample counters per device, for the clipping ratio and mean input power over any window.
  - `rtl_airband_build_info`.
- **The MP3 bitrate and VBR mode can be set per output**, with `bitrate` (kbit/s: 8, 16, 24, 32, 40,
  48, 56, 64, 80, 96, 112, 128, 144 or 160) and `vbr` (`true`/`false`). The defaults, 16 kbit/s VBR,
  are upstream's fixed settings. A CBR stream keeps a constant, short delay in players, while a VBR
  file output stays small on disk.

### Fixes

- **Scan mode no longer hops away from a transmission it just tuned to.** The 200 ms dwell was
  shorter than the time a new signal needs to reach the squelch, so transmissions starting late in
  a dwell were missed until the next scan cycle.
- **Scan mode no longer leaks the previous frequency's audio into the next one.** Samples from
  before a retune are dropped instead of running through the new frequency's squelch, which used
  to cause short false transmissions and clip the start of real ones.

### Container image and CI

- The Dockerfile has an `NFM` build argument (default `TRUE`). Build with `--build-arg NFM=FALSE`
  for AM only, since NFM support also doubles the audio rate from 8 to 16 kHz. Rebuilds reuse a
  cached build tree.
- GitHub Actions publishes an **AM-only image** for amd64 and arm64:
  `ghcr.io/ap-andersson/rtlsdr-airband` (`:latest` from main, `:sha-<commit>` for every build).
- Upstream's workflows are kept but only run by hand: this fork has no self-hosted runners, and
  keeping the files makes merges from upstream simpler.

None of this has been offered upstream yet.

---

![version tag](https://img.shields.io/github/v/tag/rtl-airband/RTLSDR-Airband?label=)

### CI Workflow Status

<table>
  <thead>
    <tr>
      <th></th>
      <th><a href="https://github.com/rtl-airband/RTLSDR-Airband/actions/workflows/ci_build.yml">Run CI</a></th>
      <th><a href="https://github.com/rtl-airband/RTLSDR-Airband/actions/workflows/platform_build.yml">Platform Build</a></th>
      <th><a href="https://github.com/rtl-airband/RTLSDR-Airband/actions/workflows/code_formatting.yml">Code Formatting</a></th>
    </tr>
  </thead>
  <tbody>
    <tr>
      <td>x86_64 ubuntu-22.04</td>
      <td align="center"><a href="https://github.com/rtl-airband/RTLSDR-Airband/actions/workflows/ci_build.yml"><img src="https://github.com/rtl-airband/RTLSDR-Airband/actions/workflows/ci_build.yml/badge.svg?branch=main" alt="Run CI"></a></td>
      <td></td>
      <td align="center" rowspan="11"><a href="https://github.com/rtl-airband/RTLSDR-Airband/actions/workflows/code_formatting.yml"><img src="https://github.com/rtl-airband/RTLSDR-Airband/actions/workflows/code_formatting.yml/badge.svg?branch=main" alt="Code Formatting"></a></td>
    </tr>
    <tr>
      <td>x86_64 ubuntu-24.04</td>
      <td align="center"><a href="https://github.com/rtl-airband/RTLSDR-Airband/actions/workflows/ci_build.yml"><img src="https://github.com/rtl-airband/RTLSDR-Airband/actions/workflows/ci_build.yml/badge.svg?branch=main" alt="Run CI"></a></td>
      <td></td>
    </tr>
    <tr>
      <td>ARM64 ubuntu-22.04</td>
      <td align="center"><a href="https://github.com/rtl-airband/RTLSDR-Airband/actions/workflows/ci_build.yml"><img src="https://github.com/rtl-airband/RTLSDR-Airband/actions/workflows/ci_build.yml/badge.svg?branch=main" alt="Run CI"></a></td>
      <td></td>
    </tr>
    <tr>
      <td>ARM64 ubuntu-24.04</td>
      <td align="center"><a href="https://github.com/rtl-airband/RTLSDR-Airband/actions/workflows/ci_build.yml"><img src="https://github.com/rtl-airband/RTLSDR-Airband/actions/workflows/ci_build.yml/badge.svg?branch=main" alt="Run CI"></a></td>
      <td></td>
    </tr>
    <tr>
      <td>ARM64 macos-14</td>
      <td align="center"><a href="https://github.com/rtl-airband/RTLSDR-Airband/actions/workflows/ci_build.yml"><img src="https://github.com/rtl-airband/RTLSDR-Airband/actions/workflows/ci_build.yml/badge.svg?branch=main" alt="Run CI"></a></td>
      <td></td>
    </tr>
    <tr>
      <td>ARM64 macos-15</td>
      <td align="center"><a href="https://github.com/rtl-airband/RTLSDR-Airband/actions/workflows/ci_build.yml"><img src="https://github.com/rtl-airband/RTLSDR-Airband/actions/workflows/ci_build.yml/badge.svg?branch=main" alt="Run CI"></a></td>
      <td></td>
    </tr>
    <tr>
      <td>ARM64 macos-26</td>
      <td align="center"><a href="https://github.com/rtl-airband/RTLSDR-Airband/actions/workflows/ci_build.yml"><img src="https://github.com/rtl-airband/RTLSDR-Airband/actions/workflows/ci_build.yml/badge.svg?branch=main" alt="Run CI"></a></td>
      <td></td>
    </tr>
    <tr>
      <td>x86_64 macos-15 (Intel)</td>
      <td align="center"><a href="https://github.com/rtl-airband/RTLSDR-Airband/actions/workflows/ci_build.yml"><img src="https://github.com/rtl-airband/RTLSDR-Airband/actions/workflows/ci_build.yml/badge.svg?branch=main" alt="Run CI"></a></td>
      <td></td>
    </tr>
    <tr>
      <td>x86_64 macos-26 (Intel)</td>
      <td align="center"><a href="https://github.com/rtl-airband/RTLSDR-Airband/actions/workflows/ci_build.yml"><img src="https://github.com/rtl-airband/RTLSDR-Airband/actions/workflows/ci_build.yml/badge.svg?branch=main" alt="Run CI"></a></td>
      <td></td>
    </tr>
    <tr>
      <td>ARM64 Debian Trixie (Pi 4B)</td>
      <td></td>
      <td align="center"><a href="https://github.com/rtl-airband/RTLSDR-Airband/actions/workflows/platform_build.yml"><img src="https://github.com/rtl-airband/RTLSDR-Airband/actions/workflows/platform_build.yml/badge.svg?branch=main" alt="Platform Build"></a></td>
    </tr>
    <tr>
      <td>ARM32 Debian Bookworm (Pi 3B)</td>
      <td></td>
      <td align="center"><a href="https://github.com/rtl-airband/RTLSDR-Airband/actions/workflows/platform_build.yml"><img src="https://github.com/rtl-airband/RTLSDR-Airband/actions/workflows/platform_build.yml/badge.svg?branch=main" alt="Platform Build"></a></td>
    </tr>
  </tbody>
</table>

### [Published Containers](https://github.com/rtl-airband/RTLSDR-Airband/actions/workflows/build_docker_containers.yml)

| linux/amd64 | linux/386 | linux/arm64 | linux/arm/v6 | linux/arm/v7 |
|:---:|:---:|:---:|:---:|:---:|
| [![Build Containers](https://github.com/rtl-airband/RTLSDR-Airband/actions/workflows/build_docker_containers.yml/badge.svg?branch=main)](https://github.com/rtl-airband/RTLSDR-Airband/actions/workflows/build_docker_containers.yml) | [![Build Containers](https://github.com/rtl-airband/RTLSDR-Airband/actions/workflows/build_docker_containers.yml/badge.svg?branch=main)](https://github.com/rtl-airband/RTLSDR-Airband/actions/workflows/build_docker_containers.yml) | [![Build Containers](https://github.com/rtl-airband/RTLSDR-Airband/actions/workflows/build_docker_containers.yml/badge.svg?branch=main)](https://github.com/rtl-airband/RTLSDR-Airband/actions/workflows/build_docker_containers.yml) | [![Build Containers](https://github.com/rtl-airband/RTLSDR-Airband/actions/workflows/build_docker_containers.yml/badge.svg?branch=main)](https://github.com/rtl-airband/RTLSDR-Airband/actions/workflows/build_docker_containers.yml) | [![Build Containers](https://github.com/rtl-airband/RTLSDR-Airband/actions/workflows/build_docker_containers.yml/badge.svg?branch=main)](https://github.com/rtl-airband/RTLSDR-Airband/actions/workflows/build_docker_containers.yml) |


### Major / Minor Version Changes:

Changes as of v5.1.0:
 - License is now GPLv2 [#503](https://github.com/rtl-airband/RTLSDR-Airband/discussions/503)

NOTE: Repo URL has moved to https://github.com/rtl-airband/RTLSDR-Airband see [#502](https://github.com/rtl-airband/RTLSDR-Airband/discussions/502) for info

Changes as of v5.0.0:
 - PRs will be opened directly against `main` and the `unstable` branch will no longer be used
 - Version tags will be automatically created on each merge to `main`
 - A release will be created on each `major` or `minor` version tag but not `minor` tags
 - Checking out `main` is recommended over using a release artifact to stay on the latest version
 - This repo has significantly diverged from the original project [microtony/RTLSDR-Airband](https://github.com/microtony/RTLSDR-Airband) so it has been been detached (ie no longer a fork).
 - Specific build support for `rpiv1`, `armv7-generic`, and `armv8-generic` have been deprecated for the new default `native`, see [#447](https://github.com/rtl-airband/RTLSDR-Airband/discussions/447)


## Overview

RTLSDR-Airband receives analog radio voice channels and produces
audio streams which can be routed to various outputs, such as online
streaming services like LiveATC.net. Originally the only SDR type
supported by the program was Realtek DVB-T dongle (hence the project's
name). However, thanks to SoapySDR vendor-neutral SDR library, other
radios are now supported as well.

## Documentation

User's manual is now on the [wiki](https://github.com/rtl-airband/RTLSDR-Airband/wiki).

## Contributing

Changes are merged through pull requests. Open a pull request from either a branch in
this repository or from your own fork.

Branches in this repository are named `<prefix>/<short_description>`, where the
prefix is a username or a category such as `dev` or `fix` (e.g. `dev/drop_macos14_ci`,
`fix/docker-ci-no-push-on-pr`).

VSCode setup is provided, running in the devcontainer installs all dependencies. If
developing locally, install the [pre-commit](https://pre-commit.com) hooks once after
cloning (`pre-commit install`). NOTE: [uv](https://docs.astral.sh/uv/) needs to be
installed to run the pre-commit hooks.

## Credits and thanks

I hereby express my gratitude to everybody who helped with the development and testing
of RTLSDR-Airband. Special thanks go to:

* Dave Pascoe
* SDR Guru
* Marcus Ströbel
* strix-technica
* Tomasz Lemiech
* charlie-foxtrot

## License

Copyright (C) 2022-2025 charlie-foxtrot

Copyright (C) 2015-2022 Tomasz Lemiech <szpajder@gmail.com>

Based on original work by Wong Man Hang <microtony@gmail.com>

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, see <https://www.gnu.org/licenses/>.

## Open Source Licenses of bundled code

### gpu_fft

BCM2835 "GPU_FFT" release 2.0
Copyright (c) 2014, Andrew Holme.
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

* Redistributions of source code must retain the above copyright
  notice, this list of conditions and the following disclaimer.
* Redistributions in binary form must reproduce the above copyright
  notice, this list of conditions and the following disclaimer in the
  documentation and/or other materials provided with the distribution.
* Neither the name of the copyright holder nor the
  names of its contributors may be used to endorse or promote products
  derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

### rtl-sdr

* Copyright (C) 2012 by Steve Markgraf <steve@steve-m.de>
* Copyright (C) 2015 by Kyle Keen <keenerd@gmail.com>
* GNU General Public License Version 2
