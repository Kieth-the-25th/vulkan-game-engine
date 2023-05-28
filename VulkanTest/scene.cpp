#include "scene.h"

#include <vulkan/vulkan.h>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <stb_image.h>

#include <chrono>

#include <iostream>
#include <fstream>
#include <stdexcept>
#include <cstdlib>
#include <cstdint>
#include <algorithm>
#include <optional>
#include <vector>
#include <string>
#include <cstring>
#include <set>

#include "btBulletDynamicsCommon.h"
#include "BulletSoftBody/btSoftBody.h"
#include "Bullet3Common/b3Quaternion.h"
#include "Bullet3Common/b3AlignedObjectArray.h"
#include <stdio.h>

btDefaultCollisionConfiguration* defaultConfig = new btDefaultCollisionConfiguration();

btCollisionDispatcher* dispatcher = new btCollisionDispatcher(defaultConfig);

btBroadphaseInterface* pairCache = new btDbvtBroadphase();

btSequentialImpulseConstraintSolver* solver = new btSequentialImpulseConstraintSolver();

btDiscreteDynamicsWorld* world = new btDiscreteDynamicsWorld(dispatcher, pairCache, solver, defaultConfig);
btDynamicsWorld* w;

float timestep = 1/40;

scene::scene() {
	scene::defaultConfig = new btDefaultCollisionConfiguration();

	scene::dispatcher = new btCollisionDispatcher(defaultConfig);

	scene::pairCache = new btDbvtBroadphase();

	scene::solver = new btSequentialImpulseConstraintSolver();

	scene::world = new btDiscreteDynamicsWorld(dispatcher, pairCache, solver, defaultConfig);
}

void scene::drawObjects() {
	for (size_t i = 0; i < scene::renderedScene.size(); i++)
	{
		render::draw(scene::renderedScene[i].mesh);
	}
}

void init() {
	world->setGravity(btVector3(0, 0, 0));

}

void loop() {
	world->stepSimulation(timestep);
}

