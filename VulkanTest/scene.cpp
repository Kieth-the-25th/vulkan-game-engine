#include "scene.h"
#include "render.h"

#include <GLFW/glfw3.h>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <iostream>
#include <fstream>
#include <stdexcept>
#include <vector>
#include <set>

#include "BulletCollision/btBulletCollisionCommon.h"
#include "btBulletDynamicsCommon.h"

#include <stdio.h>

Renderer::Renderer(render::Material* material, render::Mesh* mesh, uint32_t object) {
	this->objectIndex = object;
	this->material = material;
	this->mesh = mesh;
}

Scene::Scene(render::Drawer* drawer) {
	Scene::timestep = 1 / 60;

	Scene::mainCamera = render::Camera();

	Scene::defaultConfig = new btDefaultCollisionConfiguration();

	Scene::dispatcher = new btCollisionDispatcher(defaultConfig);

	Scene::pairCache = new btDbvtBroadphase();

	Scene::solver = new btSequentialImpulseConstraintSolver();

	Scene::world = new btDiscreteDynamicsWorld(dispatcher, pairCache, solver, defaultConfig);

	Scene::drawer = drawer;

	world->setGravity({ 0, 0, -9.81 });
}

void Scene::step() {
	world->stepSimulation(timestep, 10);
}

void Scene::addRigidBody(btRigidBody::btRigidBodyConstructionInfo info) {
	btRigidBody* body = new btRigidBody(info);
	world->addRigidBody(body);
}

void Scene::attachRenderer(Renderer component) {
	renderedScene.push_back(component);

	/* TODO: sorting function? */
}

void Scene::drawObjects() {
	drawer->beginFrame(glm::lookAt(glm::vec3(1, 1, 1), glm::vec3(0, 0, 0), glm::vec3(0, 0, 1)), 60);
	for (size_t i = 0; i < Scene::renderedScene.size(); i++)
	{
		glm::mat4 m{};
		btTransform transform;
		btMotionState* state = btRigidBody::upcast(world->getCollisionObjectArray()[Scene::renderedScene[i].objectIndex])->getMotionState();
		state->getWorldTransform(transform);
		transform.getOpenGLMatrix(reinterpret_cast<btScalar*>(&m));
		std::cout << m[0][0] << " " << m[1][0] << " " << m[2][0] << " " << m[3][0] << "\n";
		std::cout << m[0][1] << " " << m[1][1] << " " << m[2][1] << " " << m[3][1] << "\n";
		std::cout << m[0][2] << " " << m[1][2] << " " << m[2][2] << " " << m[3][2] << "\n";
		std::cout << m[0][3] << " " << m[1][3] << " " << m[2][3] << " " << m[3][3] << "\n";
		drawer->draw(Scene::renderedScene[i].mesh, Scene::renderedScene[i].material, m);
	}
	drawer->submitDraws();
	drawer->endFrame();
}

