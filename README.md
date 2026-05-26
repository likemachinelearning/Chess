# Chess Visualizer & Trainer

## Compiling the Project

To compile the application, you need to include the following source files and have the required dependencies configured in your build environment (e.g., Visual Studio).

### Core Source Files
* `main.cpp`
* `AssetManager.hpp`
* `BoardGeometry.hpp`
* `EffectsManager.hpp`
* `EngineManager.hpp`
* `GameContext.hpp`
* `GuiRenderer.hpp`
* `PuzzleManager.hpp`
* `SettingsManager.hpp`
* `StatsManager.hpp`
* `ThemeLibrary.hpp`
* `chess.hpp` (Core chess logic)

### Required Dependencies
* **SDL3 & SDL3_image**: For window creation, input handling, and image loading.
* **OpenGL**: For hardware-accelerated rendering.
* **Dear ImGui**: For the user interface and menus.
* **nlohmann/json**: For parsing settings and puzzle data.

### Assets Directory
Ensure that the `Assets/` directory is placed in the same directory as your compiled executable (or in a relative directory reachable by the application). The app relies on this folder for textures, sounds, fonts, and puzzle sets.

---

## Setting Up Chess Engines

This application supports deep analysis and auto-play using the **Stockfish** and **Lc0 (Leela Chess Zero)** engines. For licensing and repository size reasons, the engine executables and weights are **not bundled**. You must download and place them manually for engine features to work.

### 1. Stockfish
1. Download the latest Stockfish binary from the [official Stockfish website](https://stockfishchess.org/download/).
2. Extract the downloaded archive.
3. Place the executable (e.g., `stockfish-windows-x86-64-avx512icl.exe`) in the following directory:
   `Assets/Engines/stockfish/`
*(Note: You do not need to rename the file. The app will automatically scan this folder and use the first `.exe` it finds).*

### 2. Lc0 (Leela Chess Zero)
1. Download the Lc0 binary from the [official Lc0 GitHub releases](https://github.com/LeelaChessZero/lc0/releases).
2. Download a network weights file (e.g., `791556.pb.gz`) from the Lc0 networks page.
3. Extract the Lc0 archive.
4. Place **both** the Lc0 executable and your weights file (`.pb.gz` or `.onnx`) in the following directory:
   `Assets/Engines/lc0/`
*(Note: The app will automatically detect the `.exe` and the weights file in this folder).*

## Running the Application
Once the code is compiled and the engine executables are placed in their respective `Assets/Engines/` subdirectories, run the executable. Enjoy your analysis and training!

---

## Licenses & Acknowledgments

This project is open-source under the **MIT License**.

It is built using several incredible open-source libraries. If you are redistributing this software, please ensure their respective licenses are respected:

* **[chess-library](https://github.com/disservin/chess) (chess.hpp)** - MIT License. A fast and efficient chess logic library.
* **[Dear ImGui](https://github.com/ocornut/imgui)** - MIT License. Bloat-free graphical user interface for C++ with minimal dependencies.
* **[nlohmann/json](https://github.com/nlohmann/json)** - MIT License. JSON for Modern C++.
* **[SDL3](https://github.com/libsdl-org/SDL)** - Zlib License. Simple DirectMedia Layer for cross-platform windowing and hardware access.
* **Stockfish & Lc0** - Used as external UCI engines (GPLv3). They are run as independent processes and are not bundled with this repository.
