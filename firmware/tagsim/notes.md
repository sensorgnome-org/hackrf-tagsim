Basic idea:

- include a buffer of byte codes to do tuning, gain-setting, and square-wave
  pattern generation:

  - 'f' [FREQ: u64, OFFSET: u32] - set transmit frequency to FREQ + value in reg OFFSET
  - 'g' [STAGE: u8, REG: u8] - set gain at STAGE to value in REG; stages: rfgain = 0 (0 or 14dB), txvgagain = 1 (0 to 47 dB)
  - '1' turn full-amp signal on; i.e. set dac_value to 0xff
  - '0' turn full-amp signal off; i.e. set dac_value to 0x00
  - 't' [TIME: u32 clocks (@ 20MHz)] - do nothing for TIME CLOCKS
  - 'a' [REG: u8, CONST: u8] - add CONST to value in REG and store in REG
  - 's' [REG: u8, CONST: u8] - store CONST in REG
  - 'L' [LABEL: u8] - set label in code
  - 'G' [LABEL: u8] - go to label

- synthesize completed USB usb transfer buffers into the hackrf_usb framework
  without

Here's what I needed to learn about the `hackrf_usb` firmware and its
support code:

usb_vendor_request(endpoint, stage):
 - dispatches to request handler based on `endpoint->setup.request`

 - most of the vendor requests we want use a data transfer, so the handler
   works in two stages SETUP and DATA;  it might be cleaner to strip
   out the underlying handler logic and call it directly, rather than
   synthesizing fake USB transfer packets:
   - set frequency: `bool set_freq(const uint64_t freq)`
   - set amplifier on or off: `void rf_path_set_lna(rf_path_t* const rf_path, const uint_fast8_t enable)`
     with `rf_path=&rf_path`
   - set tx vga gain: `bool max2837_set_txvga_gain(max2837_driver_t* const drv, const uint32_t gain_db)`
     with `drv = &max2837`
   - turn transmit on: `void set_transceiver_mode(const transceiver_mode_t new_transceiver_mode)`
     with `new_transceiver_mode = TRANSCEIVER_MODE_TX`
 - main loop:
   - look for `cpld_update` or `start_sweep` commands, and dispatch these

   - set up bulk transfer into or out of a bulk buffer as soon as we're
     finished with it.  There's a 32k USB bulk buffer, split in half for alternating
     transfers.  As soon as the offset pointer is into the phase X buffer,
     a set-up for transfer of the phase (1-X) buffer is initiated.


In hackrf_usb:main.c, replace the busy loop with the byte-code interpreter.
Since we're using fake 0 or 1 data to the dac, we don't need to queue
bulk transfers.

Instead:
 - fill the first 32 bytes of `usb_bulk_buffer` with 0,0,0,0,... (for '0')  or 0xff,0,0xff,0,... (for '1')
 - set `usb_bulk_buffer_offset` to 0
 - call `sgpio_isr_rx`



