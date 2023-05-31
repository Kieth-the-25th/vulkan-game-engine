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

	Renderer();
};

class Scene {
private:
	float timestep;

	render::Camera mainCamera;

	render::Drawer drawer;

	btDefaultCollisionConfiguration* defaultConfig;

	btCollisionDispatcher* dispatcher;

	btBroadphaseInterface* pairCache;

	btSequentialImpulseConstraintSolver* solver;

	btDiscreteDynamicsWorld* world;
	
	std::vector<Renderer> renderedScene;

	Scene();

	void addRigidBody(btTransform transform = btTransform(btQuaternion()));
	void attachRenderer(Renderer component);
	void drawObjects();
};