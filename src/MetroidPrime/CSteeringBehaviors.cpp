#include "MetroidPrime/CSteeringBehaviors.hpp"

#include "Kyoto/Math/CMath.hpp"
#include "Kyoto/Math/CloseEnough.hpp"
#include "MetroidPrime/CPhysicsActor.hpp"
#include "MetroidPrime/CStateManager.hpp"

#include <float.h>

// libc/float.h's `FLT_MAX` is `(*(float*)__float_max)`, which makes mwcceppc materialise the
// address in a register and load through it instead of reading the constant in place. Retail
// reads it in place (lbl_8041B984, 0x7f7fffff, in the DOL's shared `.sdata2`), so the constant
// is spelled as a literal. Same finding, same workaround, as
// src/MetroidPrime/ScriptObjects/CScriptTeamAiMgr.cpp:13 and
// src/MetroidPrime/PathFinding/CPathFindArea.cpp:17.
#undef FLT_MAX
#define FLT_MAX 3.402823466e+38f

// The shared polynomial solver at 0x802CB918, inside `Kyoto/Math/RMathUtils.cpp`'s claimed range
// (`config/G2ME01/splits.txt`, 0x802CB330..0x802CDF8C). It takes the coefficients in ASCENDING
// powers of t - it reads `coefficients[4]` and drops to `fn_802CBCA0` with `coefficients[0..3]`
// copied out when that one is zero - returns how many real roots it wrote, and writes at most 4.
// Its own bytes are still unwritten in that unit, so it is only declared here and called; the
// DOL link resolves it out of the retail object.
extern "C" int fn_802CB918(const float* coefficients, float* roots);

CSteeringBehaviors::CSteeringBehaviors() : x0_(M_PIF / 2.f) {}

CVector3f CSteeringBehaviors::Flee(const CPhysicsActor& actor, const CVector3f& position) const {
  const CVector3f delta = actor.GetTranslation() - position;
  if (delta.CanBeNormalized()) {
    return delta.AsNormalized();
  }
  return actor.GetTransform().GetForward();
}

CVector3f CSteeringBehaviors::Seek(const CPhysicsActor& actor,
                                   const CVector3f& destination) const {
  const CVector3f delta = destination - actor.GetTranslation();
  if (delta.CanBeNormalized()) {
    return delta.AsNormalized();
  }
  return CVector3f::Zero();
}

CVector3f CSteeringBehaviors::Arrival(const CPhysicsActor& actor, const CVector3f& destination,
                                      float dampingRadius) const {
  const CVector3f delta = destination - actor.GetTranslation();
  if (delta.CanBeNormalized()) {
    if (delta.MagSquared() < (dampingRadius * dampingRadius)) {
      dampingRadius = delta.MagSquared() / (dampingRadius * dampingRadius);
    } else {
      dampingRadius = 1.f;
    }
    return dampingRadius * delta.AsNormalized();
  }
  return CVector3f::Zero();
}

CVector3f CSteeringBehaviors::Separation(const CPhysicsActor& actor, const CVector3f& position,
                                         float separation) const {
  const CVector3f delta = actor.GetTranslation() - position;
  const float distanceSquared = delta.MagSquared();
  const float radiusSquared = separation * separation;
  CVector3f ret = CVector3f::Zero();
  if (distanceSquared < radiusSquared) {
    const float t = (1.f - (distanceSquared / radiusSquared));
    ret = delta.CanBeNormalized() ? delta.AsNormalized() * t : actor.GetTransform().GetForward();
  }
  return ret;
}

CVector3f CSteeringBehaviors::Alignment(const CPhysicsActor& actor,
                                        rstl::reserved_vector< TUniqueId, 1024 >& list,
                                        const CStateManager& mgr) const {
  CVector3f direction = CVector3f::Zero();
  if (!list.empty()) {
    for (int i = 0; i < list.size(); ++i) {
      if (const CActor* neighbor = static_cast< const CActor* >(mgr.GetObjectById(list[i]))) {
        direction += neighbor->GetTransform().GetForward();
      }
    }
    direction *= 1.f / list.size();
  }

  const float diff = CVector3f::GetAngleDiff(actor.GetTransform().GetForward(), direction);
  direction *= (diff / M_PIF);
  return direction;
}


