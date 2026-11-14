# Browser GPU tests

The Linux Emscripten CI jobs run `DiligentCoreAPITest` in headless Chrome with
WebGPU and SwiftShader, in both Debug and Release. All selected tests run in one
browser session. No browser automation packages are required.

Build the Emscripten configuration with `DILIGENT_BUILD_CORE_TESTS=ON` and
`DILIGENT_NO_WEBGPU=OFF`, then run:

```sh
python3 Tests/DiligentCoreAPITest/browser/run_tests.py \
    --module build/Tests/DiligentCoreAPITest/DiligentCoreAPITest.js \
    --browser "$(command -v google-chrome)" \
    --output BrowserTestResults
```

Chrome must support WebGPU and JSPI. The runner serves the built module with the
cross-origin isolation headers required for pthreads and requests a software
adapter. Keep the generated `.js`, `.wasm`, and `.data` files together.

Each test's output is printed immediately. The output directory contains
`tests.log`, `chrome.log`, and, on normal completion, Google Test `results.xml`.
Test failures, browser errors, missing results, and 120 seconds without test
progress fail the command. CI uploads these files even when tests fail.

`exclude.txt` lists current browser failures and their reasons. This is an
exclusion list, so new tests run by default. Tests that already skip unsupported
features retain their normal Google Test skip behavior.

Use `--filter 'Suite.Test*'` to select tests and
`--no-exclusions` to investigate an excluded test. `--timeout` changes the
maximum seconds without test progress. Exclusions do not affect native runs.
