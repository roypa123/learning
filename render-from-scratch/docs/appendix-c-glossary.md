# Appendix C — Glossary

[← Contents](README.md)

| Term | Meaning | Chapter |
|------|---------|---------|
| **AABB** | Axis-aligned bounding box | 23 |
| **ACES** | Academy Color Encoding System; the filmic tone curve we use | 34 |
| **Albedo** | Fraction of light a surface reflects, per color | 17 |
| **Aliasing** | Jagged or shimmering artifacts from sampling at too few points | 9 |
| **Alpha** | Opacity: 0 transparent, 1 opaque | 9 |
| **Anti-aliasing** | Estimating pixel coverage with several samples or distances | 9, 16 |
| **AOV** | Arbitrary output variable: extra images (albedo, normals) from a render | 36 |
| **Aperture** | The lens opening; bigger = more depth-of-field blur | 20 |
| **Barycentric coordinates** | Weights of a triangle's three corners for a point inside it | 10 |
| **Bias** | Systematic error that more samples don't remove | 31 |
| **Bloom** | Glow around bright areas | 35 |
| **BRDF** | Function describing how a surface reflects light between two directions | 29, 32 |
| **BVH** | Bounding volume hierarchy: a tree of boxes to speed up ray tests | 23 |
| **Caustic** | Light focused by glass or water into bright patterns | 19, 31 |
| **CDF** | Cumulative distribution function | 30 |
| **Chromatic aberration** | Colored fringes from a lens bending colors differently | 35 |
| **Clamp** | Limit a value to a range | 4 |
| **Cornell box** | Classic test scene: a room with red/green walls and a ceiling light | 26 |
| **CRC-32** | Checksum used by PNG chunks | 6 |
| **DEFLATE** | The compression used by PNG, ZIP, gzip: LZ77 + Huffman | 7 |
| **Delta tracking** | Exact sampling of varying-density volumes with null collisions | 28 |
| **Depth of field** | Blur for objects outside the focus distance | 20 |
| **Dielectric** | Non-conducting material: glass, water, plastic | 19 |
| **Dithering** | Adding a pattern before quantizing, to hide banding | 39 |
| **Emission** | Light produced by a surface | 26 |
| **Exposure** | Overall brightness multiplier (in stops: ×2 per stop) | 34 |
| **F0** | Reflectance of a surface seen head-on | 32 |
| **fBm** | Fractal Brownian motion: noise summed over octaves | 11 |
| **Firefly** | Isolated extremely bright pixel from a rare light path | 31 |
| **Fresnel** | Reflectance increasing toward grazing angles | 19, 32 |
| **Gamma / sRGB** | The non-linear encoding of image files and screens | 4 |
| **GGX** | The microfacet distribution used by modern PBR materials | 32 |
| **Global illumination** | Light bouncing between surfaces | 17, 26 |
| **HDR** | High dynamic range: light values beyond 0..1 | 4, 34 |
| **Huffman code** | Variable-length prefix code: frequent symbols get fewer bits | 7 |
| **Importance sampling** | Sampling where the integrand is large, and weighting by 1/pdf | 30 |
| **Instance** | A transformed reference to shared geometry | 27 |
| **IOR** | Index of refraction (η) | 19 |
| **Lambertian** | Ideal matte surface | 17 |
| **Lerp** | Linear interpolation `a + (b−a)t` | 5 |
| **Linear light** | Values proportional to physical light | 4 |
| **LZ77 / LZW** | Dictionary compression methods (PNG / GIF) | 7, 39 |
| **Mesh** | Triangles sharing vertices | 25 |
| **Microfacet** | Model of a rough surface as tiny mirrors | 32 |
| **MIS** | Multiple importance sampling: combining sampling strategies | 31 |
| **Monte Carlo** | Estimating integrals by averaging random samples | 29 |
| **Normal** | Unit vector perpendicular to a surface | 15 |
| **OBJ** | Simple text 3D model format | 25 |
| **ONB** | Orthonormal basis: three perpendicular unit axes | 30 |
| **Path tracing** | Ray tracing with random bounces, averaged | 17 |
| **PBR** | Physically based rendering | 29–33 |
| **PDF** | Probability density function | 30 |
| **Perlin noise** | Gradient noise invented by Ken Perlin | 11 |
| **Phase function** | Direction distribution of scattering in a volume | 28 |
| **Radiance** | Light traveling along a ray; what a pixel measures | 29 |
| **Rasterization** | Drawing by finding the pixels each shape covers | 0, 10 |
| **Ray** | Half-line `origin + t·direction` | 13 |
| **Rendering equation** | Kajiya's equation of light transport | 29 |
| **Russian roulette** | Randomly ending weak paths without bias | 31 |
| **SDF** | Signed distance function | 37 |
| **Solid angle** | Size of a set of directions, in steradians | 30 |
| **Sphere tracing** | Marching a ray by the SDF distance | 37 |
| **Stratified sampling** | One random sample per grid cell | 16, 29 |
| **Supersampling** | Several samples per pixel | 10, 16 |
| **Texture / UV** | Surface color lookup; 2D surface coordinates | 24 |
| **Throughput** | Fraction of light carried along a path so far | 17 |
| **Tone mapping** | Compressing HDR values to displayable 0..1 | 34 |
| **Variance** | Spread of sample values; seen as noise | 29 |
| **Vignette** | Darkening toward the image corners | 35 |
| **VNDF** | Distribution of visible normals, used to sample GGX | 32 |
