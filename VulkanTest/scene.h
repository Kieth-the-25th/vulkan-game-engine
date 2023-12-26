#pragma once

#include <vector>
#include "render.h"
#include "threading.h"
#include "btBulletDynamicsCommon.h"
#include "btBulletCollisionCommon.h"
#include "bulletCustom.h"

class SyncFunc;
class AsyncFunc;

struct Renderer {
	btCustomMotionState* motionState;
	render::Mesh* mesh;
	uint8_t count;

	Renderer(render::Mesh* mesh, uint8_t count, btCustomMotionState* motionState);
};

class Scene {
public:
	Scene(Threading* threading, render::Drawer* drawer);

	void step();
	btRigidBody* addRigidBody(btRigidBody::btRigidBodyConstructionInfo info);
	void attachRenderer(Renderer component);
	void addSyncObject(SyncFunc* o);
	void addAsyncObject(AsyncFunc* o);
	void updateShadowMap(render::Light* l);
	void changeView(glm::mat4 view);
	void drawObjects();

	~Scene();

	struct Physics {
		btDefaultCollisionConfiguration* defaultConfig;

		btCollisionDispatcher* dispatcher;

		btBroadphaseInterface* pairCache;

		btSequentialImpulseConstraintSolver* solver;

		btDiscreteDynamicsWorld* world;

		btAlignedObjectArray<btCollisionShape*> collisionShapes;
	};

private:
	float timestep;

	glm::vec3 up;

	glm::mat4 mainCameraView;
	double fov;

	render::Drawer* drawer;

	Physics physics;
	Threading* threading;

	std::vector<Renderer> renderedScene;
	std::vector<SyncFunc*> synchronizedObjects;
	std::vector<AsyncFunc*> threadedObjects;
};

typedef void (*AsyncMessage)(Scene* scene);

class AsyncFunc {
public:
	bool active = true;
	virtual void update(const std::vector<AsyncMessage> messageList, const Scene* scene) {};
};

class debugCollisionSendMessage : AsyncFunc {
	int attachedRigidbody;
	void update(const std::vector<AsyncMessage> messageList, const Scene* scene, Scene::Physics physics) {
		Callback cb{};
		cb.messageList = &messageList;
		physics.world->contactTest((physics.world->getCollisionObjectArray()[attachedRigidbody]), cb);
	}

	struct Callback : btCollisionWorld::ContactResultCallback {
		const std::vector<AsyncMessage>* messageList;

		btScalar addSingleResult(btManifoldPoint& cp, const btCollisionObjectWrapper* colObj0Wrap, int partId0, int index0, const btCollisionObjectWrapper* colObj1Wrap, int partId1, int index1) {
			std::cout << "bruh" << "\n";
		}
	};
};

class SyncFunc {
public:
	bool active = true;
	virtual void update(Scene::Physics physics) { std::cout << "placeholder" << "\n"; };
};

class debugLogPosition : public SyncFunc {
public:
	int attachedRigidbody;
	void update(Scene::Physics physics) {
		//std::cout << "transform:" << physics.world->getCollisionObjectArray()[attachedRigidbody]->getWorldTransform().getOrigin().z() << "\n";
	}
};