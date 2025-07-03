#include "shakedetector.h"

using namespace std::chrono;

ShakeDetector::ShakeDetector() {
  samples.resize(HISTORY_SIZE);
  samples_distance.resize(HISTORY_SIZE, 0.0);
}

bool ShakeDetector::update(const Vector2D &pos) {
  int previous_index =
      (samples_index == 0) ? HISTORY_SIZE - 1 : samples_index - 1;
  samples[samples_index] = pos;
  samples_distance[samples_index] =
      samples[samples_index].distance(samples[previous_index]);
  samples_index = (samples_index + 1) % HISTORY_SIZE;

  double trail = 0.0;
  for (double distance : samples_distance) {
    trail += distance;
  }

  double left = 1e100, right = -1e100, top = 1e100, bottom = -1e100;
  for (const auto &position : samples) {
    left = std::min(left, position.x);
    right = std::max(right, position.x);
    top = std::min(top, position.y);
    bottom = std::max(bottom, position.y);
  }
  double diagonal = Vector2D{left, top}.distance(Vector2D{right, bottom});

  if (diagonal > SHAKE_MIN_DIAGONAL && (trail / diagonal) > SHAKE_THRESHOLD) {
    is_shaking = true;
    shake_end_time = steady_clock::now() + milliseconds(SHAKE_TIMEOUT_MS);
  } else {
    if (is_shaking && steady_clock::now() > shake_end_time) {
      is_shaking = false;
    }
  }
  return is_shaking;
}
