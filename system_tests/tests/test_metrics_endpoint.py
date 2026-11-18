"""
test_metrics_endpoint.py — metrics_listen serves the stats over HTTP at /metrics.

The endpoint is scraped while rtl_airband runs, and the response must parse as the
Prometheus text format and contain the same metric families as the stats file.
"""

import socket
import subprocess
import time
import urllib.error
import urllib.request
from pathlib import Path

import pytest
from conftest import BinaryUnderTest, rtl_airband_cmd
from helpers import config_writer, iq_generator
from prometheus_client.parser import text_string_to_metric_families

SAMPLE_RATE = 2_048_000
CENTERFREQ_HZ = 120_000_000
CHANNEL_OFFSET_HZ = 25_000
# Same fixture as test_am_squelch_closed so the cached IQ file is shared.
DURATION_S = 10.0
# The endpoint is only up while the process runs, so replay in real time regardless of --mode.
SPEEDUP_FACTOR = 1.0
POLL_S = 0.1
TIMEOUT_S = DURATION_S * 3 + 30
PROMETHEUS_CONTENT_TYPE = "text/plain; version=0.0.4; charset=utf-8"


def pytest_generate_tests(metafunc):
    """Run against one binary only; the endpoint is modulation-independent."""
    if "binary_under_test" in metafunc.fixturenames:
        am_bins: list[BinaryUnderTest] = metafunc.config._rtlsdr_am_binaries
        metafunc.parametrize(
            "binary_under_test",
            am_bins[:1],
            ids=[b.label for b in am_bins[:1]],
        )


def _free_port() -> int:
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
        s.bind(("127.0.0.1", 0))
        return s.getsockname()[1]


def _write_config(test_output_dir: Path, cache_dir: Path, metrics_listen: str) -> Path:
    iq_file = iq_generator.get_or_generate_noise(
        duration_s=DURATION_S,
        cache_dir=cache_dir,
    )
    config_path = test_output_dir / "rtl_airband.conf"
    config_writer.write_config(
        config_path=config_path,
        iq_filepath=iq_file,
        sample_rate=SAMPLE_RATE,
        centerfreq_hz=CENTERFREQ_HZ,
        channels=[
            {
                "freq_hz": CENTERFREQ_HZ + CHANNEL_OFFSET_HZ,
                "output_filename_template": "metrics_endpoint",
            }
        ],
        output_dir=test_output_dir,
        speedup_factor=SPEEDUP_FACTOR,
        mode="multichannel",
        mp3_tmp_dir=test_output_dir,
        stats_filepath=test_output_dir / "stats.txt",
        metrics_listen=metrics_listen,
    )
    return config_path


def _request(url: str, method: str = "GET") -> tuple[int, dict[str, str], str]:
    req = urllib.request.Request(url, method=method)
    try:
        with urllib.request.urlopen(req, timeout=5) as resp:
            return resp.status, dict(resp.headers), resp.read().decode("utf-8")
    except urllib.error.HTTPError as err:
        return err.code, dict(err.headers), err.read().decode("utf-8")


def _stop(proc: subprocess.Popen) -> None:
    # SIGTERM first: sudo relays it to rtl_airband, but cannot relay SIGKILL
    proc.terminate()
    try:
        proc.wait(timeout=10)
    except subprocess.TimeoutExpired:
        proc.kill()
        proc.wait()


def _family_names(text: str) -> set[str]:
    return {family.name for family in text_string_to_metric_families(text)}


def test_metrics_endpoint_serves_prometheus_text(
    binary_under_test: BinaryUnderTest,
    test_output_dir: Path,
    cache_dir: Path,
) -> None:
    """While running, GET /metrics returns parseable Prometheus text matching the stats file."""
    port = _free_port()
    base = f"http://127.0.0.1:{port}"
    config_path = _write_config(test_output_dir, cache_dir, f"127.0.0.1:{port}")
    log_path = test_output_dir / "rtl_airband.log"

    scraped: tuple[int, dict[str, str], str] | None = None
    not_found = method_not_allowed = None
    # Log to a file rather than a pipe: an unread pipe can fill and block the process.
    with open(log_path, "w", encoding="utf-8") as log_file:
        with subprocess.Popen(
            rtl_airband_cmd(binary_under_test.path, config_path),
            stdout=log_file,
            stderr=subprocess.STDOUT,
            # sudo ignores signals from its own process group, which would include pytest
            start_new_session=True,
        ) as proc:
            deadline = time.monotonic() + TIMEOUT_S
            while proc.poll() is None and scraped is None:
                if time.monotonic() > deadline:
                    _stop(proc)
                    pytest.fail(f"no successful scrape within {TIMEOUT_S}s")
                try:
                    result = _request(f"{base}/metrics")
                except OSError:
                    time.sleep(POLL_S)
                    continue
                if result[0] == 200:
                    scraped = result
                    not_found = _request(f"{base}/")
                    method_not_allowed = _request(f"{base}/metrics", method="POST")
                else:
                    time.sleep(POLL_S)
            proc.wait(timeout=TIMEOUT_S)

    log = log_path.read_text(encoding="utf-8", errors="replace")
    assert proc.returncode == 0, f"rtl_airband exited with {proc.returncode}:\n{log}"
    assert scraped is not None, f"rtl_airband exited before /metrics answered:\n{log}"

    _, headers, body = scraped
    assert headers["Content-Type"] == PROMETHEUS_CONTENT_TYPE
    endpoint_families = _family_names(body)
    assert "channel_squelch_open" in endpoint_families
    assert "rtl_airband_build_info" in endpoint_families
    # the endpoint and the file come from the same writer
    stats_families = _family_names((test_output_dir / "stats.txt").read_text())
    assert endpoint_families == stats_families

    assert not_found is not None and not_found[0] == 404
    assert method_not_allowed is not None and method_not_allowed[0] == 405
    assert method_not_allowed[1]["Allow"] == "GET, HEAD"


@pytest.mark.parametrize("spec", ["9101", "::1:9101", "host:0", "host:99999"])
def test_metrics_listen_rejects_invalid_address(
    binary_under_test: BinaryUnderTest,
    test_output_dir: Path,
    cache_dir: Path,
    spec: str,
) -> None:
    """An unparseable metrics_listen is a configuration error."""
    config_path = _write_config(test_output_dir, cache_dir, spec)

    result = subprocess.run(
        rtl_airband_cmd(binary_under_test.path, config_path),
        capture_output=True,
        text=True,
        timeout=TIMEOUT_S,
        check=False,
    )

    assert result.returncode != 0, f"rtl_airband accepted metrics_listen={spec!r}"
    assert f'Configuration error: metrics_listen "{spec}"' in result.stderr


def test_metrics_listen_port_in_use_fails_startup(
    binary_under_test: BinaryUnderTest,
    test_output_dir: Path,
    cache_dir: Path,
) -> None:
    """If the port cannot be bound, rtl_airband exits instead of running without metrics."""
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as blocker:
        blocker.bind(("127.0.0.1", 0))
        blocker.listen()
        port = blocker.getsockname()[1]
        config_path = _write_config(test_output_dir, cache_dir, f"127.0.0.1:{port}")

        result = subprocess.run(
            rtl_airband_cmd(binary_under_test.path, config_path),
            capture_output=True,
            text=True,
            timeout=TIMEOUT_S,
            check=False,
        )

    assert (
        result.returncode != 0
    ), "rtl_airband started although the metrics port was taken"
    assert "Cannot listen for metrics" in result.stderr
