// pixel/canvas.h
// ------------------------------------------------------------
// 2D drawing on an Image: pixels, lines, rectangles, circles,
// triangles, gradients, alpha blending and anti-aliasing.
// Explained in docs/08-drawing-basics.md, docs/09-circles-antialiasing.md,
//              docs/10-triangles-gradients.md
// All colors are LINEAR (use hex_color()/rgb255() to make them).
// ------------------------------------------------------------
#pragma once
#include <cmath>
#include <vector>
#include <algorithm>
#include <utility>
#include "vec3.h"
#include "image.h"

namespace pixel {

struct Canvas {
    Image& img;
    explicit Canvas(Image& target) : img(target) {}

    int width() const { return img.width; }
    int height() const { return img.height; }

    void clear(const Color& c) { img.fill(c); }

    // ---------- single pixels ----------------------------------------------

    void set_pixel(int x, int y, const Color& c) {
        if (img.in_bounds(x, y)) img.at(x, y) = c;
    }

    // Mix a color over the existing pixel. alpha = 0 (invisible) .. 1 (solid)
    void blend_pixel(int x, int y, const Color& c, double alpha) {
        if (!img.in_bounds(x, y) || alpha <= 0.0) return;
        if (alpha >= 1.0) { img.at(x, y) = c; return; }
        Color& dst = img.at(x, y);
        dst = dst * (1.0 - alpha) + c * alpha;
    }

    // Add light to a pixel (for glows and stars).
    void add_pixel(int x, int y, const Color& c) {
        if (img.in_bounds(x, y)) img.at(x, y) += c;
    }

    // ---------- rectangles -------------------------------------------------

    void fill_rect(int x0, int y0, int w, int h, const Color& c, double alpha = 1.0) {
        for (int y = y0; y < y0 + h; y++)
            for (int x = x0; x < x0 + w; x++)
                blend_pixel(x, y, c, alpha);
    }

    void draw_rect(int x0, int y0, int w, int h, const Color& c) {
        draw_line(x0, y0, x0 + w - 1, y0, c);
        draw_line(x0, y0 + h - 1, x0 + w - 1, y0 + h - 1, c);
        draw_line(x0, y0, x0, y0 + h - 1, c);
        draw_line(x0 + w - 1, y0, x0 + w - 1, y0 + h - 1, c);
    }

    // ---------- lines ------------------------------------------------------

    // Bresenham's line algorithm: only integer math, no gaps, no anti-aliasing.
    void draw_line(int x0, int y0, int x1, int y1, const Color& c) {
        int dx = std::abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
        int dy = -std::abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
        int err = dx + dy;
        while (true) {
            set_pixel(x0, y0, c);
            if (x0 == x1 && y0 == y1) break;
            int e2 = 2 * err;
            if (e2 >= dy) { err += dy; x0 += sx; }
            if (e2 <= dx) { err += dx; y0 += sy; }
        }
    }

    // Smooth (anti-aliased) thick line using distance to a line segment.
    void draw_line_aa(double x0, double y0, double x1, double y1, double thickness, const Color& c) {
        double r = thickness * 0.5;
        int minx = (int)std::floor(std::min(x0, x1) - r - 1), maxx = (int)std::ceil(std::max(x0, x1) + r + 1);
        int miny = (int)std::floor(std::min(y0, y1) - r - 1), maxy = (int)std::ceil(std::max(y0, y1) + r + 1);
        minx = std::max(minx, 0); miny = std::max(miny, 0);
        maxx = std::min(maxx, width() - 1); maxy = std::min(maxy, height() - 1);
        double vx = x1 - x0, vy = y1 - y0;
        double len2 = vx * vx + vy * vy;
        for (int y = miny; y <= maxy; y++) {
            for (int x = minx; x <= maxx; x++) {
                double px = x + 0.5 - x0, py = y + 0.5 - y0;
                double t = len2 > 0 ? clamp01((px * vx + py * vy) / len2) : 0.0;
                double dx = px - t * vx, dy = py - t * vy;
                double d = std::sqrt(dx * dx + dy * dy);
                double coverage = clamp01(r - d + 0.5);   // 1 inside, fades over 1 pixel
                blend_pixel(x, y, c, coverage);
            }
        }
    }

