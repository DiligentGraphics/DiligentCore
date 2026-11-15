# DiligentCoreTest in Chrome

Build `DiligentCoreTest` with Emscripten, then run the CPU suite in one headless
Chrome session:

```sh
python3 Tests/DiligentCoreTest/browser/run_tests.py \
    --module /path/to/build/Tests/DiligentCoreTest/DiligentCoreTest.js \
    --output CoreCPUBrowserTestResults
```

The runner discovers Chrome/Chromium on `PATH`; use `--browser` to select an
executable. The browser must support JSPI and shared Wasm memory. A GPU is not
required. The generated `.wasm` and `.data` files must remain beside the `.js`
file. Test assets are preloaded into the module's virtual filesystem.

The CPU suite preloads four workers per reported logical CPU. Threading tests
limit their groups to that budget, accounting for concurrent coordinator and
factory threads. The reference-counting stress test runs its coordinator on a
worker and uses blocking barriers, preserving its iterations and race scenarios
without busy-waiting on the browser thread.

The runner shares its server, browser page, logging, and result collection with
the API GPU tests in `Tests/TestFramework/browser`. Every test's output is
streamed to the console. `tests.log`, `chrome.log`, and (when the suite completes)
`results.xml` are written to the output directory. Test failures, browser aborts,
missing results, and timeouts produce a nonzero exit status.

`exclude.txt` lists tests that require C++ exception catching or filesystem
operations that are not implemented in the Web build. `--filter` adds a Google
Test selection; `--no-exclusions` disables the list. `--timeout` changes the maximum number of seconds without a
test starting or finishing (default: 120). It is not a limit on the entire suite.

CI can invoke the dedicated `run-core-cpu-browser-tests` action from
`DiligentGraphics/github-action`. The action runs an existing build and uploads
logs and XML; configuration and exclusions remain in this directory.
