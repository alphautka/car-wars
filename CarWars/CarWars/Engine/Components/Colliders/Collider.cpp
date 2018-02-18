#include "Collider.h"
#include <extensions/PxRigidActorExt.h>
#include "../../Systems/Physics/CollisionFilterShader.h"
#include "../../Systems/Content/ContentManager.h"
#include "../../Systems/Physics/VehicleSceneQuery.h"
#include "imgui/imgui.h"

using namespace physx;

Collider::Collider(std::string _collisionGroup, physx::PxMaterial *_material, physx::PxFilterData _queryFilterData)
    : collisionGroup(_collisionGroup), material(_material), queryFilterData(_queryFilterData), shape(nullptr), geometry(nullptr) {}

Collider::Collider(nlohmann::json data) {
    collisionGroup = ContentManager::GetFromJson<std::string>(data["CollisionGroup"], "Default");
    material = ContentManager::GetPxMaterial(ContentManager::GetFromJson<std::string>(data["Material"], "Default.json"));
    const std::string queryFilterType = ContentManager::GetFromJson<std::string>(data["QueryFilterType"], "DrivableSurface");
    if (queryFilterType == "DrivableSurface") {
        setupDrivableSurface(queryFilterData);
    } else {
        setupNonDrivableSurface(queryFilterData);
    }
    transform = Transform(data);
}

Collider::~Collider() {
    shape->release();
    delete geometry;
}

physx::PxShape* Collider::GetShape() const {
    return shape;
}

void Collider::CreateShape(PxRigidActor *actor) {
    shape = physx::PxRigidActorExt::createExclusiveShape(*actor, *geometry, *material);
    shape->setQueryFilterData(queryFilterData);                                         // For raycasts
    shape->setSimulationFilterData(CollisionGroups::GetFilterData(collisionGroup));     // For collisions
    shape->setLocalPose(Transform::ToPx(transform));
}

void Collider::RenderDebugGui() {
    if (ImGui::TreeNode("Transform")) {
        if (transform.RenderDebugGui()) shape->setLocalPose(Transform::ToPx(transform));
        ImGui::TreePop();
    }
    ImGui::LabelText("Collision Group", "%s", collisionGroup);
    ImGui::Text("Static Friction: %f", material->getStaticFriction());
    ImGui::Text("Dynamic Friction: %f", material->getDynamicFriction());
    ImGui::Text("Restitution Friction: %f", material->getRestitution());
}

std::string Collider::GetTypeName(ColliderType type) {
    switch(type) {
        case Collider_Box: return "Box";
        case Collider_ConvexMesh: return "ConvexMesh";
        case Collider_TriangleMesh: return "TriangleMesh";
        default: return std::to_string(type);
    }
}

Transform Collider::GetLocalTransform() const {
    return shape->getLocalPose();
}

Transform Collider::GetGlobalTransform() const {
    return PxShapeExt::getGlobalPose(*shape, *shape->getActor());
}