    // ---------- circles ----------------------------------------------------

    // Hard-edged filled circle: a pixel is in or out.
    void fill_circle(int cx, int cy, int radius, const Color& c) {
        for (int y = cy - radius; y <= cy + radius; y++)
            for (int x = cx - radius; x <= cx + radius; x++) {
                int dx = x - cx, dy = y - cy;
                if (dx * dx + dy * dy <= radius * radius) set_pixel(x, y, c);
            }
    }

    // Midpoint circle algorithm: outline only, integer math.
    void draw_circle(int cx, int cy, int radius, const Color& c) {
        int x = radius, y = 0, err = 1 - radius;
        while (x >= y) {
            set_pixel(cx + x, cy + y, c); set_pixel(cx + y, cy + x, c);
            set_pixel(cx - y, cy + x, c); set_pixel(cx - x, cy + y, c);
            set_pixel(cx - x, cy - y, c); set_pixel(cx - y, cy - x, c);
            set_pixel(cx + y, cy - x, c); set_pixel(cx + x, cy - y, c);
            y++;
            if (err < 0) err += 2 * y + 1;
            else { x--; err += 2 * (y - x) + 1; }
        }
    }

    // Smooth-edged circle. Edge pixels get partial coverage -> no "staircase".
    void fill_circle_aa(double cx, double cy, double radius, const Color& c, double alpha = 1.0) {
        int minx = std::max(0, (int)std::floor(cx - radius - 1));
        int maxx = std::min(width() - 1, (int)std::ceil(cx + radius + 1));
        int miny = std::max(0, (int)std::floor(cy - radius - 1));
        int maxy = std::min(height() - 1, (int)std::ceil(cy + radius + 1));
        for (int y = miny; y <= maxy; y++)
            for (int x = minx; x <= maxx; x++) {
                double dx = x + 0.5 - cx, dy = y + 0.5 - cy;
                double d = std::sqrt(dx * dx + dy * dy);
                double coverage = clamp01(radius - d + 0.5);
                blend_pixel(x, y, c, coverage * alpha);
            }
    }

    // A soft glowing disc: brightness falls off smoothly from the center.
    void glow(double cx, double cy, double radius, const Color& c) {
        int minx = std::max(0, (int)(cx - radius)), maxx = std::min(width() - 1, (int)(cx + radius));
        int miny = std::max(0, (int)(cy - radius)), maxy = std::min(height() - 1, (int)(cy + radius));
        for (int y = miny; y <= maxy; y++)
            for (int x = minx; x <= maxx; x++) {
                double dx = x + 0.5 - cx, dy = y + 0.5 - cy;
                double d = std::sqrt(dx * dx + dy * dy) / radius;
                if (d < 1.0) {
                    double k = (1.0 - d) * (1.0 - d);
                    add_pixel(x, y, c * k);
                }
            }
    }

    // ---------- triangles --------------------------------------------------

    // "Edge function": tells on which side of the line a->b the point p lies.
    // It is also twice the signed area of triangle (a, b, p).
    static double edge(double ax, double ay, double bx, double by, double px, double py) {
        return (bx - ax) * (py - ay) - (by - ay) * (px - ax);
    }

    // Filled triangle with a color at each corner, smoothly blended
    // using barycentric coordinates. ss = samples per pixel side (anti-aliasing).
    void fill_triangle(double x0, double y0, double x1, double y1, double x2, double y2,
                       const Color& c0, const Color& c1, const Color& c2, int ss = 4) {
        double area = edge(x0, y0, x1, y1, x2, y2);
        if (std::fabs(area) < 1e-12) return;   // degenerate (flat) triangle
        int minx = std::max(0, (int)std::floor(std::min({x0, x1, x2})));
        int maxx = std::min(width() - 1, (int)std::ceil(std::max({x0, x1, x2})));
        int miny = std::max(0, (int)std::floor(std::min({y0, y1, y2})));
        int maxy = std::min(height() - 1, (int)std::ceil(std::max({y0, y1, y2})));

        for (int y = miny; y <= maxy; y++) {
            for (int x = minx; x <= maxx; x++) {
                int inside = 0;
                Color sum(0, 0, 0);
                for (int sy = 0; sy < ss; sy++) {
                    for (int sx = 0; sx < ss; sx++) {
                        double px = x + (sx + 0.5) / ss;
                        double py = y + (sy + 0.5) / ss;
                        double w0 = edge(x1, y1, x2, y2, px, py) / area;
                        double w1 = edge(x2, y2, x0, y0, px, py) / area;
                        double w2 = edge(x0, y0, x1, y1, px, py) / area;
                        if (w0 >= 0 && w1 >= 0 && w2 >= 0) {
                            inside++;
                            sum += w0 * c0 + w1 * c1 + w2 * c2;
                        }
                    }
                }
                if (inside > 0) blend_pixel(x, y, sum / inside, (double)inside / (ss * ss));
            }
        }
    }

