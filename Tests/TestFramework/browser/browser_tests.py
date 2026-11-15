"""Run C++ tests in one headless Chrome session.

Requires only Python's standard library and an installed Chrome/Chromium browser.
All selected tests execute in the same Wasm instance. Test failures, browser
errors, missing/empty Google Test XML, and timeouts fail the command.
"""

import argparse
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import tempfile
import threading
import time
from urllib.parse import unquote, urlsplit
import xml.etree.ElementTree as ET


HERE = Path(__file__).resolve().parent
PROGRESS = re.compile(r"^\[(?: RUN      |       OK |  FAILED  |  SKIPPED |==========)\]")
ANSI_ESCAPE = re.compile(r"\x1b\[[0-9;]*m")


def make_filter(selection, exclusion_file):
    exclusions = []
    if exclusion_file:
        for line in exclusion_file.read_text(encoding="utf-8").splitlines():
            pattern = line.partition("#")[0].strip()
            if pattern:
                exclusions.append(pattern)
    positive, separator, negative = selection.partition("-")
    if separator and negative:
        exclusions.insert(0, negative)
    return (positive or "*") + ("-" + ":".join(exclusions) if exclusions else "")


def find_browser():
    for name in ("google-chrome", "google-chrome-stable", "chromium", "chromium-browser", "chrome"):
        executable = shutil.which(name)
        if executable:
            return executable

    candidates = []
    if sys.platform == "win32":
        for variable in ("PROGRAMFILES", "PROGRAMFILES(X86)", "LOCALAPPDATA"):
            directory = os.environ.get(variable)
            if directory:
                candidates.append(Path(directory) / "Google/Chrome/Application/chrome.exe")
    elif sys.platform == "darwin":
        for directory in (Path("/Applications"), Path.home() / "Applications"):
            candidates.extend((directory / "Google Chrome.app/Contents/MacOS/Google Chrome",
                               directory / "Chromium.app/Contents/MacOS/Chromium"))
    for candidate in candidates:
        if candidate.is_file():
            return str(candidate)
    return None