CVector3f CSteeringBehaviors::Cohesion(const CPhysicsActor& actor,
                                       rstl::reserved_vector< TUniqueId, 1024 >& list,
                                       float dampingRadius, const CStateManager& mgr) const {
  CVector3f destination = CVector3f::Zero();
  if (!list.empty()) {
    for (int i = 0; i < list.size(); ++i) {
      if (const CActor* neighbor = static_cast< const CActor* >(mgr.GetObjectById(list[i]))) {
        destination += neighbor->GetTranslation();
      }
    }
    destination *= 1.f / list.size();
    return Arrival(actor, destination, dampingRadius);
  }
  return destination;
}

CVector2f CSteeringBehaviors::Separation2D(const CPhysicsActor& actor, const CVector2f& position,
                                           float separation) const {
  CVector2f ret = CVector2f::Zero();
  const CVector2f delta = actor.GetTranslation().ToVec2f() - position;
  const float distanceSquared = delta.MagSquared();
  const float radiusSquared = separation * separation;
  if (distanceSquared < radiusSquared) {
    const float t = (1.f - (distanceSquared / radiusSquared));
    ret = distanceSquared > FLT_EPSILON ? delta.AsNormalized() * t
                                        : actor.GetTransform().GetForward().ToVec2f();
  }
  return ret;
}

bool CSteeringBehaviors::ProjectLinearIntersection(const CVector3f& origin, float speed,
                                                   const CVector3f& position,
                                                   const CVector3f& velocity,
                                                   CVector3f& intersection) {
  const CVector3f delta = position - origin;
  float positive, negative;
  if (CMath::SolveQuadratic(velocity.MagSquared() - speed * speed,
                            2.f * CVector3f::Dot(velocity, delta), delta.MagSquared(), positive,
                            negative) &&
      negative > 0.f) {
    intersection = position + velocity * negative;
    return true;
  }
  return false;
}

bool CSteeringBehaviors::ProjectLinearIntersection(const CVector3f& origin, float speed,
                                                   const CVector3f& position,
                                                   const CVector3f& velocity,
                                                   const CVector3f& acceleration,
                                                   CVector3f& intersection) {
  const CVector3f delta = position - origin;
  // The intercept time t solves |delta + velocity*t + 0.5f*acceleration*t*t| == speed*t; squared
  // out that is the quartic below, in ascending powers of t. Retail's constants at
  // 0x8041B990/0x8041B994/0x8041B998 are 2.0f, 0.25f and 0.5f.
  float coefficients[5];
  coefficients[0] = delta.MagSquared();
  coefficients[1] = CVector3f::Dot(delta, velocity) * 2.f;
  coefficients[2] =
    velocity.MagSquared() + CVector3f::Dot(delta, acceleration) - speed * speed;
  coefficients[3] = CVector3f::Dot(velocity, acceleration);
  coefficients[4] = acceleration.MagSquared() * 0.25f;

  float roots[4];
  bool found = false;
  // Unsigned on purpose: retail's loop guard is `cmplwi r3,0` + `ble`, not `cmpwi`, so the counter
  // and the bound have to be `uint` for mwcceppc to pick the same compare.
  const uint rootCount = fn_802CB918(coefficients, roots);
  for (uint i = 0; i < rootCount; ++i) {
    const float time = roots[i];
    if (time > 0.f) {
      found = true;
      intersection = position + velocity * time + 0.5f * time * time * acceleration;
    }
  }
  const bool result = found;
  return result;
}

bool CSteeringBehaviors::ProjectOrbitalIntersection(const CVector3f& origin, float speed,
                                                    float dt, const CVector3f& position,
                                                    const CVector3f& velocity,
                                                    const CVector3f& orbitPoint,
                                                    CVector3f& intersection) {
  if (speed > 0.f) {
    if (velocity.CanBeNormalized()) {
      CVector3f radial((position - orbitPoint).ToVec2f(), 0.f);
      if (radial.CanBeNormalized()) {
        CVector3f currentPosition = position;
        CVector3f currentVelocity = velocity;
        CVector3f delta = currentPosition - origin;
        float travelTime = delta.Magnitude() / speed;
        float elapsed = 0.f;
        float previousRemaining = FLT_MAX;
        float remaining = travelTime - elapsed;
        CVector3f radialUnit = radial.AsNormalized();
        CVector3f tangent = CVector3f::Cross(radialUnit, CVector3f::Up());
        const float tangentialSpeed = CVector3f::Dot(currentVelocity, tangent);
        const float radialSpeed = CVector3f::Dot(currentVelocity, radialUnit);

        while (remaining < previousRemaining && elapsed < 4.f) {
          if (close_enough(remaining, dt) || remaining < 0.f) {
            intersection = currentPosition;
            return true;
          }

          currentPosition += dt * currentVelocity;
          previousRemaining = remaining;
          radial = CVector3f((currentPosition - orbitPoint).ToVec2f(), 0.f);
          if (!radial.CanBeNormalized()) {
            break;
          }

          radialUnit = radial.AsNormalized();
          CVector3f tangent = CVector3f::Cross(radialUnit, CVector3f::Up());
          currentVelocity = tangentialSpeed * tangent + radialSpeed * radialUnit;
          delta = currentPosition - origin;
          travelTime = delta.Magnitude() / speed;
          elapsed += dt;
          remaining = travelTime - elapsed;
        }
      } else {
        return ProjectLinearIntersection(origin, speed, position, velocity, intersection);
      }
    } else {
      intersection = position;
      return true;
    }
  }
  return false;
}

