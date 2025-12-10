# ELEC2645 Unit 2 – Interactive Circuit Design Workbench

This project is a command-line **electronics workbench** developed for the ELEC2645 Unit 2 assessment.  
It provides a set of integrated calculation and analysis tools for common circuits, with a shared
“workbench” state so that calculations can build on each other.

All tools are accessed via a text menu in `main.c`, and their core logic lives in `funcs.c`.

---

## 1. Project Overview

The application is designed to support a student (or engineer) doing quick design-level calculations:

- Decode/encode resistor colour bands and approximate arbitrary values to the **E24** series  
- Perform **Ohm’s Law** and power calculations with shared variables  
- Analyse simple **voltage dividers**  
- Simulate **RC / RL / LC / RLC** transients numerically and display them as an ASCII strip chart  
- Design **LED series resistors** with automatic E24 selection  
- Design **op-amp gains** (non-inverting and inverting) using realistic E24 resistor pairs  
- Automatically log all calculations into a **history table** and optional **CSV file**

The codebase is deliberately structured to demonstrate:

- Separation of concerns (`main.c` = UI/menu, `funcs.c` = logic)
- Modular design (each tool is a separate `menu_item_X` function)
- Re-use of shared state (the “workbench” variables) across tools
- Simple but effective text-based visualisation (vertical strip charts)

---

## 2. Workbench Concept (Shared State)

Several tools share a set of **global workbench variables** declared in `funcs.c`:

- `g_wb_voltage`  – default 10.0 V  
- `g_wb_resistor` – default 4.7 kΩ  
- `g_wb_capacitor` – default 1 µF  
- `g_wb_current` – default 1 mA  
- `g_wb_vf` – default diode / LED forward drop 0.7 V  
- `g_wb_inductor` – default 10 mH  

Many prompts use these as **defaults** and update them after a calculation.  
This allows a natural workflow, for example:

1. Use Ohm’s Law to compute a resistor → workbench `R` is updated  
2. Use the voltage divider tool, which starts from the same `R` as a suggested value  
3. Use the LED calculator, again reusing `Vs`, `R`, and target current

Values can be entered using engineering suffixes:  

- `p` (pico), `n` (nano), `u` (micro), `m` (milli), `k` / `K` (kilo), `M` (mega), `G` (giga)  
- Examples: `4.7k`, `1M`, `220`, `10u`, `1e3` etc.

---

## 3. Main Menu Tools

The main menu (printed in `print_main_menu()` in `main.c`) gives access to seven tools:

### 3.1 Item 1 – 4-Band Resistor Tool (Decode & Encode)

**Filename:** `funcs.c` → `menu_item_1`

Two modes:

1. **Colour Bands → Resistance**

   - User enters:
     - Band 1 digit (0–9)  
     - Band 2 digit (0–9)  
     - Multiplier index (0–6 → ×1, ×10, ×100, ×1k, ×10k, ×100k, ×1M)  
   - Tool computes:  
     $R = (10 \times \text{Band1} + \text{Band2}) \times 10^{\text{multiplier}}$
   - Assumes **±5% (Gold)** tolerance.
   - Updates `g_wb_resistor` to this value.
   - **History entry** includes:
     - Full colour string, e.g. `Brown-Black-Red-Gold`
     - Result like `4.70kOhms +/- 5% [Brown-Black-Red-Gold]`.

2. **Resistance → Colour Bands (Nearest E24)**

   - User enters a resistance value (supports engineering suffixes).
   - The value is approximated to the nearest **E24 standard**.  
   - The E24 value is then converted back into a 4-band code:
     - Band 1 digit
     - Band 2 digit
     - Multiplier ($10^n$)  
     - Fixed tolerance band = **Gold (5%)**
   - Example history result:
     - Inputs: `Req=5.3kOhms,E24=5.6kOhms,Bands=Green-Blue-Red-Gold`
     - Results: `5.6kOhms -> Green-Blue-Red-Gold (5%, Gold)`

> **Range:** colour encoding currently targets approximately **10 Ω to 9.9 MΩ** (×10⁰ to ×10⁶).

---

### 3.2 Item 2 – Ohm’s Law & Power

**Filename:** `funcs.c` → `menu_item_2`

Modes:

1. **V = I × R**  
2. **I = V / R**  
3. **R = V / I**  
4. **P = V × I**

Inputs typically use the workbench defaults (`g_wb_voltage`, `g_wb_resistor`, `g_wb_current`) and then:

- Update the appropriate workbench variable (e.g. calculating `R` updates `g_wb_resistor`)  
- Log a readable history line, including the inputs and the result with engineering notation.

Example output:

- `Result: 3.40 V`  
- History: `Tool Name: Ohm's Law (V), Inputs: I=1.00e-3A, R=3.40e+3R, Results: 3.40 V`

---

### 3.3 Item 3 – Voltage Divider

**Filename:** `funcs.c` → `menu_item_3`