## functions we can call directly
```C
bool sample_rate_set(const uint32_t sample_rate_hz); // hackrf_core.c
bool set_freq(const uint64_t freq);


Packing shift registers:

Fail:
        while(SGPIO_STATUS_1==0);
        SGPIO_REG_SS(SGPIO_SLICE_L) = val;
        SGPIO_REG_SS(SGPIO_SLICE_F) = val;
        SGPIO_REG_SS(SGPIO_SLICE_K) = val;
        SGPIO_REG_SS(SGPIO_SLICE_C) = val;
        SGPIO_REG_SS(SGPIO_SLICE_J) = val;
        SGPIO_REG_SS(SGPIO_SLICE_E) = val;
        SGPIO_REG_SS(SGPIO_SLICE_I) = val;
        SGPIO_REG_SS(SGPIO_SLICE_A) = val;
        SGPIO_CLR_STATUS_1 = 1;

Fail:
	uint32_t val = (mag << 24) | (mag << 8);
	uint_fast8_t i;
        while(SGPIO_STATUS_1==0);
        SGPIO_REG_SS(SGPIO_SLICE_L) = val;
        SGPIO_CLR_STATUS_1 = 1;
        while(SGPIO_STATUS_1==0);
        SGPIO_REG_SS(SGPIO_SLICE_F) = val;
        SGPIO_CLR_STATUS_1 = 1;
        while(SGPIO_STATUS_1==0);
        SGPIO_REG_SS(SGPIO_SLICE_K) = val;
        SGPIO_CLR_STATUS_1 = 1;
        while(SGPIO_STATUS_1==0);
        SGPIO_REG_SS(SGPIO_SLICE_C) = val;
        SGPIO_CLR_STATUS_1 = 1;
        while(SGPIO_STATUS_1==0);
        SGPIO_REG_SS(SGPIO_SLICE_J) = val;
        SGPIO_CLR_STATUS_1 = 1;
        while(SGPIO_STATUS_1==0);
        SGPIO_REG_SS(SGPIO_SLICE_E) = val;
        SGPIO_CLR_STATUS_1 = 1;
        while(SGPIO_STATUS_1==0);
        SGPIO_REG_SS(SGPIO_SLICE_I) = val;
        SGPIO_CLR_STATUS_1 = 1;
        while(SGPIO_STATUS_1==0);
        SGPIO_REG_SS(SGPIO_SLICE_A) = val;
        SGPIO_CLR_STATUS_1 = 1;

Fail:
        while(SGPIO_STATUS_1==0);
	for (i = 0; i < 8; ++i) {
		SGPIO_REG_SS(SGPIO_SLICE_L) = val;
	}
	for (i = 0; i < 8; ++i) {
		SGPIO_REG_SS(SGPIO_SLICE_F) = val;
        }
	for (i = 0; i < 8; ++i) {
		SGPIO_REG_SS(SGPIO_SLICE_K) = val;
        }
	for (i = 0; i < 8; ++i) {
		SGPIO_REG_SS(SGPIO_SLICE_C) = val;
        }
	for (i = 0; i < 8; ++i) {
		SGPIO_REG_SS(SGPIO_SLICE_J) = val;
        }
	for (i = 0; i < 8; ++i) {
		SGPIO_REG_SS(SGPIO_SLICE_E) = val;
        }
	for (i = 0; i < 8; ++i) {
		SGPIO_REG_SS(SGPIO_SLICE_I) = val;
        }
	for (i = 0; i < 8; ++i) {
		SGPIO_REG_SS(SGPIO_SLICE_A) = val;
        }
        SGPIO_CLR_STATUS_1 = 1;

Fail:
        while(SGPIO_STATUS_1==0);
	for (i = 0; i < 8; ++i) {
		SGPIO_REG_SS(SGPIO_SLICE_L) = val;
	}
        SGPIO_CLR_STATUS_1 = 1;
        while(SGPIO_STATUS_1==0);
	for (i = 0; i < 8; ++i) {
		SGPIO_REG_SS(SGPIO_SLICE_F) = val;
        }
        SGPIO_CLR_STATUS_1 = 1;
        while(SGPIO_STATUS_1==0);
	for (i = 0; i < 8; ++i) {
		SGPIO_REG_SS(SGPIO_SLICE_K) = val;
        }
        SGPIO_CLR_STATUS_1 = 1;
        while(SGPIO_STATUS_1==0);
	for (i = 0; i < 8; ++i) {
		SGPIO_REG_SS(SGPIO_SLICE_C) = val;
        }
        SGPIO_CLR_STATUS_1 = 1;
        while(SGPIO_STATUS_1==0);
	for (i = 0; i < 8; ++i) {
		SGPIO_REG_SS(SGPIO_SLICE_J) = val;
        }
        SGPIO_CLR_STATUS_1 = 1;
        while(SGPIO_STATUS_1==0);
	for (i = 0; i < 8; ++i) {
		SGPIO_REG_SS(SGPIO_SLICE_E) = val;
        }
        SGPIO_CLR_STATUS_1 = 1;
        while(SGPIO_STATUS_1==0);
	for (i = 0; i < 8; ++i) {
		SGPIO_REG_SS(SGPIO_SLICE_I) = val;
        }
        SGPIO_CLR_STATUS_1 = 1;
        while(SGPIO_STATUS_1==0);
	for (i = 0; i < 8; ++i) {
		SGPIO_REG_SS(SGPIO_SLICE_A) = val;
        }
        SGPIO_CLR_STATUS_1 = 1;
(but at least this one works for 00)

Works:
		while(SGPIO_STATUS_1==0);
		SGPIO_REG_SS(SGPIO_SLICE_L) = val;
		SGPIO_CLR_STATUS_1 = 1;
		while(SGPIO_STATUS_1==0);
		SGPIO_REG_SS(SGPIO_SLICE_F) = val;
		SGPIO_CLR_STATUS_1 = 1;
		while(SGPIO_STATUS_1==0);
		SGPIO_REG_SS(SGPIO_SLICE_K) = val;
		SGPIO_CLR_STATUS_1 = 1;
		while(SGPIO_STATUS_1==0);
		SGPIO_REG_SS(SGPIO_SLICE_C) = val;
		SGPIO_CLR_STATUS_1 = 1;
		while(SGPIO_STATUS_1==0);
		SGPIO_REG_SS(SGPIO_SLICE_J) = val;
		SGPIO_CLR_STATUS_1 = 1;
		while(SGPIO_STATUS_1==0);
		SGPIO_REG_SS(SGPIO_SLICE_E) = val;
		SGPIO_CLR_STATUS_1 = 1;
		while(SGPIO_STATUS_1==0);
		SGPIO_REG_SS(SGPIO_SLICE_I) = val;
		SGPIO_CLR_STATUS_1 = 1;
		while(SGPIO_STATUS_1==0);
		SGPIO_REG_SS(SGPIO_SLICE_A) = val;
		SGPIO_CLR_STATUS_1 = 1;

Works:
	for (i = 0; i < 8; ++i) {
		while(SGPIO_STATUS_1==0);
		SGPIO_REG_SS(SGPIO_SLICE_L) = val;
		SGPIO_REG_SS(SGPIO_SLICE_F) = val;
		SGPIO_REG_SS(SGPIO_SLICE_K) = val;
		SGPIO_REG_SS(SGPIO_SLICE_C) = val;
		SGPIO_REG_SS(SGPIO_SLICE_J) = val;
		SGPIO_REG_SS(SGPIO_SLICE_E) = val;
		SGPIO_REG_SS(SGPIO_SLICE_I) = val;
		SGPIO_REG_SS(SGPIO_SLICE_A) = val;
		SGPIO_CLR_STATUS_1 = 1;
        }


Works:
	for (i = 0; i < 4; ++i) {
		while(SGPIO_STATUS_1==0);
		SGPIO_REG_SS(SGPIO_SLICE_L) = val;
		SGPIO_REG_SS(SGPIO_SLICE_F) = val;
		SGPIO_REG_SS(SGPIO_SLICE_K) = val;
		SGPIO_REG_SS(SGPIO_SLICE_C) = val;
		SGPIO_REG_SS(SGPIO_SLICE_J) = val;
		SGPIO_REG_SS(SGPIO_SLICE_E) = val;
		SGPIO_REG_SS(SGPIO_SLICE_I) = val;
		SGPIO_REG_SS(SGPIO_SLICE_A) = val;
		SGPIO_CLR_STATUS_1 = 1;
        }

Fails:
	for (i = 0; i < 2; ++i) {
		while(SGPIO_STATUS_1==0);
		SGPIO_REG_SS(SGPIO_SLICE_L) = val;
		SGPIO_REG_SS(SGPIO_SLICE_F) = val;
		SGPIO_REG_SS(SGPIO_SLICE_K) = val;
		SGPIO_REG_SS(SGPIO_SLICE_C) = val;
		SGPIO_REG_SS(SGPIO_SLICE_J) = val;
		SGPIO_REG_SS(SGPIO_SLICE_E) = val;
		SGPIO_REG_SS(SGPIO_SLICE_I) = val;
		SGPIO_REG_SS(SGPIO_SLICE_A) = val;
		SGPIO_CLR_STATUS_1 = 1;
        }

So each slice needs 16 bytes = 128 bits.

Would be nice to use the FM bit on the RFC5072 to do binary FSK, but that
pin (GPO3/FM) is connected to test point 19.

MAX DD0..DD9 are 10 bit inputs to DAC
these are connected to the CPLD.

CPLD pins are connected through resistors to SGPIO0..SGPIO:

NET "HOST_DATA<7>"   LOC="77" BANK2F3M7   SGPIO7
NET "HOST_DATA<6>"   LOC="61" BANK2F3M16  SGPIO6
NET "HOST_DATA<5>"   LOC="64" BANK2F3M15  SGPIO5
NET "HOST_DATA<4>"   LOC="67" BANK2F3M14  SGPIO4
NET "HOST_DATA<3>"   LOC="72" BANK2F3M10  SGPIO3
NET "HOST_DATA<2>"   LOC="74" BANK2F3M9   SGPIO2
NET "HOST_DATA<1>"   LOC="79" BANK2F3M5   SGPIO1
NET "HOST_DATA<0>"   LOC="89" BANK2F3M3   SGPIO0
so SGPIOs 0..7 carry a byte of either I or Q channel

How are SGPIO slices connected to the SGPIO pins?
In multislice mode, SGPIO0..7 are driven by slice L0...L7
(mode 8bit 8c).  In single-slice mode, SGPIO0..7 are driven
by slice A0..A7 (mode 8bit 8a).
See OUT_MUX_CFG, bits P_OUT_CFG

Going back to 2018.1.1 release, why does stuffing the SGPIO_REG
and SGPIO_REG_SS registers do nothing?

Is mixer not enabled?  Seems to be when *not* multi_slice_mode,
but when multi_slice_mode, is not enabled.

Something weird about interrupt counts etc.
at 2 x 32 x 5 register loads of a 0x7f007f00 pattern, we
see 320 active samples on the receiving hackrf_one.
This is with POS=PRESET=0xff.

We see approximately 4 samples high, 4 samples low during this 320-sample
period.  But we're only waiting for 1 interrupt on STATUS_0.
So it seems we're getting the interrupt only on every 16 bits shifted out.

If instead we wait on STATUS_1, we get 36058 samples, and the minimum
period is 5 to 6 samples at POS=PRESET=0xff.
With POS=PRESET=0x7f we get 16417 samples
With POS=PRESET=0x3f we get 5406 samples.
With POS=PRESET=0x1f we get 332 samples.
STATUS_0: POS=PRESET=0x3f we get 320 samples.

IRQ   POS=PRESET=  samples
0     0xff         320
0     0x3f         320
1     0xff         36058
1     0x7f         16417
1     0x3f         5406
1     0x1f         332

0     0xff


revert to most original settings.
Now, ctt tag gives 6775 samples when waiting on GPIO_STATUS_1
and 8290 samples when waiting on GPIO_STATUS_0

2f05b4d74c6c9c505c1414c4d66791a75ff61e00
16-sample pattern repeated 8 times, then new pattern, but only total of 7394 samples.
(need to get 20480:  64/25000*8E6)

switch to listen on STATUS_1 instead of STATUS_0 and pattern is now 47369 samples.

STATUS_0:
                SGPIO_POS(slice) =
                  SGPIO_POS_POS_RESET(0x3f)
                  | SGPIO_POS_POS(0x3f);  // exchange REG with REG_SS every 8 x 32 shifts.  i.e. after concatenated 8 regs have shifted.
		SGPIO_PRESET(slice) = 0x3f;			// External clock, don't care
		SGPIO_COUNT(slice) = 0x3f;				// External clock, don't care
16 sample pattern repeated 32 times?


STATUS_0:
                SGPIO_POS(slice) =
                  SGPIO_POS_POS_RESET(0x3f)
                  | SGPIO_POS_POS(0x3f);  // exchange REG with REG_SS every 8 x 32 shifts.  i.e. after concatenated 8 regs have shifted.
		SGPIO_PRESET(slice) = 0xff;			// External clock, don't care
		SGPIO_COUNT(slice) = 0xff;				// External clock, don't care
64-sample pattern repeated xxx times


STATUS_0:
                SGPIO_POS(slice) =
                  SGPIO_POS_POS_RESET(0xff)
                  | SGPIO_POS_POS(0xff);  // exchange REG with REG_SS every 8 x 32 shifts.  i.e. after concatenated 8 regs have shifted.
		SGPIO_PRESET(slice) = 0xff;			// External clock, don't care
		SGPIO_COUNT(slice) = 0xff;				// External clock, don't care
8 repetitions of 16-sample pattern.


STATUS_0:
                SGPIO_POS(slice) =
                  SGPIO_POS_POS_RESET(0x1f)
                  | SGPIO_POS_POS(0x1f);  // exchange REG with REG_SS every 8 x 32 shifts.  i.e. after concatenated 8 regs have shifted.
		SGPIO_PRESET(slice) = 0xff;			// External clock, don't care
		SGPIO_COUNT(slice) = 0xff;				// External clock, don't care
repetition of 32-sample pattern 100 times


worse pattern with larger values for POS_RESET, POS_POS



## Summary ##

Something weird I don't yet understand about SGPIO:
- why does this sequence not succeed in leaving (mag, 0) in the
  lowest 16 bits of the DAC  (i.e. I0/Q0)?

        SGPIO_CTRL_ENABLE = 0x00; // disable shifting so we load registers at
        // known boundary
        for (uint8_t i = 0; i < 8; ++i) {
                uint8_t slice = slice_indices[i];
                SGPIO_POS(slice) =
                        SGPIO_POS_POS_RESET(0x1f)
                        | SGPIO_POS_POS(0x1f);
        }

         uint32_t val = (sig << 24) | (sig << 8);
          SGPIO_REG_SS(SGPIO_SLICE_L) = val;
          SGPIO_REG_SS(SGPIO_SLICE_F) = val;
          SGPIO_REG_SS(SGPIO_SLICE_K) = val;
          SGPIO_REG_SS(SGPIO_SLICE_C) = val;
          SGPIO_REG_SS(SGPIO_SLICE_J) = val;
          SGPIO_REG_SS(SGPIO_SLICE_E) = val;
          SGPIO_REG_SS(SGPIO_SLICE_I) = val;
          SGPIO_REG_SS(SGPIO_SLICE_A) = val;

          SGPIO_CTRL_DISABLE = 0x0;
          SGPIO_CTRL_ENABLE = 0xFFFF; // enable shifting so we get an interrupt

          while((SGPIO_STATUS_0 & 1) == 0);
          SGPIO_CLR_STATUS_0 = 1;
        }

};

Answer (from experiments):
1. use SGPIO_STATUS_1, which triggers after each shift out of (n+1) bytes where
n is the value placed in SGPIO_POS_RESET and SGPIO_POS_POS for each slice.
2. make sure to clear the status of the interrupt before the wait loop!
3. set POS=POS_RESET=0x01 to indicate 2 shift events, each of 8 bits,
   before generating an exchange interrupt.
4. wait for a rising edge on SGPIO8, which is the double-speed sample clock.
This works, and is I/Q-correct!  Not sure why we seem to have to load both
SGPIO_REG(SGPIO_SLICE_F) and SGPIO_REG_SS(SGPIO_SLICE_F).

          SGPIO_CTRL_ENABLE = 0;
          for (uint8_t i = 0; i < 8; ++i) {
            uint8_t slice = slice_indices[i];
            SGPIO_POS(slice) =
              // do two shifts before exchanging; each shift is 8 bits, therefore
              // generate an exchange interrupt after an I and a Q sample have been shifted
              SGPIO_POS_POS_RESET(0x01)
              | SGPIO_POS_POS(0x01);
          }

          uint32_t val = (sig << 8);
          // still not sure why both of these regs need to be loaded,
          // but level setting only works 50% of the time if one of
          // them is not loaded!  Seems as if perhaps the exchange is
          // being counted by a separate clock?
          SGPIO_REG(SGPIO_SLICE_F) = val;
          SGPIO_REG_SS(SGPIO_SLICE_F) = val;

          SGPIO_CLR_STATUS_1 = 0xffff; //  needed!  Otherwise, the wait-loop
                                  // below might be a nop, if the interrupt
                                  // has been triggered and not cleared.
          // wait for rising edge of SGPIO_CLK
          while ((SGPIO->GPIO_INREG & (1 << 8)));
          while (!(SGPIO->GPIO_INREG & (1 << 8)));
          SGPIO_CTRL_ENABLE = enab_mask; // enable shifting then wait until we get an exchange interrupt (after 2 shifts)
          while((SGPIO_STATUS_1 & 1) == 0);
          SGPIO_CLR_STATUS_1 = 0xffff;
          SGPIO_CTRL_ENABLE = 0; // values shifted in - lock them there.

Okay, then why does this method no longer work when used in tagsim.c???  We get intermittent pulses there
with this method.  i.e. only about half the pulses appear.
GRRRRR!!

Apparently, if the delay between set_sig_level calls is too low, pulses are intermittent.
e.g., with 50000us, intermittent.
with 80000us, okay, but then stops.
60000us okay if all regs set to 0 first.
40000us okay if all regs set to 0 first
and even down to 350 us.

Okay very weird.  If we delay by different amounts for on vs. off, we get no signal.

1 issue - hadn't been enabling interrupts on register L, and we were waiting for
an interrupt on register A was not useful.

so:
	SGPIO_SET_EN_1 = (1 << SGPIO_SLICE_L);
and then the wait loop is:
          while((SGPIO_STATUS_1 & SGPIO_SLICE_L) == 0);
          SGPIO_CLR_STATUS_1 = 0xffff;


Seems also that we need to stuff SGPIO_REG and SGPIO_REG_SS with zeroes before starting?

Very flaky indeed!  Seems as if adding an extra "nop" into the loop affects timing enough
to throw everything off.  WTF??

With POS=POS_RESET=0 (i.e. 1 x 8 bit shift and SGPIO_REG(L) = val << 24), we get
full signal strength, but skip the first 3 pulses.

Doing this:
sig=0x80
POS=POS_RESET=0
uint32_t val = (sig << 24);
SGPIO_REG_SS(SGPIO_SLICE_L) = 0;
SGPIO_REG(SGPIO_SLICE_L) = val;
misses first 3 pulses but then is very reliably on at full signal strength

Moreover, using (sig << 23) gets half the signal strenght, so it is definitely
that top 8 bits of SGPIO_REG(SGPIO_SLICE_L) that is sent out on the 4th enable.
So where is the byte for the 1st enable coming from?
Stuffing any slot in SGPIO_REG_SS(SGPIO_SLICE_L) fails: no pulses at all.
Stuffing any slot in SGPIO_REG_SS(SGPIO_SLICE_F) fails: no pulses at all.

Stuffing byte 1 of SGPIO_REG(SGPIO_SLICE_F) with val works on first beep!

So:  if exchanging after 1 8-bit shift, value left on DAC comes from SGPIO_REG(SGPIO_SLICE_F)[15:8]
Still don't know where other value comes from if exchanging after two 8-bit shifts (i.e. POS=POS_RESET=0x1)
Actually, we do: it comes from SGPIO_REG_SS(SGPIO_SLICE_FF)[7:0]

POS_RESET  what ends up in L[7:0] and A[31:24] after 1st xchg interrupt
(and what ends up there is definitely what the DAC is outputting)
POS_RESET  L[7:0]                    A[31:24]
================================================
0          F[15:8] -20 dB = 5        F[7:0]
1          SS_F[7:0] -28 dB = 2      SS_L[31:24]
2          SS_F[7:0]                 SS_L[31:24]
3          F[23:16]  -18.8 dB = 6    F[15:8]
4          K[15:8] -20.4 dB = 5
5          K[7:0] -22.1 dB = 4
6          F[31:24] -17.5 dB = 7
7          SS_F[15:8]
8          K[15:8]
9          SS_K[7:0]
10         SS_F[31:24]
11         SS_F[23:16]
12         SS_K[15:8]
13         SS_K[7:0]
14         SS_F[31:24]
15         SS_F[23:16]
16         SS_K[15:8]
17         SS_K[7:0]
18         SS_F[31:24]
19         SS_F[23:16]
20         SS_K[15:8]
21         SS_K[7:0]
...
31         SS_F[23:16]
32         SS_K[15:8]
63         SS_F[23:16]
255        SS_F[23:16]

hmmmmm
Also makes no difference whether we wait on SGPIO8 (clock).
Something weird about multiplexing!!!

Now setting random values in all 8 REG and SS_REG registers
and waiting for 1 exchange interrupt.
    SGPIO_REG(SGPIO_SLICE_L)    = 0X563BC99D;
    SGPIO_REG(SGPIO_SLICE_A)    = 0XF2667F8B;
    SGPIO_REG(SGPIO_SLICE_I)    = 0XDF79B21F;
    SGPIO_REG(SGPIO_SLICE_E)    = 0X7EE2F6C3;
    SGPIO_REG(SGPIO_SLICE_J)    = 0X9839B195;
    SGPIO_REG(SGPIO_SLICE_C)    = 0XD388ADA8;
    SGPIO_REG(SGPIO_SLICE_K)    = 0X093ED8FF;
    SGPIO_REG(SGPIO_SLICE_F)    = 0X23E34D01;
    SGPIO_REG_SS(SGPIO_SLICE_L) = 0X2CB8C285;
    SGPIO_REG_SS(SGPIO_SLICE_A) = 0XD5541EEE;
    SGPIO_REG_SS(SGPIO_SLICE_I) = 0X0EB9BB83;
    SGPIO_REG_SS(SGPIO_SLICE_E) = 0XD48C6006;
    SGPIO_REG_SS(SGPIO_SLICE_J) = 0XCB18A7F3;
    SGPIO_REG_SS(SGPIO_SLICE_C) = 0XEA750F8A;
    SGPIO_REG_SS(SGPIO_SLICE_K) = 0X76BFC86E;
    SGPIO_REG_SS(SGPIO_SLICE_F) = 0X1317F452;

value in L[0]
POS_RESET  val  reg[byte; 0 = low order]
0          2C   L[3]
1          01   F[0]
2          01   F[0]
3          2C   L[3]
4          B8   SS_L[2]
5          4D   F[1]
6          4D   F[1]
0x1f       01   F[0]
0x2f       01   F[0]

Perhaps measurements at low shift values are unreliable because we're not
sure how many shift events have actually happened before we catch the
interrupt and disable further shifting.



!!! Doesn't seem that shifting is disabled by doing SGPIO_CTRL_ENABLE = 0 !!!
does SGPIO_CTRL_ENABLE enable use of external clock??
