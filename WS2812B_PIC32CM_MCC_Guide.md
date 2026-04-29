# WS2812B On PIC32CM JH With MCC

This note is based on a review of the reference drivers in `pluss context`:

- AVR bit-banged driver: exact cycle counting, interrupts disabled, direct port writes
- STM32 PWM driver: convert LED bits into timer compare values
- STM32 TIM+PWM+DMA design note: use a peripheral to generate the waveform and DMA to keep CPU load low

The reference code is not directly portable to PIC32CM JH, but the important design ideas are reusable.

## Short Answer

For PIC32CM JH, the best first robust solution is:

1. Use `SERCOM SPI` to generate an encoded WS2812 bitstream.
2. Use `blocking SPI transmit` first, because you only have 9 LEDs and it is simple.
3. Add `DMAC` later if you want a non-blocking production driver.

Do not use `TC` first unless you are ready to build a PWM/compare plus DMA driver.

Important board constraint:

- Your current WS2812 pin `PA02` is not a good peripheral-output candidate for this job.
- In the device pack, `PA02` has no `SERCOM`, `TCx_WO`, or `TCCx_WO` mux option.
- That means `PA02` can only be used as plain GPIO for WS2812 on this board revision.

So there are really 2 cases.

## Case 1: Current PCB Revision, WS2812 Still On PA02

If the strip data line is physically routed to `PA02`, then you cannot solve this in MCC by assigning the pin to `SERCOM SPI` or `TC/TCC waveform`, because the silicon pin mux does not offer those functions on `PA02`.

Your options on this PCB revision are:

1. Keep a `GPIO bit-bang` implementation.
2. Use very tight timing, direct register writes, and disable interrupts during transfer.
3. Keep the LED count small and frame rate modest.

This can work for testing, but it is the least robust approach.

## Case 2: Recommended Next Revision Or Rewire

If you can move the WS2812 data signal to a different unused pin, the cleanest approach is `SERCOM SPI`.

### Recommended Pins

The following currently-unused pins are much better choices because they support both `SERCOM` and timer waveform outputs:

- `PA14`: `SERCOM2_PAD2`, `SERCOM4_PAD2`, `TC4_WO0`, `TCC0_WO4`
- `PA15`: `SERCOM2_PAD3`, `SERCOM4_PAD3`, `TC4_WO1`, `TCC0_WO5`
- `PA12`: `SERCOM2_PAD0`, `SERCOM4_PAD0`, `TCC0_WO6`, `TCC2_WO0`
- `PA13`: `SERCOM2_PAD1`, `SERCOM4_PAD1`, `TCC0_WO7`, `TCC2_WO1`
- `PB00`: `SERCOM5_PAD2`, `TC3_WO0`, `TC6_WO0`
- `PB01`: `SERCOM5_PAD3`, `TC3_WO1`, `TC6_WO1`
- `PB02`: `SERCOM5_PAD0`, `TC2_WO0`, `TC5_WO0`
- `PB03`: `SERCOM5_PAD1`, `TC2_WO1`, `TC5_WO1`
- `PA30`: `SERCOM1_PAD2`, `TCC1_WO0`
- `PA31`: `SERCOM1_PAD3`, `TCC1_WO1`

Best practical recommendation for your design:

1. Move WS2812 data to `PA14`.
2. Reserve `PA15` as the matching `SCK` pin for the same SERCOM instance.
3. Use `SERCOM2` or `SERCOM4` in SPI master mode.

Why `PA14/PA15`:

- Both are unused in your current project.
- They are next to each other and easy to route.
- They also keep the timer/PWM option open later if you ever want to move away from SPI encoding.

## What The Working Reference Libraries Teach

## 1. The WS2812 protocol is timing-dominant

Things every implementation must guarantee:

- Bit period: about `1.25 us` (`800 kHz`)
- One LED frame: `24 bits`, in `GRB` order
- Logical `0`: shorter high time, longer low time
- Logical `1`: longer high time, shorter low time
- Reset/latch: keep line low for at least `50 us`; using `80 us` is a good safety margin

