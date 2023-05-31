#include "scene.h"

#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
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

Renderer::Renderer() {

}

Scene::Scene() {
	Scene::timestep = 1 / 40;

	Scene::mainCamera = render::Camera();

	Scene::defaultConfig = new btDefaultCollisionConfiguration();

	Scene::dispatcher = new btCollisionDispatcher(defaultConfig);

	Scene::pairCache = new btDbvtBroadphase();

	Scene::solver = new btSequentialImpulseConstraintSolver();

	Scene::world = new btDiscreteDynamicsWorld(dispatcher, pairCache, solver, defaultConfig);
}

void Scene::addRigidBody(btTransform transform = btTransform(btQuaternion())) {

}

void Scene::attachRenderer(Renderer component) {
	renderedScene.push_back(component);

	/* TODO: sorting function? */
}

void Scene::drawObjects() {
	for (size_t i = 0; i < Scene::renderedScene.size(); i++)
	{
		glm::mat4 m{};
		world->getCollisionObjectArray()[Scene::renderedScene[i].objectIndex]->getWorldTransform().getOpenGLMatrix(reinterpret_cast<btScalar*>(&m));
		drawer.draw(Scene::renderedScene[i].mesh, Scene::renderedScene[i].material, m);
	}
}

