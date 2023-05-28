#pragma once

#include <cstdlib>
#include "render.h"

float timestep;

renderobject::Camera mainCamera;

std::vector<renderobject::Material> materials;

class scene {
	btDefaultCollisionConfiguration* defaultConfig = new btDefaultCollisionConfiguration();

	btCollisionDispatcher* dispatcher = new btCollisionDispatcher(defaultConfig);

	btBroadphaseInterface* pairCache = new btDbvtBroadphase();

	btSequentialImpulseConstraintSolver* solver = new btSequentialImpulseConstraintSolver();

	btDiscreteDynamicsWorld* world = new btDiscreteDynamicsWorld(dispatcher, pairCache, solver, defaultConfig);
	btDynamicsWorld* w;

	/* renderer component */
	struct renderer {
		int objectIndex;
		renderobject::Mesh* mesh;
		renderobject::Material* material;
	};

	std::vector<renderer> renderedScene;

	scene();

	void drawObjects();
	void drawFrame();
};