def run(args):
    module = args.module.resolve(strict=True)
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    (output / "results.xml").unlink(missing_ok=True)
    browser = args.browser or find_browser()
    if not browser:
        raise RuntimeError("Chrome was not found; specify --browser")
    selected = make_filter(args.filter, None if args.no_exclusions else args.exclude_file)
    arguments = ["--gtest_color=no", "--gtest_filter=" + selected,
                 "--gtest_output=xml:/results.xml"]
    if args.mode != "cpu":
        arguments.insert(0, "--mode=" + args.mode)
    config = json.dumps({"module": module.name, "arguments": arguments, "mode": args.mode})
    page = (HERE / "run_tests.html").read_text(encoding="utf-8").replace(
        "CONFIG_JSON", config.replace("<", "\\u003c")).encode()
    completed = threading.Event()
    reports = []
    last_progress = time.monotonic()
    log_lock = threading.Lock()

    with (output / "tests.log").open("w", encoding="utf-8") as test_log:
        def log(text):
            nonlocal last_progress
            text = ANSI_ESCAPE.sub("", text)
            with log_lock:
                print(text, flush=True)
                test_log.write(text + "\n")
                test_log.flush()
                if PROGRESS.match(text):
                    last_progress = time.monotonic()

        class Handler(BaseHTTPRequestHandler):
            def end_headers(self):
                self.send_header("Cross-Origin-Opener-Policy", "same-origin")
                self.send_header("Cross-Origin-Embedder-Policy", "require-corp")
                self.send_header("Cache-Control", "no-store")
                super().end_headers()

            def do_GET(self):
                path = unquote(urlsplit(self.path).path)
                if path == "/":
                    self.send_response(200)
                    self.send_header("Content-Type", "text/html; charset=utf-8")
                    self.send_header("Content-Length", str(len(page)))
                    self.end_headers()
                    self.wfile.write(page)
                    return
                if not path.startswith("/module/"):
                    self.send_error(404)
                    return
                file = (module.parent / path[len("/module/"):]).resolve()
                if not file.is_relative_to(module.parent) or not file.is_file():
                    self.send_error(404)
                    return
                self.send_response(200)
                content_type = {".js": "text/javascript", ".wasm": "application/wasm"}
                self.send_header("Content-Type", content_type.get(file.suffix, "application/octet-stream"))
                self.send_header("Content-Length", str(file.stat().st_size))
                self.end_headers()
                try:
                    with file.open("rb") as source:
                        shutil.copyfileobj(source, self.wfile)
                except (BrokenPipeError, ConnectionResetError):
                    pass

            def do_POST(self):
                if self.path not in ("/log", "/result"):
                    self.send_error(404)
                    return
                try:
                    size = int(self.headers.get("Content-Length", "0"))
                    if not 0 < size <= 16 * 1024 * 1024:
                        raise ValueError("Invalid report size")
                    report = json.loads(self.rfile.read(size))
                    if self.path == "/log":
                        log(report["text"])
                    else:
                        reports.append(report)
                except (ValueError, KeyError, TypeError) as error:
                    reports.append({"fatal": f"Invalid browser report: {error}"})
                self.send_response(204)
                self.end_headers()
                if reports:
                    completed.set()

            def log_message(self, *_args):
                pass

        with ThreadingHTTPServer(("127.0.0.1", 0), Handler) as server:
            thread = threading.Thread(target=server.serve_forever, daemon=True)
            thread.start()
            try:
                # Chrome helpers may still modify the profile after its main process exits.
                # Best-effort profile cleanup must not replace the actual test result.
                with tempfile.TemporaryDirectory(prefix="diligent-browser-", ignore_cleanup_errors=True) as profile:
                    command = [browser, "--headless=new", "--no-first-run", "--no-default-browser-check",
                               "--disable-background-networking", "--disable-extensions", "--disable-sync",
                               *args.browser_arg, "--user-data-dir=" + profile,
                               f"http://127.0.0.1:{server.server_port}/"]
                    log("Browser test filter: " + selected)
                    with (output / "chrome.log").open("w", encoding="utf-8") as chrome_log:
                        process = subprocess.Popen(command, stdout=chrome_log, stderr=chrome_log)
                        try:
                            while not completed.wait(0.2):
                                if process.poll() is not None:
                                    raise RuntimeError(f"Chrome exited without results ({process.returncode})")
                                if time.monotonic() - last_progress > args.timeout:
                                    raise RuntimeError(f"No test progress for {args.timeout:g} seconds")
                        finally:
                            process.terminate()
                            try:
                                process.wait(timeout=5)
                            except subprocess.TimeoutExpired:
                                process.kill()
                                process.wait()
            finally:
                server.shutdown()
                thread.join()

        report = reports[0]
        if "fatal" in report:
            raise RuntimeError(report["fatal"])
        xml = report.get("xml")
        if not xml:
            raise RuntimeError("The browser did not produce Google Test XML")
        (output / "results.xml").write_text(xml, encoding="utf-8")
        root = ET.fromstring(xml)
        tests = int(root.get("tests", "0"))
        skipped = len(root.findall(".//testcase/skipped"))
        failures = int(root.get("failures", "0")) + int(root.get("errors", "0"))
        log(f"Browser results: {tests} tests, {failures} failures, {skipped} skipped")
        if tests == 0 or tests == skipped:
            raise RuntimeError("No tests executed successfully")
        return 0 if report.get("code") == 0 and failures == 0 else 1


def main(default_mode, default_exclude_file=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--module", type=Path, required=True, help="Built test executable JavaScript file")
    parser.add_argument("--mode", choices=("cpu", "wgpu"), default=default_mode)
    parser.add_argument("--browser", help="Chrome/Chromium executable; discovered from PATH or standard install locations")
    parser.add_argument("--browser-arg", action="append", default=[],
                        help="Additional browser option; repeat --browser-arg=OPTION as needed")
    parser.add_argument("--output", type=Path, default=Path("BrowserTestResults"))
    parser.add_argument("--filter", default="*", help="Google Test filter, before browser exclusions")
    parser.add_argument("--exclude-file", type=Path, default=default_exclude_file)
    parser.add_argument("--no-exclusions", action="store_true", help="Run without browser exclusions")
    parser.add_argument("--timeout", type=float, default=120, help="Maximum seconds without test progress")
    args = parser.parse_args()
    try:
        return run(args)
    except (OSError, RuntimeError, ValueError, ET.ParseError) as error:
        print(f"Browser tests failed: {error}", file=sys.stderr)
        print(f"Logs: {args.output.resolve()}", file=sys.stderr)
        return 1