If the waveform timing is wrong, the strip will appear dead even though the MCU is running.

## 2. Bit-bang works only when everything is controlled tightly

The AVR driver works because it does all of this:

- Direct register access
- Cycle-counted assembly
- Fixed CPU frequency
- Interrupts disabled during transmission

That is why copying the general idea into ordinary C with approximate delays is risky.

## 3. Timer/PWM drivers work by converting bits into waveform entries

The STM32 PWM driver works by converting each WS2812 bit into one peripheral waveform slot:

- `0` maps to a low duty value
- `1` maps to a high duty value
- reset maps to a run of zeros

This is robust because the timer generates the waveform, not the CPU.

## 4. DMA makes the robust design practical

The STM32 DMA example shows the real production pattern:

- keep a color buffer in RGB or GRB form
- expand it into an encoded transmission buffer
- let DMA feed the peripheral
- use interrupts only to refill chunks or detect completion

That is the architecture you should target if this becomes part of a safety-related product later.

## Recommended PIC32CM Software Architecture

## Phase 1: Reliable bring-up

Use:

- `SERCOM SPI master`
- `blocking transmit`
- prebuilt encoded buffer

Why this is the best first step:

- easy to reason about
- no cycle-counted CPU code
- no DMA dependency at first
- only 9 LEDs, so transfer size is small

## Phase 2: Cleaner runtime behavior

Add:

- `DMAC` for SPI TX
- non-blocking frame update
- double buffering if animation rate increases

## Phase 3: Production-quality driver

Add:

- frame validity checks
- consistent reset timing
- explicit state machine
- update rate control
- diagnostics if transmit stalls or configuration is invalid

## Recommended SPI Encoding

For PIC32CM SPI, use `3-bit encoding` at `2.4 MHz`.

Each WS2812 bit becomes 3 SPI bits:

- WS2812 `0` -> SPI bits `100`
- WS2812 `1` -> SPI bits `110`

Why this works:

- SPI bit time at `2.4 MHz` is about `416.7 ns`
- `100` gives about `416 ns high + 833 ns low`
- `110` gives about `833 ns high + 416 ns low`
- total is still `1.25 us` per WS2812 bit

This is one of the simplest and most widely-used SPI-based WS2812 tricks.

## Buffer Sizing For Your 9 LEDs

For `9` RGB LEDs:

- raw LED payload = `9 * 24 = 216 WS bits`
- SPI encoded bits = `216 * 3 = 648 bits`
- encoded bytes = `648 / 8 = 81 bytes`

Reset/latch padding:

- at `2.4 MHz`, one SPI bit is `416.7 ns`
- `80 us` reset needs about `192` SPI zero bits
- `192 bits = 24 bytes`

So a good first transmit buffer is:

- `24 bytes` zero prefix or suffix for reset
- `81 bytes` encoded LED data
- optionally another `24 bytes` zero suffix

Practical first choice:

- send `81 + 24 = 105 bytes`
- keep MOSI low when idle

Safer choice:

- send `24 + 81 + 24 = 129 bytes`

## Data Preparation Rules

Your WS2812 driver should do this:

1. Store user colors in `RGB` or `GRB` form.
2. Convert to `GRB` transmission order.
3. Expand each original data bit to `100` or `110`.
4. Append reset zeros.
5. Transmit the full buffer.

Keep this separate from animation logic.

Recommended module split:

- `ws2812b.h/.c`: strip driver only
- `led_effects.c`: animations only
- `main.c`: initialization and update loop only

## MCC Recommendation

## Preferred MCC setup: SERCOM SPI

Use this if you can move the WS2812 data line off `PA02`.

### Suggested MCC configuration

1. Add a new `SERCOM` instance in `SPI Master` mode.
2. Use a dedicated unused SERCOM, preferably on `PA14/PA15`.
3. Route:
   - `MOSI/DO` to `PA14`
   - `SCK` to `PA15`
