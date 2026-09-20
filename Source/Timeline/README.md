Phase 3-4: TimelineState and PlaybackState live here. PlaybackState
tracks the host transport (playing/stopped, position, BPM, PPQ, time
signature) read-only from the visual layer's perspective -- the visual
layer reads this state but never calculates it.
