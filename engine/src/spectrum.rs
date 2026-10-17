//! Live spectrum from the PCM the player writes: 56 log-spaced bands, 20 Hz - 20 kHz, 30 frames a second,
//! with bar fall-off and peak hold, as needle's "Live spectrum" draws them (0..255 per band).

use std::sync::Arc;

use realfft::{RealFftPlanner, RealToComplex};

pub const SAMPLE_RATE: f32 = 44_100.0;
pub const BANDS: usize = 56;
pub const FPS: usize = 30;
/// 4096 points: ~10.8 Hz bins, so the sub-bass bands are still distinct bars.
const N: usize = 4096;
const HOP: usize = SAMPLE_RATE as usize / FPS;
const FLOOR_DB: f32 = -72.0;
const CEIL_DB: f32 = -12.0;
/// Music falls off about like pink noise; +3 dB per octave around 1 kHz keeps the highs visible.
const TILT_DB_PER_OCTAVE: f32 = 3.0;
const FALL_PER_FRAME: f32 = 0.045;
const PEAK_HOLD_FRAMES: u32 = 12;
const PEAK_FALL_PER_FRAME: f32 = 0.012;

pub struct Analyzer {
    fft: Arc<dyn RealToComplex<f32>>,
    window: Vec<f32>,
    ring: Vec<f32>,
    write: usize,
    since_frame: usize,
    scratch_in: Vec<f32>,
    scratch_out: Vec<realfft::num_complex::Complex<f32>>,
    edges: Vec<(usize, usize, f32)>,
    bars: [f32; BANDS],
    peaks: [f32; BANDS],
    peak_age: [u32; BANDS],
}

pub struct Frame {
    pub bands: Vec<u8>,
    pub peaks: Vec<u8>,
}

impl Default for Analyzer {
    fn default() -> Self {
        Self::new()
    }
}

impl Analyzer {
    pub fn new() -> Self {
        let fft = RealFftPlanner::<f32>::new().plan_fft_forward(N);
        let window = (0..N).map(|i| 0.5 - 0.5 * (2.0 * std::f32::consts::PI * i as f32 / N as f32).cos()).collect();
        let scratch_out = fft.make_output_vec();
        Self {
            fft,
            window,
            ring: vec![0.0; N],
            write: 0,
            since_frame: 0,
            scratch_in: vec![0.0; N],
            scratch_out,
            edges: band_edges(),
            bars: [0.0; BANDS],
            peaks: [0.0; BANDS],
            peak_age: [0; BANDS],
        }
    }

    /// Feed mono samples; returns the frames that became due (one per 1/30 s of audio), each with the number
    /// of samples of this chunk that come after it (so the caller can time it to when the speaker plays it).
    pub fn push_timed(&mut self, samples: &[f32]) -> Vec<(usize, Frame)> {
        let mut out = Vec::new();
        for (i, &s) in samples.iter().enumerate() {
            self.ring[self.write] = s;
            self.write = (self.write + 1) % N;
            self.since_frame += 1;
            if self.since_frame >= HOP {
                self.since_frame = 0;
                out.push((samples.len() - 1 - i, self.frame()));
            }
        }
        out
    }

    #[cfg(test)]
    pub fn push(&mut self, samples: &[f32]) -> Vec<Frame> {
        self.push_timed(samples).into_iter().map(|(_, f)| f).collect()
    }

    /// Decay to silence (paused or stopped): call at the frame rate while no audio arrives.
    pub fn idle(&mut self) -> Frame {
        self.step(&[0.0; BANDS])
    }

    fn frame(&mut self) -> Frame {
        for i in 0..N {
            self.scratch_in[i] = self.ring[(self.write + i) % N] * self.window[i];
        }
        self.fft.process(&mut self.scratch_in, &mut self.scratch_out).expect("fft buffers are sized by the plan");
        // A full-scale sine through a Hann window peaks at N/4: scale so it reads 0 dB.
        let norm = 4.0 / N as f32;
        let mut level = [0.0f32; BANDS];
        for (b, &(lo, hi, centre)) in self.edges.iter().enumerate() {
            let mag = (lo..=hi).map(|k| self.scratch_out[k].norm()).fold(0.0f32, f32::max) * norm;
            let db = 20.0 * (mag + 1e-9).log10() + TILT_DB_PER_OCTAVE * (centre / 1000.0).log2();
            level[b] = ((db - FLOOR_DB) / (CEIL_DB - FLOOR_DB)).clamp(0.0, 1.0);
        }
        self.step(&level)
    }

