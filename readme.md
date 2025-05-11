# RISC-V Virtual Memory Simulator


A web-based simulator demonstrating RISC-V virtual memory schemes (SV32, SV39, SV48) with C/WASM backend and WebGL visualization.

## Features

- **Supported Schemes**:
  - RV32: Bare, SV32 (4KB pages, 4MB megapages)
  - RV64: SV39 (2MB/1GB pages), SV48 (512GB/1TB pages)
- **Interactive Visualization**:
  - Page table hierarchy display
  - Address translation path highlighting
  - Physical memory mapping view
- **Educational Demos**:
  - 4KB page translation
  - Superpage (megapage/gigapage) alignment checks
  - Page fault simulation

## Installation

### Prerequisites
- Linux/macOS
- Emscripten SDK
- Python 3 (for local server)

## Usage

### Build and Run

```bash
make        # Build project
make serve  # Start web server on port 8000
```
Open `http://localhost:8000` in your browser.

#### Web Interface Controls

1. Select virtualization mode (SV32/SV39/SV48)

2. Enter virtual address in hex (e.g., 0x40000000)

3. Click "Translate" to see:

    - Physical address result

    - Page table walk visualization

    - Permission checks & fault detection


## Demo Scenarios
### Demo 1: SV32 4KB Page Translation

Preconfigured mapping: VA 0x40000000 → PA 0x10000000
1. Select mode: SV32
2. Enter address: 0x40000000
3. See translation to 0x10000000

### Demo 2: SV39 1GB Gigapage

Preconfigured gigapage at VA 0x80000000
1. Select mode: SV39
2. Enter address: 0x80004000
3. See translation to 0x30004000

### Demo 3: Page Fault Simulation

1. Select mode: SV48
2. Enter address: 0xFFFF00000000 (unmapped)
3. See page fault indicator and error details