    void fill_triangle(double x0, double y0, double x1, double y1, double x2, double y2,
                       const Color& c, int ss = 4) {
        fill_triangle(x0, y0, x1, y1, x2, y2, c, c, c, ss);
    }

    // Filled polygon (any shape, given as a list of points) using the
    // even-odd scanline rule. Points are (x,y) pairs.
    void fill_polygon(const std::vector<std::pair<double, double>>& pts, const Color& c, int ss = 4) {
        if (pts.size() < 3) return;
        double miny = pts[0].second, maxy = pts[0].second;
        for (auto& p : pts) { miny = std::min(miny, p.second); maxy = std::max(maxy, p.second); }
        int y0 = std::max(0, (int)std::floor(miny)), y1 = std::min(height() - 1, (int)std::ceil(maxy));
        std::vector<double> coverage(width());
        for (int y = y0; y <= y1; y++) {
            std::fill(coverage.begin(), coverage.end(), 0.0);
            for (int s = 0; s < ss; s++) {
                double sy = y + (s + 0.5) / ss;
                std::vector<double> xs;
                for (size_t i = 0; i < pts.size(); i++) {
                    auto a = pts[i], b = pts[(i + 1) % pts.size()];
                    if ((a.second <= sy && b.second > sy) || (b.second <= sy && a.second > sy)) {
                        double t = (sy - a.second) / (b.second - a.second);
                        xs.push_back(a.first + t * (b.first - a.first));
                    }
                }
                std::sort(xs.begin(), xs.end());
                for (size_t i = 0; i + 1 < xs.size(); i += 2) {
                    // add horizontal coverage of span [xs[i], xs[i+1]]
                    double xa = xs[i], xb = xs[i + 1];
                    int ia = std::max(0, (int)std::floor(xa)), ib = std::min(width() - 1, (int)std::floor(xb));
                    for (int x = ia; x <= ib; x++) {
                        double left = std::max(xa, (double)x), right = std::min(xb, (double)x + 1);
                        if (right > left) coverage[x] += (right - left) / ss;
                    }
                }
            }
            for (int x = 0; x < width(); x++)
                if (coverage[x] > 0) blend_pixel(x, y, c, std::min(1.0, coverage[x]));
        }
    }

    // ---------- gradients --------------------------------------------------

    // Vertical gradient from 'top' color to 'bottom' color over rows [y0, y1).
    void vertical_gradient(int y0, int y1, const Color& top, const Color& bottom) {
        for (int y = std::max(0, y0); y < std::min(height(), y1); y++) {
            double t = (y1 - y0) > 1 ? (double)(y - y0) / (y1 - y0 - 1) : 0.0;
            Color c = lerp(top, bottom, t);
            for (int x = 0; x < width(); x++) img.at(x, y) = c;
        }
    }

    // Radial gradient: 'inner' at the center fading to 'outer' at 'radius'.
    void radial_gradient(double cx, double cy, double radius, const Color& inner, const Color& outer) {
        for (int y = 0; y < height(); y++)
            for (int x = 0; x < width(); x++) {
                double dx = x + 0.5 - cx, dy = y + 0.5 - cy;
                double t = clamp01(std::sqrt(dx * dx + dy * dy) / radius);
                img.at(x, y) = lerp(inner, outer, t);
            }
    }
};

} // namespace pixel
