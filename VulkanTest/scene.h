#pragma once

#include <vector>
#include "render.h"
#include "btBulletDynamicsCommon.h"
#include "btBulletCollisionCommon.h"

/* renderer component */

struct Renderer {
	uint32_t objectIndex;
	render::Mesh* mesh;
	render::Material* material;

	Renderer(render::Material* material, render::Mesh* mesh, uint32_t object);
};

class Scene {
private:
	float timestep;

	render::Camera mainCamera;

	render::Drawer* drawer;

	btDefaultCollisionConfiguration* defaultConfig;

	btCollisionDispatcher* dispatcher;

	btBroadphaseInterface* pairCache;

	btSequentialImpulseConstraintSolver* solver;

	btDiscreteDynamicsWorld* world;
	
	btAlignedObjectArray<btCollisionShape*> collisionShapes;
	std::vector<Renderer> renderedScene;

public:
	Scene(render::Drawer* drawer);

	void step();
	void addRigidBody(btRigidBody::btRigidBodyConstructionInfo info);
	void attachRenderer(Renderer component);
	void drawObjects();
};