- Inputs:
  - $V_\text{in}$
  - $R_1$ (top resistor) – chosen from E24 via `get_standard_resistor_input`
  - $R_2$ (bottom resistor) – also E24
- Output:
```math
V_{\text{out}} = V_{\text{in}} \cdot \frac{R_2}{R_1 + R_2}
```

- History records both resistor values and the resulting output voltage.

This supports quick design of bias networks or level shifters based on standard components.

---

### 3.4 Item 4 – RLC Transient Analyser (Vertical Detail Mode)

**Filename:** `funcs.c` → `menu_item_4`

Circuit types:

1. RC (Resistor–Capacitor)  
2. RL (Resistor–Inductor)  
3. LC (Inductor–Capacitor, with small series resistance)  
4. Series RLC

Features:

- Uses a **numerical Euler method** with 1000 simulation steps (internally sub-stepped) to solve the differential equations.
- Automatically suggests a simulation time:
  - RC: $5 \cdot R \cdot C$
  - RL: $5 \cdot \dfrac{L}{R}$
  - LC: about 3 periods  
  - RLC: depends on damping ($\alpha$ vs $\omega_0$) to capture either oscillation or decay
- User can override the suggested total time.

Outputs:

- ASCII **vertical strip charts**:
  - Loop current $I(t)$  
  - Capacitor voltage $V_C(t)$ (when applicable)  
  - Energy in capacitor ($E_C = \tfrac{1}{2} C \cdot V_C^2$)  
  - Energy in inductor ($E_L = \tfrac{1}{2} L \cdot I^2$)
- Each chart prints:
  - Time (ms)
  - A bar with an `O` marker showing relative magnitude
  - Exact numeric value with suitable formatting.

History:

- Tool name: `RLC Analyser`
- Inputs: circuit type and supply voltage, e.g. `RLC Type 1, Vs=10.0V`
- Results summarise peak values and energy, e.g.  
  - RC: `PkV:5.0V Ec:1.23e-03J`  
  - RL: `PkI:2.00e-1A El:4.00e-03J`  
  - LC/RLC: `Ec:..., El:...`

---

### 3.5 Item 5 – LED Resistor Calculator (Automatic E24)

**Filename:** `funcs.c` → `menu_item_5`

Inputs:

- $V_s$ – supply voltage  
- $V_f$ – LED forward voltage  
- $I_\text{target}$ – desired LED current

Process:

1. Compute the **ideal** series resistor:
   
  
 ```math
 R_\text{ideal} = \frac{V_s - V_f}{I_\text{target}}
```
2. Approximate to the nearest **E24** resistor.
3. Recompute the **actual current** with the chosen standard value.
4. Update workbench `g_wb_resistor` and `g_wb_current`.

Outputs (console and history):

- Theoretical resistor  
- Nearest E24 resistor  
- Actual LED current using that resistor

Example result string:

- `R_std=470R, I_act=18.0mA`

---

### 3.6 Item 6 – Op-Amp Gain Designer

**Filename:** `funcs.c` → `menu_item_6`

Modes:

#### Non-inverting Amplifier

$$
G = 1 + \frac{R_2}{R_1}
$$

#### Inverting Amplifier

$$
G = -\frac{R_2}{R_1}
$$

Given a target gain magnitude:

- The tool scans through a set of E24-based $R_1$ candidates (1 kΩ to 100 kΩ),
  and for each:
  - Computes the ideal $R_2$
  - Approximates $R_2$ to the nearest E24 value
  - Calculates the **actual gain** and percentage error
- It prints a small table of “good” combinations and selects the **best** pair
  with minimal gain error.

Example final summary:

- `R1 = 10kΩ`  
- `R2 = 47kΩ`  
- `Actual Gain = 5.70 (Error: 1.23%)`

Workbench:

- `g_wb_resistor` is set to the chosen $R_1$ value for future tools.

History:

- Inputs: `Non-Inv, Tgt G=5.00`  
- Results: `R1=10k, R2=47k, G=5.70`

---

### 3.7 Item 7 – History View & CSV Export

**Filename:** `funcs.c` → `menu_item_7`

All tools add a `CalcRecord` entry via `add_record_to_history()`:

- `tool_name` – name of the tool (e.g. `4-Band Decode`, `LED Resistor Calc`)  
- `details` – summary of inputs and context  
- `result_str` – human-readable result string (may include colour codes, units, etc.)

The history viewer:

1. Prints a formatted table:

   - ID  
   - Tool Name  
   - Inputs  
   - Results  

2. Offers to **save** the history to a CSV file:

   - Prompts for a base filename, e.g. `result1`  
   - Automatically appends `.csv` if missing  
   - Writes: `Tool Name,Inputs,Results`  

This CSV can be imported into Excel / LibreOffice / Google Sheets and used as evidence in
journal entries and the Unit 2 report.

---

## 4. Building and Running the Code

### 4.1 Using `gcc` directly

In a terminal:

```bash
gcc main.c funcs.c -o main.out -lm
./main.out
