#pragma once

#include "config.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <vector>

constexpr double SHAKE_THRESHOLD = 1.5;
constexpr double SHAKE_MIN_DIAGONAL = 100.0;
constexpr int SHAKE_TIMEOUT_MS = 800;
constexpr int HISTORY_SIZE = (1000 / FRECUENCIA_MS);

struct Vector2D {
  double x = 0.0, y = 0.0;
  double distance(const Vector2D &other) const {
    return std::sqrt(std::pow(x - other.x, 2) + std::pow(y - other.y, 2));
  }
};

class ShakeDetector {
public:
  ShakeDetector();
  bool update(const Vector2D &pos);

private:
  std::vector<Vector2D> samples;
  std::vector<double> samples_distance;
  int samples_index = 0;
  bool is_shaking = false;
  std::chrono::steady_clock::time_point shake_end_time;
};
