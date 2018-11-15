/*
 * Copyright 2012 Jared Boone
 * Copyright 2013 Benjamin Vernoux
 * Copyright 2018 John Brzustowski
 *
 * This file is part of HackRF.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; see the file COPYING.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street,
 * Boston, MA 02110-1301, USA.
 */

#include <stddef.h>

#include <libopencm3/lpc43xx/m4/nvic.h>

#include <streaming.h>
#include <math.h>
#include "hackrf_core.h"
#include "tuning.h"
#include "rf_path.h"

#include <rom_iap.h>

/* macro to wrap delay function, allowing parameter
   to be in microseconds, and doing floating point math at
   compile time where possible
   delay(n) consumes ~ n * 7 cycles at 48 MHz, so n * 1.458333E-7 sec */

#define delay_us(us) delay(us * 1.0E-6 / 1.458333e-07 + 0.5)

const uint_fast8_t slice_indices[] = {
  SGPIO_SLICE_A,
  SGPIO_SLICE_I,
  SGPIO_SLICE_E,
  SGPIO_SLICE_J,
  SGPIO_SLICE_C,
  SGPIO_SLICE_K,
  SGPIO_SLICE_F,
  SGPIO_SLICE_L,
};

/* set signal level;
   value should be from 0 ... 128
   A bit tricky because we have to do this synchronously with
   the SGPIO shift / swap cycle
*/

void set_sig_level(
                   uint8_t mag
                   ) {

  uint32_t val = (mag << 24) | (mag << 8);

  // wait for the start of an interrupt, not just one already
  // in progress (hence two loops)

  while((SGPIO_STATUS_1 & 1) == 0);
  SGPIO_CLR_STATUS_1 = 0xffff;
  while((SGPIO_STATUS_1 & 1) == 0);
  SGPIO_CLR_STATUS_1 = 0xffff;
  SGPIO_REG_SS(SGPIO_SLICE_L) = val;
  SGPIO_REG_SS(SGPIO_SLICE_F) = val;
  SGPIO_REG_SS(SGPIO_SLICE_K) = val;
  SGPIO_REG_SS(SGPIO_SLICE_C) = val;
  SGPIO_REG_SS(SGPIO_SLICE_J) = val;
  SGPIO_REG_SS(SGPIO_SLICE_E) = val;
  SGPIO_REG_SS(SGPIO_SLICE_I) = val;
  SGPIO_REG_SS(SGPIO_SLICE_A) = val;

  // REG_SS values will have just been swapped into REG
  // at next interrupt; wait for it and stuff
  // REG_SS again so that all 16 regs have what we want.
  while((SGPIO_STATUS_1 & 1) == 0);
  SGPIO_CLR_STATUS_1 = 0xffff;
  SGPIO_REG_SS(SGPIO_SLICE_L) = val;
  SGPIO_REG_SS(SGPIO_SLICE_F) = val;
  SGPIO_REG_SS(SGPIO_SLICE_K) = val;
  SGPIO_REG_SS(SGPIO_SLICE_C) = val;
  SGPIO_REG_SS(SGPIO_SLICE_J) = val;
  SGPIO_REG_SS(SGPIO_SLICE_E) = val;
  SGPIO_REG_SS(SGPIO_SLICE_I) = val;
  SGPIO_REG_SS(SGPIO_SLICE_A) = val;

};

void send_lotek_tagid_1(uint_fast8_t sig)
{
  /* p1 */
  delay_us(1000);
  set_sig_level(sig);
  delay_us(2500);
  set_sig_level(0x00);
  /* g1 */
  delay_us(19500);
  /* p2 */
  set_sig_level(sig);
  delay_us(2500);
  set_sig_level(0x00);
  /* g2 */
  delay_us(17000);
  /* p3 */
  set_sig_level(sig);
  delay_us(2500);
  set_sig_level(0x00);
  /* g3 */
  delay_us(22000);
  /* p4 */
  set_sig_level(sig);
  delay_us(2500);
  set_sig_level(0x00);
}