bool CSteeringBehaviors::ProjectOrbitalIntersection(const CVector3f& origin, float speed,
                                                    float dt, const CVector3f& position,
                                                    const CVector3f& velocity,
                                                    const CVector3f& acceleration,
                                                    const CVector3f& orbitPoint,
                                                    CVector3f& intersection) {
  bool found = false;
  if (speed > 0.f) {
    CVector3f radial((position - orbitPoint).ToVec2f(), 0.f);
    if (velocity.CanBeNormalized() && radial.CanBeNormalized()) {
      CVector3f currentPosition = position;
      CVector3f currentVelocity = velocity;
      CVector3f delta = currentPosition - origin;
      float travelTime = delta.Magnitude() / speed;
      float elapsed = 0.f;
      float previousRemaining = FLT_MAX;
      float remaining = travelTime - elapsed;
      CVector3f radialUnit = radial.AsNormalized();
      CVector3f tangent = CVector3f::Cross(radialUnit, CVector3f::Up());
      const float tangentialSpeed = CVector3f::Dot(currentVelocity, tangent);
      const float radialSpeed = CVector3f::Dot(currentVelocity, radialUnit);

      while (remaining < previousRemaining && elapsed < 4.f) {
        if (close_enough(remaining, dt) || remaining < 0.f) {
          intersection = currentPosition;
          found = true;
          break;
        }

        currentPosition += dt * currentVelocity;
        previousRemaining = remaining;
        delta = currentPosition - origin;
        travelTime = delta.Magnitude() / speed;
        elapsed += dt;
        remaining = travelTime - elapsed;
        radial = CVector3f((currentPosition - orbitPoint).ToVec2f(), 0.f);
        if (!radial.CanBeNormalized()) {
          break;
        }

        radialUnit = radial.AsNormalized();
        CVector3f tangent = CVector3f::Cross(radialUnit, CVector3f::Up());
        currentVelocity = CVector3f(0.f, 0.f, currentVelocity.GetZ()) + dt * acceleration;
        currentVelocity += tangentialSpeed * tangent + radialSpeed * radialUnit;
      }
    } else {
      return ProjectLinearIntersection(origin, speed, position, velocity, acceleration,
                                       intersection);
    }
  }
  const bool result = found;
  return result;
}

CVector3f CSteeringBehaviors::ProjectOrbitalPosition(const CVector3f& position,
                                                     const CVector3f& velocity,
                                                     const CVector3f& orbitPoint, float dt,
                                                     float preThinkDt) {
  CVector3f currentPosition = position;
  if (velocity.CanBeNormalized()) {
    CVector3f radial((position - orbitPoint).ToVec2f(), 0.f);
    if (radial.CanBeNormalized()) {
      CVector3f currentVelocity = velocity;
      float elapsed = 0.f;
      CVector3f radialUnit = radial.AsNormalized();
      CVector3f tangent = CVector3f::Cross(radialUnit, CVector3f::Up());
      const float tangentialSpeed = CVector3f::Dot(currentVelocity, tangent);
      const float radialSpeed = CVector3f::Dot(currentVelocity, radialUnit);

      while (elapsed < dt) {
        currentPosition += preThinkDt * currentVelocity;
        radial = CVector3f((currentPosition - orbitPoint).ToVec2f(), 0.f);
        if (radial.CanBeNormalized()) {
          radialUnit = radial.AsNormalized();
          CVector3f tangent = CVector3f::Cross(radialUnit, CVector3f::Up());
          currentVelocity = tangentialSpeed * tangent + radialSpeed * radialUnit;
        }

        float step = dt - elapsed;
        if (step > preThinkDt) {
          step = preThinkDt;
        }
        elapsed += step;
      }
    }
  }
  return currentPosition;
}
