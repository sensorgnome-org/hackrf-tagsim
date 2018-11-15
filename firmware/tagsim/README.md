### Simple firmware to simulate wildlife radio telemetry tags ###

With this firmware loaded into SPI flash, the HackRF-One serves
as a free-standing tag simulator, needing USB only for power
(i.e. it operates *without* an attached computer controlling the
unit).

It emits these tags:

- Lotek tag ID 1 (from their codeset 4).  Each tag burst consists of four
  pulses 2.5 ms long.  The ID "1" is encoded by the gaps between these
  pulses.  This ID is emitted at the following frequencies:

  150.1  MHz
  150.34 MHz
  151.5  MHz
  166.38 MHz

- CTT ("lifetag") tag ID 0x61337F34.  This ID is emitted at 434.000
  MHz.  It consists of a 32-bit pre-amble, then the ID code, emitted
  high-order bit first, via binary frequency-shift keying between
  434.025 MHz and 433.975 MHz (i.e. carrier +/- 25 kHz, with +25 kHz =
  "1", and -25 kHz = "0").  The bit clock rate is 25 kHz.

All tags repeat on a cycle of 1.000 seconds.

Every 20 seconds, the signal strength is changed by 15 units on a
linear scale from 128 to 0; i.e. sig(n+1) = sig(n) - 15 (modulo 129)
For the Lotek tags, an offset is added to the carrier frequency, and
this also changes every 20 seconds, by +131 Hz, but wrapping in the range
-4095..4226:
```C
if (lotek_dfreq < 4096) {
  lotek_dfreq += 131;
 } else {
  lotek_dfreq -= 8192;
 }
```

The hackRF LEDs indicate what is being transmitted:

 - □□🔴 Lotek tag @ 150.1 MHz
 - □💛□ Lotek tag @ 150.34 MHz
 - □💛🔴 Lotek tag @ 151.5 MHz
 - 💚□□ Lotek tag @ 166.38 MHz
 - 💚💛🔴 CTT tag @ 434 MHz
