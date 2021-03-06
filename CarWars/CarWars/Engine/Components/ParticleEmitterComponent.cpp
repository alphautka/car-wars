#include "ParticleEmitterComponent.h"
#include "../Systems/StateManager.h"
#include "../Systems/Content/ContentManager.h"
#define _USE_MATH_DEFINES
#include <math.h>
#include <glm/gtx/string_cast.hpp>

ParticleEmitterComponent::~ParticleEmitterComponent() {
    glDeleteVertexArrays(1, &vao);
    glDeleteBuffers(1, &vbo);
}

ParticleEmitterComponent::ParticleEmitterComponent(nlohmann::json data) {
    transform = Transform(data);

    emitOnSpawn = ContentManager::GetFromJson<size_t>(data["EmitOnSpawn"], 0);
    emitCount = ContentManager::GetFromJson<size_t>(data["EmitCount"], 1);
    emitConeMinAngle = glm::radians(ContentManager::GetFromJson<float>(data["EmitConeMinAngle"], 0.f));
    emitConeMaxAngle = glm::radians(ContentManager::GetFromJson<float>(data["EmitConeMaxAngle"], 90.f));
    emitScale = ContentManager::JsonToVec3(data["EmitScale"], glm::vec3());

    lockedToEntity = ContentManager::GetFromJson<bool>(data["LockedToEntity"], false);

    initialSpeed = ContentManager::GetFromJson<float>(data["InitialSpeed"], 10.f);
    acceleration = ContentManager::JsonToVec3(data["Acceleration"], glm::vec3(0.f, -9.81f, 0.f));

    initialScale = ContentManager::JsonToVec2(data["InitialScale"], glm::vec2(1.f));
    finalScale = ContentManager::JsonToVec2(data["FinalScale"], glm::vec2(1.f));

    texture = ContentManager::GetTexture(ContentManager::GetFromJson<std::string>(data["Texture"], "Particles/Explosion.png"));
    initialColor = ContentManager::GetColorFromJson(data["InitialColor"], glm::vec4(1.f));
    finalColor = ContentManager::GetColorFromJson(data["FinalColor"], glm::vec4(1.f));
    emissiveness = ContentManager::GetFromJson<float>(data["Emissiveness"], 0.f);

    isSprite = ContentManager::GetFromJson<bool>(data["IsSprite"], false);
    spriteColumns = ContentManager::GetFromJson<int>(data["SpriteColumns"], 1);
    spriteRows = ContentManager::GetFromJson<int>(data["SpriteRows"], 1);
    spriteSize = ContentManager::JsonToVec2(data["SpriteSize"], glm::vec2(10.f));
    animationCycles = ContentManager::GetFromJson<float>(data["AnimationCycles"], 2.f);

    lifetime = ContentManager::GetFromJson<double>(data["Lifetime"], 3.0);
    spawnRate = ContentManager::GetFromJson<double>(data["SpawnRate"], 0.1);
    nextSpawn = StateManager::globalTime + spawnRate;

    InitializeBuffers();
}

void ParticleEmitterComponent::UpdateBuffers() {
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(Particle) * particles.size(), particles.data(), GL_DYNAMIC_DRAW);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void ParticleEmitterComponent::InitializeBuffers() {
    glGenBuffers(1, &vbo);
    UpdateBuffers();

    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);

    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);

    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Particle), reinterpret_cast<const GLvoid*>(0));                  // position
    glVertexAttribPointer(1, 1, GL_FLOAT, GL_FALSE, sizeof(Particle), reinterpret_cast<const GLvoid*>(sizeof(float) * 6));  // lifetime

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
}

void ParticleEmitterComponent::Update() {
    const float delta = StateManager::deltaTime.GetSeconds();

    for (auto it = particles.begin(); it != particles.end();) {
        Particle& particle = *it;
        if (particle.lifetimeSeconds > lifetime.GetSeconds()) {
            it = particles.erase(it);
        } else {
            particle.velocity = particle.velocity + delta * acceleration;
            particle.position = particle.position + delta * particle.velocity;
            particle.lifetimeSeconds += delta;
            ++it;
        }
    }

    if (spawnRate > 0.0 && StateManager::globalTime >= nextSpawn) {
        nextSpawn = StateManager::globalTime + spawnRate;
        Emit(emitCount);
    }
}

void ParticleEmitterComponent::AddParticle(glm::vec3 p, glm::vec3 v) {
    if (GetParticleCount() >= MAX_PARTICLES) return;
    
    Particle particle;
    particle.position = lockedToEntity ? p : p + transform.GetGlobalPosition();
    particle.velocity = v;
    particle.lifetimeSeconds = 0.f;
    particles.push_back(particle);
}

float UnitRand() {
    return static_cast <float> (rand()) / static_cast <float> (RAND_MAX);
}

