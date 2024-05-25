
* Read [WASM Debugging with Emscripten and VSCode](https://floooh.github.io/2023/11/11/emscripten-ide.html)

* Install all the VS Code extensions described in the article above, and build the project as described in
  [build instructions](https://github.com/DiligentGraphics/DiligentEngine?tab=readme-ov-file#build_and_run_emscripten)

* Create `tasks.json` file in the `.vscode` folder if it does not exist and add the following content:
  ```json
  {
      "version": "2.0.0",
      "tasks": [
          {
              "label": "StartServer",
              "type": "process",
              "command": "${input:startServer}"
          }
      ],
      "inputs": [
          {
              "id": "startServer",
              "type": "command",
              "command": "livePreview.runServerLoggingTask"
          }
      ]
  }
  ```

* Add a target to the `launch.json` file for launching, for example:
  ```json
  {
      "name": "Tutorial01_HelloTriangle_Emscripten",
      "type": "chrome",
      "request": "launch",
      "url": "http://localhost:3000/build/Emscripten/DiligentSamples/Tutorials/Tutorial01_HelloTriangle/Tutorial01_HelloTriangle.html",
      "preLaunchTask": "StartServer",
  }
  ```

* Open VSCode and press `F5` to launch the application.
