#include "engine/physics/CollisionCallbackDispatcher.h"

#include "engine/physics/CollisionInfo.h"
#include "engine/physics/EntityUserData.h"

#include <Jolt/Physics/Body/Body.h>

#include <cmath>
#include <utility>

namespace engine::physics {

namespace {

std::uint64_t MakeBodyPairKey(const JPH::Body& body1, const JPH::Body& body2) {
    std::uint32_t raw1 = body1.GetID().GetIndexAndSequenceNumber();
    std::uint32_t raw2 = body2.GetID().GetIndexAndSequenceNumber();
    if (raw1 > raw2) {
        std::swap(raw1, raw2);
    }
    return (static_cast<std::uint64_t>(raw1) << 32) | static_cast<std::uint64_t>(raw2);
}

glm::vec3 FromJolt(JPH::Vec3Arg v) { return glm::vec3(v.GetX(), v.GetY(), v.GetZ()); }

script::ScriptComponent* FindScript(const std::vector<script::ScriptComponent*>& scripts, ecs::Entity entity) {
    for (script::ScriptComponent* script : scripts) {
        if (script != nullptr && script->GetEntity() == entity) {
            return script;
        }
    }
    return nullptr;
}

} // namespace

void CollisionCallbackDispatcher::PushRawEvent(RawEventType type, const JPH::Body& body1,
                                               const JPH::Body& body2,
                                               const JPH::ContactManifold& manifold,
                                               const JPH::ContactSettings& settings) {
    RawEvent event;
    event.type = type;
    event.bodyPairKey = MakeBodyPairKey(body1, body2);
    event.userData1 = body1.GetUserData();
    event.userData2 = body2.GetUserData();
    event.contactPoint = FromJolt(manifold.GetWorldSpaceContactPointOn1(0));
    event.contactNormal = FromJolt(manifold.mWorldSpaceNormal);
    // Relative speed along the contact normal at the moment of first contact -- see
    // CollisionInfo::impactSpeed's own comment for why this (not a true post-solve
    // impulse) is what's reported.
    const JPH::Vec3 relativeVelocity = body1.GetLinearVelocity() - body2.GetLinearVelocity();
    event.impactSpeed = std::abs(relativeVelocity.Dot(manifold.mWorldSpaceNormal));
    event.isSensor = settings.mIsSensor;

    std::lock_guard<std::mutex> lock(m_bufferMutex);
    m_pendingEvents.push_back(event);
}

void CollisionCallbackDispatcher::OnContactAdded(const JPH::Body& body1, const JPH::Body& body2,
                                                  const JPH::ContactManifold& manifold,
                                                  JPH::ContactSettings& settings) {
    PushRawEvent(RawEventType::Added, body1, body2, manifold, settings);
}

void CollisionCallbackDispatcher::OnContactPersisted(const JPH::Body& body1, const JPH::Body& body2,
                                                      const JPH::ContactManifold& manifold,
                                                      JPH::ContactSettings& settings) {
    PushRawEvent(RawEventType::Persisted, body1, body2, manifold, settings);
}

void CollisionCallbackDispatcher::OnContactRemoved(const JPH::SubShapeIDPair& subShapePair) {
    // Cannot access the bodies here at all (Jolt's own ContactListener.h comment) -- only
    // the raw ids survive into the buffer; Dispatch() resolves entities/isSensor from
    // m_activeContacts (populated by the matching Added event) instead of from the bodies.
    std::uint32_t raw1 = subShapePair.GetBody1ID().GetIndexAndSequenceNumber();
    std::uint32_t raw2 = subShapePair.GetBody2ID().GetIndexAndSequenceNumber();
    if (raw1 > raw2) {
        std::swap(raw1, raw2);
    }

    RawEvent event;
    event.type = RawEventType::Removed;
    event.bodyPairKey = (static_cast<std::uint64_t>(raw1) << 32) | static_cast<std::uint64_t>(raw2);

    std::lock_guard<std::mutex> lock(m_bufferMutex);
    m_pendingEvents.push_back(event);
}

void CollisionCallbackDispatcher::Dispatch(const std::vector<script::ScriptComponent*>& scripts) {
    std::vector<RawEvent> events;
    {
        std::lock_guard<std::mutex> lock(m_bufferMutex);
        events.swap(m_pendingEvents);
    }

    for (const RawEvent& event : events) {
        if (event.type == RawEventType::Removed) {
            auto it = m_activeContacts.find(event.bodyPairKey);
            if (it == m_activeContacts.end()) {
                continue; // e.g. removed before any Added ever reached Dispatch() -- ignore
            }
            const ActiveContact contact = it->second;
            m_activeContacts.erase(it);

            script::ScriptComponent* script1 = FindScript(scripts, contact.entity1);
            script::ScriptComponent* script2 = FindScript(scripts, contact.entity2);
            if (contact.isSensor) {
                if (script1 != nullptr) {
                    script1->OnTriggerExit(contact.entity2);
                }
                if (script2 != nullptr) {
                    script2->OnTriggerExit(contact.entity1);
                }
            } else {
                if (script1 != nullptr) {
                    script1->OnCollisionExit(contact.entity2);
                }
                if (script2 != nullptr) {
                    script2->OnCollisionExit(contact.entity1);
                }
            }
            continue;
        }

        const ecs::Entity entity1 = UnpackEntityUserData(event.userData1);
        const ecs::Entity entity2 = UnpackEntityUserData(event.userData2);

        if (event.type == RawEventType::Added) {
            m_activeContacts[event.bodyPairKey] = ActiveContact{entity1, entity2, event.isSensor};
        }

        script::ScriptComponent* script1 = FindScript(scripts, entity1);
        script::ScriptComponent* script2 = FindScript(scripts, entity2);

        if (event.isSensor) {
            // No OnTriggerStay in the design (docs/01 section 9.6) -- only Enter/Exit
            // matter for sensors, so Persisted events for a sensor pair are dropped here.
            if (event.type == RawEventType::Added) {
                if (script1 != nullptr) {
                    script1->OnTriggerEnter(entity2);
                }
                if (script2 != nullptr) {
                    script2->OnTriggerEnter(entity1);
                }
            }
            continue;
        }

        CollisionInfo infoForEntity1;
        infoForEntity1.otherEntity = entity2;
        infoForEntity1.contactPoint = event.contactPoint;
        infoForEntity1.contactNormal = event.contactNormal;
        infoForEntity1.impactSpeed = event.impactSpeed;

        CollisionInfo infoForEntity2 = infoForEntity1;
        infoForEntity2.otherEntity = entity1;
        infoForEntity2.contactNormal = -event.contactNormal; // away from *this* surface

        if (event.type == RawEventType::Added) {
            if (script1 != nullptr) {
                script1->OnCollisionEnter(infoForEntity1);
            }
            if (script2 != nullptr) {
                script2->OnCollisionEnter(infoForEntity2);
            }
        } else { // Persisted
            if (script1 != nullptr) {
                script1->OnCollisionStay(infoForEntity1);
            }
            if (script2 != nullptr) {
                script2->OnCollisionStay(infoForEntity2);
            }
        }
    }
}

} // namespace engine::physics
