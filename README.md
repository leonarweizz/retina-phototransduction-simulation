# Retina Phototransduction Simulation

Interactive simulation of rod and cone light adaptation based on Hamer et al. (2005) and Adler's Physiology of the Eye (Ch. 19 & 20; 2024) with rodent and primate parameters.

## Live Demo

**[▶ Launch Simulation](https://leonarweizz.github.io/retina-phototransduction-simulation/retina_simulator_primate.html)**

**[▶ Arduino Prototype (Wokwi)](https://wokwi.com/projects/450072916246326273)**

## Overview

Reduces the phototransduction cascade to 5 core equations while preserving:
- Rod half-saturation: ~600 R*/s
- Cone half-saturation: ~100,000 R*/s
- Ca²⁺-mediated Weber-law adaptation

## Key Parameters

| Parameter | Rod | Cone | Function |
|-----------|-----|------|----------|
| τ_R | 80 ms | 4 ms | R* lifetime |
| τ_E | 200 ms | 10 ms | E* lifetime |
| β_sub | 0.1 /s | 2.0 /s | Operating range |
| γ_Ca | 50 /s | 300 /s | Adaptation speed |

## Files

| File | Description |
|------|-------------|
| `retina_simulator_primate.html` | Browser-based interactive simulation |
| `retina_phototransduction.ino` | Arduino source code |

## References

- Hamer RD et al. (2005). *Visual Neuroscience* 22:417–436.
- Lamb TD & Pugh EN (2006). *IOVS* 47:5138–5152.

## License

MIT License

## Citation
```
Leonard Weiß (2026). Retina Phototransduction Simulation.
https://github.com/leonarweizz/retina-phototransduction-simulation
```
