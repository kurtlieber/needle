//! The audio sink: PulseAudio (PipeWire on Omarchy) with a short buffer, plus a tap that copies the PCM to the
//! spectrum analyser.
//!
//! librespot's own PulseAudio sink asks for the server's default buffer (about two seconds), so a pause would
//! keep playing for two seconds and the spectrum would run two seconds ahead of the music. This one asks for
//! ~120 ms, flushes on pause, and publishes its measured latency so the spectrum can wait for the speaker.

use std::sync::atomic::{AtomicBool, AtomicU64, Ordering};
use std::sync::mpsc::SyncSender;
use std::sync::Arc;

use libpulse_binding as pulse;
use libpulse_simple_binding::Simple;
use librespot_playback::audio_backend::{Sink, SinkError, SinkResult};
use librespot_playback::convert::Converter;
use librespot_playback::decoder::AudioPacket;
use librespot_playback::{NUM_CHANNELS, SAMPLE_RATE};

const TARGET_LATENCY_MS: u32 = 120;

/// What the sink shares with the rest of the engine.
#[derive(Clone)]
pub struct Tap {
    /// Mono samples for the analyser; dropped (never blocking playback) when the analyser falls behind.
    pub tx: SyncSender<Vec<f32>>,
    /// Only copy samples while needle shows the spectrum.
    pub enabled: Arc<AtomicBool>,
    /// The sink's measured latency in microseconds.
    pub latency_us: Arc<AtomicU64>,
}

pub struct PulseSink {
    simple: Option<Simple>,
    tap: Tap,
}

impl PulseSink {
    pub fn new(tap: Tap) -> Self {
        Self { simple: None, tap }
    }
}

fn err(e: pulse::error::PAErr) -> String {
    e.to_string().unwrap_or_else(|| format!("PulseAudio error {}", e.0))
}

impl Sink for PulseSink {
    fn start(&mut self) -> SinkResult<()> {
        if self.simple.is_none() {
            let spec = pulse::sample::Spec { format: pulse::sample::Format::FLOAT32NE, channels: NUM_CHANNELS, rate: SAMPLE_RATE };
            let bytes_per_ms = SAMPLE_RATE as usize * NUM_CHANNELS as usize * 4 / 1000;
            let attr = pulse::def::BufferAttr {
                maxlength: u32::MAX,
                tlength: (TARGET_LATENCY_MS as usize * bytes_per_ms) as u32,
                prebuf: u32::MAX,
                minreq: u32::MAX,
                fragsize: u32::MAX,
            };
            let s = Simple::new(None, "needle", pulse::stream::Direction::Playback, None, "Review", &spec, None, Some(&attr))
                .map_err(|e| SinkError::ConnectionRefused(err(e)))?;
            self.simple = Some(s);
        }
        Ok(())
    }

    /// Pause, stop, seek: drop what's buffered so the speaker goes quiet now, not in two seconds.
    fn stop(&mut self) -> SinkResult<()> {
        if let Some(s) = self.simple.take() {
            s.flush().map_err(|e| SinkError::StateChange(err(e)))?;
        }
        Ok(())
    }

    fn write(&mut self, packet: AudioPacket, converter: &mut Converter) -> SinkResult<()> {
        let AudioPacket::Samples(samples) = packet else {
            return Err(SinkError::InvalidParams("passthrough audio is not supported".into()));
        };
        if self.tap.enabled.load(Ordering::Relaxed) {
            let mono: Vec<f32> = samples.as_chunks::<2>().0.iter().map(|[l, r]| ((l + r) * 0.5) as f32).collect();
            let _ = self.tap.tx.try_send(mono);
        }
        let s = self.simple.as_ref().ok_or_else(|| SinkError::NotConnected("sink not started".into()))?;
        let f32s = converter.f64_to_f32(&samples);
        let bytes: Vec<u8> = f32s.iter().flat_map(|v| v.to_ne_bytes()).collect();
        s.write(&bytes).map_err(|e| SinkError::OnWrite(err(e)))?;
        if let Ok(l) = s.get_latency() {
            self.tap.latency_us.store(l.0, Ordering::Relaxed);
        }
        Ok(())
    }
}