4. `MISO` not used.
5. Hardware chip select not used.
6. Set SPI clock to `2.4 MHz`.
7. Use `8-bit` transfers.
8. Use `MSB first`.
9. Use `SPI mode 0`.
10. Leave the line idle low.

### MCC pin settings for the WS2812 data pin

If using SPI/peripheral output:

- configure the pin as `SERCOM peripheral`, not GPIO
- no pull-up
- no pull-down
- no input enable unless MCC requires it
- standard push-pull output
- normal drive strength is usually enough

### Important MCC note

You may need to let MCC assign both `MOSI` and `SCK` even though the LED strip only uses the data line.

That is fine.

- Only connect the `MOSI` pin to the strip DIN
- Leave the selected `SCK` pin unconnected on the PCB if necessary

## Alternative MCC setup: TC/TCC + DMA

Use this only if you specifically want PWM/timer-driven WS2812.

This path needs:

1. A pin with `TCx_WO` or `TCCx_WO`
2. A timer configured for `800 kHz` update rate or equivalent waveform period
3. Compare values representing logical `0`, logical `1`, and reset
4. `DMAC` to stream the compare values

This is more complex in MCC/Harmony than SPI encoding.

Recommendation:

- do not start with TC/TCC unless you already know you need timer waveform generation

## Which one should you choose?

For your project right now:

1. `SERCOM SPI` is the better first implementation.
2. `TC/TCC + DMA` is the better advanced implementation if you want a more canonical peripheral waveform engine later.

If you ask only what to configure in MCC for the next working revision, the answer is:

1. move the data pin away from `PA02`
2. put the new WS2812 signal on `PA14`
3. configure `SERCOM SPI Master`
4. encode WS2812 bits into SPI bytes in software

## Electrical Requirements You Must Not Skip

Successful code alone is not enough. The hardware must also be correct.

Required items:

- common ground between MCU and LED supply
- proper LED supply current capacity
- bulk capacitor near strip power input, typically `>= 100 uF`
- series resistor on data line, typically `220-470 ohm`
- short data path to first LED

Strong recommendation:

- if LEDs run at `5 V`, use a logic-level shifter from `3.3 V` MCU to LED DIN
- common choice: `74AHCT` family

This matters because a `3.3 V` output is often marginal for a `5 V` WS2812B input.

## Practical Implementation Order

## If you keep the current PCB

1. Keep `PA02` as GPIO.
2. Write a strict bit-bang driver with direct register writes.
3. Disable interrupts during LED transfer.
4. Keep frame updates simple.
5. Use this only as a temporary bring-up solution.

## If you can revise or rewire the LED data pin

1. Move data to `PA14`.
2. Add `SERCOM2` or `SERCOM4` SPI master in MCC.
3. Route `MOSI` to `PA14`, `SCK` to `PA15`.
4. Implement 3-bit SPI encoding at `2.4 MHz`.
5. Get blocking transmit working first.
6. Add `DMAC` only after the blocking version works.

## Suggested Acceptance Tests

Before writing animations, validate these in order:

1. Send a full-off frame and confirm the line is quiet afterward.
2. Send one LED red only.
3. Send one LED green only.
4. Send one LED blue only.
5. Move the lit LED across the chain.
6. Confirm reset timing by repeating frames slowly.
7. Only then add higher-level effects.

## Final Recommendation

For this PIC32CM JH project, choose `SERCOM SPI`, not `TC`, for the first serious WS2812 implementation.

The main reasons are:

- simplest MCC setup
- easiest software architecture
- good timing stability
- no CPU cycle-counting tricks
- easy upgrade path to DMA

For the current board revision, if the LED line is fixed to `PA02`, there is no MCC peripheral configuration that will turn `PA02` into a good SPI or timer-driven WS2812 output. On this board revision, `PA02` means GPIO bit-bang only.