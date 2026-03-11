#ifndef PARTICLE_H
#define PARTICLE_H

#include <cstdlib>

/*
 * SwanOS — Simple floating particle for splash/login backgrounds.
 */

struct Particle {
    double x, y, vx, vy, size, life;
    int w, h;

    Particle(int w_, int h_) : w(w_), h(h_) { reset(); }

    void reset() {
        x    = rand() % w;
        y    = rand() % h;
        vx   = (rand() % 600 - 300) / 1000.0;  // -0.3 .. +0.3
        vy   = (rand() % 400 - 500) / 1000.0;   // -0.5 .. -0.1
        size = 1.0 + (rand() % 200) / 100.0;     // 1..3
        life = 0.5 + (rand() % 500) / 1000.0;    // 0.5..1.0
    }

    void update() {
        x += vx;
        y += vy;
        life -= 0.005;
        if (life <= 0.0) {
            reset();
            y = h; // respawn at bottom
        }
    }
};

#endif // PARTICLE_H