float UnitRandNegative() {
    return (UnitRand() * 2.f) - 1.f;
}

void ParticleEmitterComponent::Emit(size_t count) {
    const glm::vec3 forward = transform.GetForward();
    glm::vec3 cross = normalize(glm::cross(Transform::UP, forward));
    if (abs(length(cross)) == 0.f) cross = Transform::RIGHT;
    for (size_t i = 0; i < count; ++i) {
        const float fAngle = UnitRand() * M_PI * 2.f;
        const float cAngle = glm::mix(emitConeMinAngle, emitConeMaxAngle, UnitRand());

        const glm::quat qAroundF = angleAxis(fAngle, forward);
        const glm::quat qAroundC = angleAxis(cAngle, cross);
        const glm::quat q = qAroundF * qAroundC;
        
        const glm::vec3 direction = normalize(q * forward);

        const glm::vec3 localPosition = emitScale * glm::vec3(UnitRandNegative(), UnitRandNegative(), UnitRandNegative());
        const glm::vec3 position = localPosition;
        AddParticle(position, direction * initialSpeed);
    }
}

void ParticleEmitterComponent::SetEmitScale(glm::vec3 _emitScale) {
    emitScale = _emitScale;
}

bool ParticleEmitterComponent::IsLockedToEntity() const {
    return lockedToEntity;
}

glm::mat4 ParticleEmitterComponent::GetModelMatrix() {
    if (!lockedToEntity) return glm::mat4(1.f);
    return transform.GetTransformationMatrix();
}

float ParticleEmitterComponent::GetInitialSpeed() const {
    return initialSpeed;
}

void ParticleEmitterComponent::Sort(glm::vec3 cameraPosition) {
    glm::vec3 localCameraPosition = lockedToEntity ? inverse(transform.GetTransformationMatrix()) * glm::vec4(cameraPosition, 1.f) : cameraPosition;
    std::sort(particles.begin(), particles.end(), [localCameraPosition](const Particle& lhs, const Particle& rhs) -> bool {
        return length(lhs.position - localCameraPosition) > length(rhs.position - localCameraPosition);
    });
    UpdateBuffers();
}

GLuint ParticleEmitterComponent::GetVao() const {
    return vao;
}

size_t ParticleEmitterComponent::GetParticleCount() const {
    return particles.size();
}

void ParticleEmitterComponent::SetEmitCount(size_t _emitCount) {
    emitCount = _emitCount;
}

glm::vec2 ParticleEmitterComponent::GetInitialScale() const {
    return initialScale;
}

void ParticleEmitterComponent::SetInitialScale(glm::vec2 scale) {
    initialScale = scale;
}

glm::vec2 ParticleEmitterComponent::GetFinalScale() const {
    return finalScale;
}

void ParticleEmitterComponent::SetFinalScale(glm::vec2 scale) {
    finalScale = scale;
}

Texture* ParticleEmitterComponent::GetTexture() const {
    return texture;
}

glm::vec4 ParticleEmitterComponent::GetInitialColor() const {
    return initialColor;
}

glm::vec4 ParticleEmitterComponent::GetFinalColor() const {
    return finalColor;
}

float ParticleEmitterComponent::GetEmissiveness() const {
    return emissiveness;
}

bool ParticleEmitterComponent::IsSprite() const {
    return isSprite;
}

int ParticleEmitterComponent::GetSpriteColumns() const {
    return spriteColumns;
}

int ParticleEmitterComponent::GetSpriteRows() const {
    return spriteRows;
}

glm::vec2 ParticleEmitterComponent::GetSpriteSize() const {
    return spriteSize;
}

float ParticleEmitterComponent::GetAnimationCycles() const {
    return animationCycles;
}

float ParticleEmitterComponent::GetLifetimeSeconds() const {
    return lifetime.GetSeconds();
}

void ParticleEmitterComponent::SetLifetime(Time _lifetime) {
    lifetime = _lifetime;
}

void ParticleEmitterComponent::SetSpawnRate(float _spawnRate) {
    spawnRate = _spawnRate;
}

void ParticleEmitterComponent::SetAnimationCycles(float _animationCycles) {
    animationCycles = _animationCycles;
}

void ParticleEmitterComponent::SetDirections(glm::vec3 direction) {
    for (Particle& p : particles) {
        p.velocity = direction * length(p.velocity);
    }
}

ComponentType ParticleEmitterComponent::GetType() {
    return ComponentType_ParticleEmitter;
}

void ParticleEmitterComponent::HandleEvent(Event* event) { }

void ParticleEmitterComponent::SetEntity(Entity* _entity) {
    Component::SetEntity(_entity);
    transform.parent = &_entity->transform;
    Emit(emitOnSpawn);
}
