# Appendix D — Math cheat sheet

[← Contents](README.md)

## Vectors

```
a + b = (ax+bx, ay+by, az+bz)          t·a = (t·ax, t·ay, t·az)
|a| = sqrt(ax² + ay² + az²)            unit(a) = a / |a|
dot(a, b) = ax·bx + ay·by + az·bz = |a||b| cos θ
cross(a, b) = (ay·bz − az·by, az·bx − ax·bz, ax·by − ay·bx)     ⟂ to both, |cross| = parallelogram area
reflect(v, n) = v − 2·dot(v, n)·n
lerp(a, b, t) = a + (b − a)·t
```

## Rays and intersections

```
ray:      P(t) = O + t·D
sphere:   |P − C|² = r²  →  a = D·D, h = D·(C−O), c = |C−O|² − r²,  t = (h ± sqrt(h² − a·c)) / a
plane:    n·P = k        →  t = (k − n·O) / (n·D)
triangle: Möller–Trumbore (chapter 25)
box:      per axis t0 = (min − O)/D, t1 = (max − O)/D; overlap of [t0, t1] intervals
```

## Camera

```
h = tan(vfov/2);   viewport_height = 2·h·focus_dist;   viewport_width = viewport_height · W/H
w = unit(lookfrom − lookat);   u = unit(cross(vup, w));   v = cross(w, u)
defocus_radius = focus_dist · tan(defocus_angle / 2)
```

## Color

```
sRGB encode: x ≤ 0.0031308 ? 12.92x : 1.055·x^(1/2.4) − 0.055
sRGB decode: x ≤ 0.04045   ? x/12.92 : ((x + 0.055)/1.055)^2.4
luminance  = 0.2126 R + 0.7152 G + 0.0722 B
over       = D·(1 − α) + C·α
ACES fit   = x(2.51x + 0.03) / (x(2.43x + 0.59) + 0.14)
exposure   = value · 2^stops
```

## Optics

```
Snell:     η sin θ = η′ sin θ′
refract:   R⊥ = (η/η′)(R + cos θ·n),  R∥ = −sqrt(1 − |R⊥|²)·n
TIR when:  (η/η′) sin θ > 1
Schlick:   F = F0 + (1 − F0)(1 − cos θ)⁵,  F0 = ((1 − η)/(1 + η))²
```

## Monte Carlo

```
∫ f  ≈  (1/N) Σ f(xᵢ)/p(xᵢ)          error ∝ 1/√N
inverse transform: solve CDF(x) = ξ
cosine hemisphere: φ = 2πr₁, x = cos φ √r₂, y = sin φ √r₂, z = √(1 − r₂),  p = cos θ / π
uniform sphere p = 1/(4π); hemisphere p = 1/(2π)
area light → solid angle: p = distance² / (cos θ_light · A)
sphere cone: cos θ_max = √(1 − R²/d²), p = 1 / (2π(1 − cos θ_max))
mixture: p = 0.5·p₁ + 0.5·p₂
Russian roulette: survive with q, divide by q
free flight in a medium: d = −ln(1 − ξ)/σ
```

## Microfacets (GGX)

```
α = roughness²
D(m)  = α² / (π((n·m)²(α² − 1) + 1)²)
Λ(v)  = (−1 + √(1 + α² tan²θ_v)) / 2
G1    = 1/(1 + Λ(v)),   G2 = 1/(1 + Λ(l) + Λ(v))
VNDF-sampled weight = F · G2 / G1
```

## Noise

```
smoothstep(t) = t²(3 − 2t)
fBm = Σ amplitudeᵢ · noise(frequencyᵢ · p),  frequency ×2, amplitude ×0.5 per octave
marble = 0.5(1 + sin(s·z + 10·turbulence(p)))
```

## SDFs

```
sphere: |p| − r              box: |max(q,0)| + min(maxcomp(q),0), q = |p| − b
union: min(a,b)   intersection: max(a,b)   subtraction: max(a, −b)
smooth union: h = clamp(0.5 + 0.5(b − a)/k); lerp(b, a, h) − k·h(1 − h)
repeat: p − c·round(p/c)
normal ≈ normalize(∇sdf) by central differences
```