    fn step(&mut self, level: &[f32; BANDS]) -> Frame {
        for (b, &lv) in level.iter().enumerate() {
            self.bars[b] = if lv >= self.bars[b] { lv } else { (self.bars[b] - FALL_PER_FRAME).max(lv) };
            if self.bars[b] >= self.peaks[b] {
                self.peaks[b] = self.bars[b];
                self.peak_age[b] = 0;
            } else if self.peak_age[b] < PEAK_HOLD_FRAMES {
                self.peak_age[b] += 1;
            } else {
                self.peaks[b] = (self.peaks[b] - PEAK_FALL_PER_FRAME).max(self.bars[b]);
            }
        }
        let q = |v: &f32| (v * 255.0).round() as u8;
        Frame { bands: self.bars.iter().map(q).collect(), peaks: self.peaks.iter().map(q).collect() }
    }
}

/// FFT bin ranges (inclusive) and centre frequency per band, log-spaced from 20 Hz to 20 kHz. A band narrower
/// than a bin takes the bin nearest its centre.
fn band_edges() -> Vec<(usize, usize, f32)> {
    let bin_hz = SAMPLE_RATE / N as f32;
    let f = |i: usize| 20.0 * 1000f32.powf(i as f32 / BANDS as f32);
    (0..BANDS)
        .map(|i| {
            let (lo_hz, hi_hz) = (f(i), f(i + 1));
            let centre = (lo_hz * hi_hz).sqrt();
            let lo = (lo_hz / bin_hz).ceil() as usize;
            let hi = ((hi_hz / bin_hz).floor() as usize).min(N / 2);
            if lo > hi {
                let k = ((centre / bin_hz).round() as usize).clamp(1, N / 2);
                (k, k, centre)
            } else {
                (lo, hi, centre)
            }
        })
        .collect()
}

#[cfg(test)]
mod tests {
    use super::*;

    fn sine(hz: f32, amp: f32, n: usize) -> Vec<f32> {
        (0..n).map(|i| amp * (2.0 * std::f32::consts::PI * hz * i as f32 / SAMPLE_RATE).sin()).collect()
    }

    fn band_of(hz: f32) -> usize {
        band_edges().iter().position(|&(lo, hi, _)| {
            let bin = SAMPLE_RATE / N as f32;
            (lo as f32 - 0.5) * bin <= hz && hz <= (hi as f32 + 0.5) * bin
        }).unwrap()
    }

    #[test]
    fn one_second_is_thirty_frames() {
        let mut a = Analyzer::new();
        assert_eq!(a.push(&vec![0.0; SAMPLE_RATE as usize]).len(), FPS);
    }

    #[test]
    fn a_tone_lights_its_band_and_not_the_far_ones() {
        let mut a = Analyzer::new();
        let frames = a.push(&sine(1000.0, 0.5, SAMPLE_RATE as usize));
        let last = frames.last().unwrap();
        let b = band_of(1000.0);
        let top = (0..BANDS).max_by_key(|&i| last.bands[i]).unwrap();
        assert!(top.abs_diff(b) <= 1, "loudest band {top}, tone in band {b}");
        assert!(last.bands[b] > 200);
        assert!(last.bands[band_of(60.0)] < 40 && last.bands[band_of(12_000.0)] < 40);
    }

    #[test]
    fn silence_reads_zero_and_bars_fall_but_peaks_hold() {
        let mut a = Analyzer::new();
        assert!(a.push(&vec![0.0; 8192]).iter().all(|f| f.bands.iter().all(|&v| v == 0)));
        a.push(&sine(100.0, 0.8, 8192));
        let b = band_of(100.0);
        let lit = a.idle();
        let after = a.idle();
        assert!(after.bands[b] < lit.bands[b], "bars fall off");
        assert_eq!(after.peaks[b], lit.peaks[b], "peaks hold");
        let mut f = a.idle();
        for _ in 0..200 {
            f = a.idle();
        }
        assert!(f.bands.iter().chain(f.peaks.iter()).all(|&v| v == 0), "everything settles to silence");
    }

    #[test]
    fn bands_cover_twenty_hertz_to_twenty_kilohertz_in_order() {
        let e = band_edges();
        assert_eq!(e.len(), BANDS);
        assert!(e.windows(2).all(|w| w[0].2 < w[1].2));
        assert!(e[0].2 > 20.0 && e[BANDS - 1].2 < 20_000.0);
    }
}
