#pragma once

#include <vector>
#include "render.h"
#include "btBulletDynamicsCommon.h"
#include "btBulletCollisionCommon.h"
#include "bulletCustom.h"

/* renderer component */

struct Renderer {
	btCustomMotionState* motionState;
	render::Mesh* mesh;
	uint8_t count;

	Renderer(render::Mesh* mesh, uint8_t count, btCustomMotionState* motionState);
};

class Scene {
public:
	Scene(render::Drawer* drawer);

	void step();
	void addRigidBody(btRigidBody::btRigidBodyConstructionInfo info);
	void attachRenderer(Renderer component);
	void updateShadowMap(render::Light* l);
	void drawObjects(glm::mat4 cameraView);

	~Scene();

private:
	float timestep;

	glm::vec3 up;

	//render::Camera mainCamera;

	render::Drawer* drawer;

	btDefaultCollisionConfiguration* defaultConfig;

	btCollisionDispatcher* dispatcher;

	btBroadphaseInterface* pairCache;

	btSequentialImpulseConstraintSolver* solver;

	btDiscreteDynamicsWorld* world;
	
	btAlignedObjectArray<btCollisionShape*> collisionShapes;
	std::vector<Renderer> renderedScene;
};