/* FSK: freq=0; freq+50kHz = 1; 25kbps preamble: 0xAAAAD391 MSB first.
   To impose 50 kHz waveform for 1/25000 second, we need to add two cycles
   and at 8 MSPS, that is 320 samples, so 1 cycle over 160 samples.
   Note:  buf[] is uint32 formatted as Q1I1Q0I0, so two samples, while
   bp points to individual int8 samples.
   In practice, we found 159 works a bit better than 160.

   the FSK frequency is set by WAVEFORM_M: a cycle lasts that many samples @ 8 MSPS
   if WAVEFORM_M is odd, we generate two waveforms so that going back to the first
   buffer slot is correct (there are two samples per 32-bit int)
*/

#define WAVEFORM_M 160
#define WAVEFORM_N 2
int32_t  __attribute__ ((aligned (4))) trigbuf[WAVEFORM_M * 2]; // cos(),sin() * 2^24
uint32_t __attribute__ ((aligned (4))) buf    [(WAVEFORM_N * WAVEFORM_M + 1)/ 2]; // waveform as I/Q

// we generate one or two waveforms, depending on whether WAVEFORM_M is
// odd or even.
void send_ctt_tag(uint64_t freq, uint_fast8_t sig, uint32_t id) {
  int8_t *bp = (int8_t *) & buf[0];

  for(int i = 0; i < WAVEFORM_M * WAVEFORM_N; ++i) {
    *bp++ = sig * trigbuf[(2*i    ) % (2 * WAVEFORM_M)] >> 23;
    *bp++ = sig * trigbuf[(2*i + 1) % (2 * WAVEFORM_M)] >> 23;
  }

  // now that we've spent a few milliseconds doing trig, turn
  // on the transmitter

  set_sig_level(0);
  set_freq(freq - 25000);
  delay_us(1000); // for freq stability, carrier

  uint32_t code, preamble = 0xAAAAD391; // pre-amble
  uint_fast8_t i = 0;
  int_fast8_t j = 0;
  uint32_t l = 0;
  // send 2 zero bits (code = 0; count is 30 out of 32)
  j = 30;
  code = 0;
  l = 0;
  for(i=0; i < 4; ++i) {
    for (; j < 32; ++j) {
      // each bit is 1/25000 sec i.e. 320 samples at 8 MSPS
      // we send these either with constant I/Q (carrier freq)
      // or with I/Q rotating twice
      uint_fast8_t incr = (code & 0x80000000) ? 1 : 0;
      for (uint_fast8_t k = 0; k < 20; ++k) {
        while((SGPIO_STATUS_1 & 1) == 0x00);
        SGPIO_CLR_STATUS_1 = 0xffff;
        if (incr == 1) {
          SGPIO_REG_SS(SGPIO_SLICE_L) = buf[l++];
          SGPIO_REG_SS(SGPIO_SLICE_F) = buf[l++];
          SGPIO_REG_SS(SGPIO_SLICE_K) = buf[l++];
          SGPIO_REG_SS(SGPIO_SLICE_C) = buf[l++];
          SGPIO_REG_SS(SGPIO_SLICE_J) = buf[l++];
          SGPIO_REG_SS(SGPIO_SLICE_E) = buf[l++];
          SGPIO_REG_SS(SGPIO_SLICE_I) = buf[l++];
          SGPIO_REG_SS(SGPIO_SLICE_A) = buf[l++];
          if (l >= WAVEFORM_M)
            l -= WAVEFORM_M;
        } else {
          uint32_t hold = (buf[l] & 0xFFFF) | (buf[l] << 16);
          SGPIO_REG_SS(SGPIO_SLICE_L) = hold;
          SGPIO_REG_SS(SGPIO_SLICE_F) = hold;
          SGPIO_REG_SS(SGPIO_SLICE_K) = hold;
          SGPIO_REG_SS(SGPIO_SLICE_C) = hold;
          SGPIO_REG_SS(SGPIO_SLICE_J) = hold;
          SGPIO_REG_SS(SGPIO_SLICE_E) = hold;
          SGPIO_REG_SS(SGPIO_SLICE_I) = hold;
          SGPIO_REG_SS(SGPIO_SLICE_A) = hold;
        }
      }
      code = code << 1;
    }
    switch (i) {
    case 0:
      code = preamble; // send 32-bit preamble
      j = 0;
      break;
    case 1:
      code = id; // send 32-bit id;
      j = 0;
      break;
    case 2:
      code = 0;
      j = 30; // send two more zero bits
      break;
    default:
      break;
    }
  };
  set_sig_level(0);
  set_freq(0);
};

