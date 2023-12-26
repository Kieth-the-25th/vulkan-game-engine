#include "scene.h"
#include "render.h"
#include "threading.h"

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
#include "bulletCustom.h"

#include <stdio.h>

Renderer::Renderer(render::Mesh* mesh, uint8_t count, btCustomMotionState* motionState) {
	this->motionState = motionState;
	this->mesh = mesh;
	this->count = count;
}

Scene::Scene(Threading* threading, render::Drawer* drawer) {
	Scene::timestep = static_cast<float>(1) / 60;

	//Scene::mainCamera = render::Camera();

	auto defaultConfig = new btDefaultCollisionConfiguration();

	auto dispatcher = new btCollisionDispatcher(defaultConfig);

	auto pairCache = new btDbvtBroadphase();

	auto solver = new btSequentialImpulseConstraintSolver();

	auto world = new btDiscreteDynamicsWorld(dispatcher, pairCache, solver, defaultConfig);

	physics = {defaultConfig, dispatcher, pairCache, solver, world};

	Scene::drawer = drawer;

	world->setGravity({ 0, -2, 0 });
}

void Scene::step() {
	//world->getCollisionObjectArray()[0]->forceActivationState(4);
	physics.world->stepSimulation(1);
	//std::cout << synchronizedObjects.size() << "\n";
	for (size_t i = 0; i < synchronizedObjects.size(); i++)
	{
		synchronizedObjects[i]->update(physics);
	}
}

btRigidBody* Scene::addRigidBody(btRigidBody::btRigidBodyConstructionInfo info) {
	btRigidBody* body = new btRigidBody(info);
	physics.world->addRigidBody(body);
	return body;
}

void Scene::addSyncObject(SyncFunc* o) {
	synchronizedObjects.push_back(o);
}

void Scene::addAsyncObject(AsyncFunc* o) {
	threadedObjects.push_back(o);
}

void Scene::attachRenderer(Renderer component) {
	renderedScene.push_back(component);

	/* TODO: sorting function? */
}

inline void drawSceneObjects(render::Drawer* d, std::vector<Renderer> o) {
	for (size_t i = 0; i < o.size(); i++)
	{
		glm::mat4 m{};
		btTransform transform;
		o[i].motionState->getGraphicsTransform(&m);
		for (size_t j = 0; j < o[i].mesh->submeshes.size(); j++)
		{
			d->draw(o[i].mesh, &o[i].mesh->submeshes[j], &(d->registeredMaterials[o[i].mesh->submeshes[j].materialIndex]), m, false);
		}
	}
}

void Scene::updateShadowMap(render::Light* l) {
	//l.updateTransform(glm::mat4(1.0), drawer->currentFrame);
	std::vector<VkClearValue> clearValues;
	clearValues.resize(1);
	clearValues[0].depthStencil = { 0.0, 0 };

	drawer->beginPass(drawer->shadowFrames[drawer->currentSwapchainIndex], drawer->shadowPass, clearValues, {512, 512});
	drawer->bindShadowPassPipeline();
	//drawSceneObjects(drawer, renderedScene);
	for (size_t i = 0; i < Scene::renderedScene.size(); i++)
	{
		glm::mat4 m{};
		btTransform transform;
		renderedScene[i].motionState->getGraphicsTransform(&m);
		for (size_t j = 0; j < Scene::renderedScene[i].mesh->submeshes.size(); j++)
		{
			drawer->draw(Scene::renderedScene[i].mesh, &Scene::renderedScene[i].mesh->submeshes[j], &(drawer->registeredMaterials[Scene::renderedScene[i].mesh->submeshes[j].materialIndex]), m, false);
		}
	}
	drawer->endPass();
};


void Scene::drawObjects() {
	std::vector<glm::mat4> lightViews;
	std::vector<double> fovs;
	glm::mat4 testView = glm::lookAt(glm::vec3(6.0, 3.0, 6.0), glm::vec3(0, 0, 0), glm::vec3(0, 1, 0));

	lightViews.push_back(testView);
	fovs.push_back(90);

	drawer->beginFrame(mainCameraView, 90, lightViews, fovs);
	/*for (size_t i = 0; i < drawer->registeredLights.size(); i++)
	{
		updateShadowMap(drawer->registeredLights[i]);
	}*/
	updateShadowMap(drawer->mainLight);
	std::vector<VkClearValue> clearValues;
	clearValues.resize(2);
	clearValues[0].color = { 0.01f, 0.01f, 0.01f, 1.0f };
	clearValues[1].depthStencil = {1.0, 0};
	drawer->beginPass(clearValues);
	for (size_t i = 0; i < Scene::renderedScene.size(); i++)
	{
		glm::mat4 m{};
		btTransform transform;
		renderedScene[i].motionState->getGraphicsTransform(&m);
		for (size_t j = 0; j < Scene::renderedScene[i].mesh->submeshes.size(); j++)
		{
			drawer->draw(Scene::renderedScene[i].mesh, &Scene::renderedScene[i].mesh->submeshes[j], &(drawer->registeredMaterials[Scene::renderedScene[i].mesh->submeshes[j].materialIndex]), m, true);
		}
	}
	drawer->endPass();
	drawer->submitDraws();
	drawer->endFrame();
	//
	//
}

void Scene::changeView(glm::mat4 view) {
	mainCameraView = view;
}

Scene::~Scene() {
	for (size_t i = 0; i < renderedScene.size(); i++)
	{
		delete renderedScene[i].motionState;
	}
	delete physics.world;
	delete physics.solver;
	delete physics.pairCache;
	delete physics.dispatcher;
	delete physics.defaultConfig;
}