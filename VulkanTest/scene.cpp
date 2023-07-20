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
#include "bulletCustom.h"

#include <stdio.h>

Renderer::Renderer(render::Mesh* mesh, uint8_t count, btCustomMotionState* motionState) {
	this->motionState = motionState;
	this->mesh = mesh;
	this->count = count;
}

Scene::Scene(render::Drawer* drawer) {
	Scene::timestep = static_cast<float>(1) / 60;

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
	world->getCollisionObjectArray()[0]->forceActivationState(4);
	world->stepSimulation(1);
}

void Scene::addRigidBody(btRigidBody::btRigidBodyConstructionInfo info) {
	btRigidBody* body = new btRigidBody(info);
	world->addRigidBody(body);
}

void Scene::attachRenderer(Renderer component) {
	renderedScene.push_back(component);

	/* TODO: sorting function? */
}

void Scene::updateShadowMap(render::Light* l) {
	//l.updateTransform(glm::mat4(1.0), drawer->currentFrame);
	std::vector<VkClearValue> clearValues;
	clearValues.resize(1);
	clearValues[0].depthStencil = { 0.0, 0 };
	drawer->beginPass(l->frames[drawer->currentSwapchainIndex], drawer->shadowPass, clearValues, {512, 512});
	drawer->bindShadowPassPipeline();
	for (size_t i = 0; i < Scene::renderedScene.size(); i++)
	{
		glm::mat4 m{};
		btTransform transform;
		renderedScene[i].motionState->getGraphicsTransform(&m);
		//renderedScene[i].motionState->getWorldTransform(transform);
		//transform.getOpenGLMatrix(reinterpret_cast<btScalar*>(&m));

		/*std::cout << m[0][0] << " " << m[1][0] << " " << m[2][0] << " " << m[3][0] << "\n";
		std::cout << m[0][1] << " " << m[1][1] << " " << m[2][1] << " " << m[3][1] << "\n";
		std::cout << m[0][2] << " " << m[1][2] << " " << m[2][2] << " " << m[3][2] << "\n";
		std::cout << m[0][3] << " " << m[1][3] << " " << m[2][3] << " " << m[3][3] << "\n";*/

		//std::cout << "submesh count:" << (int)Scene::renderedScene[i].mesh->submeshes.size() << "\n";
		for (size_t j = 0; j < Scene::renderedScene[i].mesh->submeshes.size(); j++)
		{
			drawer->draw(Scene::renderedScene[i].mesh, &Scene::renderedScene[i].mesh->submeshes[j], &(drawer->registeredMaterials[Scene::renderedScene[i].mesh->submeshes[j].materialIndex]), m, false);
		}
	}
	drawer->endPass();
};


void Scene::drawObjects(glm::vec3 cameraPos) {
	std::vector<glm::mat4> lightViews;
	std::vector<double> fovs;
	glm::mat4 testView = glm::lookAt(glm::vec3(4.0, 0, 10.0), glm::vec3(0, 0, 0), glm::vec3(1, 0, 0));

	lightViews.push_back(testView);
	fovs.push_back(90);

	drawer->beginFrame(lookAt(glm::vec3(6.0, 6.0, 10.0), glm::vec3(0, 0, 0), glm::vec3(0, 0, 1)), 90, lightViews, fovs);
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
		//renderedScene[i].motionState->getWorldTransform(transform);
		//transform.getOpenGLMatrix(reinterpret_cast<btScalar*>(&m));

		/*std::cout << m[0][0] << " " << m[1][0] << " " << m[2][0] << " " << m[3][0] << "\n";
		std::cout << m[0][1] << " " << m[1][1] << " " << m[2][1] << " " << m[3][1] << "\n";
		std::cout << m[0][2] << " " << m[1][2] << " " << m[2][2] << " " << m[3][2] << "\n";
		std::cout << m[0][3] << " " << m[1][3] << " " << m[2][3] << " " << m[3][3] << "\n";*/

		//std::cout << "submesh count:" << (int)Scene::renderedScene[i].mesh->submeshes.size() << "\n";
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

Scene::~Scene() {
	for (size_t i = 0; i < renderedScene.size(); i++)
	{
		delete renderedScene[i].motionState;
	}
	delete world;
	delete solver;
	delete pairCache;
	delete dispatcher;
	delete defaultConfig;
}