// set pattern of 3 LEDS to lowest 3 bits in `onoff`
void leds_set (uint8_t onoff) {
  if (onoff & 4) {
    led_on (LED1);
  } else {
    led_off (LED1);
  }
  if (onoff & 2) {
    led_on (LED2);
  } else {
    led_off (LED2);
  }
  if (onoff & 1) {
    led_on (LED3);
  } else {
    led_off (LED3);
  }
};

int main(void) {
  pin_setup();
  enable_1v8_power();
  enable_rf_power();

  /* Let the voltage stabilize */
  delay(1000000);
  cpu_clock_init();

  led_on(LED3);

  sample_rate_set(8000000);
  rf_path_init(&rf_path);
  rf_path_set_lna(&rf_path, 1);
  sgpio_set_slice_mode(&sgpio_config, true);
  rf_path_set_direction(&rf_path, RF_PATH_DIRECTION_TX); // calls sgpio_configure
  SGPIO_SET_EN_1 = 1 << SGPIO_SLICE_A;
  sgpio_cpld_stream_enable(&sgpio_config);
  // use constant gain; amplitude modulation will use DAC samples
  max2837_set_txvga_gain(&max2837, 25);

  // start with maximum signal strength
  uint8_t sig = 0x80;

  // we will cycle through carrier frequencies for Lotek tags
#define NUM_LOTEK_FREQS 4
  uint64_t lotek_freqs[NUM_LOTEK_FREQS] = {150.1E6, 150.34E6, 151.5E5, 166.38E6};
  int64_t lotek_dfreq = 0;

  for(int i = 0; i < WAVEFORM_M; ++i) {
    trigbuf[2 * i]     = rint((1<<23) * cos(2.0 * M_PI * i / (double) WAVEFORM_M));
    trigbuf[2 * i + 1] = rint((1<<23) * sin(2.0 * M_PI * i / (double) WAVEFORM_M));
  }

  int fcount = 20; // number of consecutive bursts to emit at each frequency, signal strength
  while(1) {
    for (uint_fast8_t i = 0; i < NUM_LOTEK_FREQS; ++i) {
      /* Lotek code 1 @ 166.38 MHz */
      leds_set((i+1));
      set_freq(lotek_freqs[i] + lotek_dfreq);
      set_sig_level(0);
      delay_us(1000); // for stability; guessed value
      send_lotek_tagid_1(sig);
      set_freq(0);
      set_sig_level(0);
      delay_us(125000); // delay so user can see LED display of freq
    }
    /* CTT tag 0x61337f34 @ 434 MHz */
    /* from measurements, FSK is between carrier +/- 25 kHz */
    leds_set(0x7);
    send_ctt_tag(434000000, sig, 0x61337F34);
    delay_us(208950); // enough to take BI to 1.000 s

    if (! --fcount) {
      if (sig >= 15) {
        sig -= 15;
      } else {
        sig = 0x80 + sig - 15;
      }
      if (lotek_dfreq < 4096) {
        lotek_dfreq += 131;
      } else {
        lotek_dfreq -= 8192;
      }
      fcount = 20;
    }
  }
  return 0;
}
