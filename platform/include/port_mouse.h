#pragma once

#include <cmath>
#include <cstdint>

namespace PortMouse {
constexpr float kMaxPitch = 1.52f; // approximately 87 degrees, short of the pole
constexpr float kPi = 3.14159265358979323846f;

struct Planar {
  float right;
  float forward;
};
inline Planar ClampPlanar(Planar value, float maximum) {
  const float length = std::hypot(value.right, value.forward);
  if (!std::isfinite(length) || maximum <= 0.f) return {0.f, 0.f};
  if (length > maximum) {
    const float scale = maximum / length;
    value.right *= scale;
    value.forward *= scale;
  }
  return value;
}

// The retail forward-force law applied to either horizontal axis. Damping and
// collision integration remain in the player; this is not teleport movement.
inline float AxisForce(float input, float velocity, float maxSpeed, float friction,
                       float mass, float dt, float acceleration) {
  if (input == 0.f || maxSpeed <= 0.f || dt <= 0.f || acceleration <= 0.f) return 0.f;
  const float frictionSpeed = friction * mass * maxSpeed / (dt * acceleration);
  const float desired = input * (maxSpeed - frictionSpeed) + (input > 0.f ? frictionSpeed : -frictionSpeed);
  const float fraction = (desired - velocity) / maxSpeed;
  return (fraction < -1.f ? -1.f : fraction > 1.f ? 1.f : fraction) * acceleration;
}

struct AimState {
  float yaw = 0.f;
  float pitch = 0.f;
  bool initialized = false;

  void Reset() { initialized = false; }

  void Synchronize(float x, float y, float z) {
    const float length = std::sqrt(x * x + y * y + z * z);
    if (!std::isfinite(length) || length < 0.00001f) {
      Reset();
      return;
    }
    // A target directly overhead has no horizontal heading. Retain the last
    // heading rather than snapping to atan2(0, 0) when that lock is released.
    if (x * x + y * y > length * length * 0.00000001f) yaw = std::atan2(-x, y);
    const float up = z / length;
    pitch = ClampPitch(std::asin(up < -1.f ? -1.f : up > 1.f ? 1.f : up));
    initialized = true;
  }

  // A locked camera owns orientation. Track its effective direction, but never
  // consume hidden mouse movement or rotate the player's body underneath it.
  bool Update(bool active, bool locked, float x, float y, float z,
              float dx, float dy, float sensitivity, bool invertX, bool invertY) {
    if (!active) {
      Reset();
      return false;
    }
    if (!initialized || locked) Synchronize(x, y, z);
    if (!initialized || locked) return false;
    if (!std::isfinite(dx) || !std::isfinite(dy) ||
        !std::isfinite(sensitivity) || sensitivity <= 0.f) return true;
    // SDL motion is right/down positive; world +pitch looks upward.
    const double nextYaw = yaw + double(dx) * sensitivity * (invertX ? 1.0 : -1.0);
    const double nextPitch = pitch + double(dy) * sensitivity * (invertY ? 1.0 : -1.0);
    yaw = static_cast<float>(std::remainder(nextYaw, 2.0 * kPi));
    pitch = ClampPitch(nextPitch);
    return true;
  }

private:
  static float ClampPitch(double angle) {
    return static_cast<float>(angle < -kMaxPitch ? -kMaxPitch : angle > kMaxPitch ? kMaxPitch : angle);
  }
};

class ButtonGate {
public:
  void Reset() { mReady = false; }
  uint32_t Poll(bool enabled, uint32_t held) {
    if (!enabled) {
      Reset();
      return 0;
    }
    // A UI click or a button held while acquiring capture is not a new shot.
    // Once neutral has been observed, preserve held states for charge/release.
    if (!mReady) {
      mReady = held == 0;
      return 0;
    }
    return held;
  }
private:
  bool mReady = false;
};
} // namespace PortMouse
