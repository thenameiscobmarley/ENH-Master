# Footstep detection

> 🔎 **[Searchbar](../../Searchbar.md)** — find any doc, setting, function or GitHub page (Ctrl+F)

The detector **recognises events**; it doesn't just boost a frequency band. A footstep is found by its
shape over time.

## One event, step by step

| When | What happens |
|---|---|
| the start | a sudden change in the sound arms it (steady noise can't) |
| +4 ms | a first guess, so the lift starts with the step, not after it |
| +42 ms | the real decision, once it's heard how the sound dies away |
| up to +160 ms | a wrong first guess is taken back before it does harm |

## The clues

How it starts · how it dies away · how noisy it is · how busy things are around it · whether it
comes in a walking rhythm · how loud it is · how much it sounds like the last one.

Two rules stop most crate lids: only sharp, ringing sounds count as "clutter", and a sound too close
to the last one is only rejected if that one was sharp too (otherwise speech cancelled real steps).

## Results on the test scenes

- 76–100 % of steps found.
- Crate scene: 8 of 8 false lifts before → 3 of 8 now; lifted time 54–61 % → 3–4 %.

The EQ then lifts where *this* step carries its detail and dips what masks it, so every step gets its own EQ.

Watch it: `EnhDspTests --events crates`. Code: `FootstepDetector.h/.cpp`.
