#pragma once

#include <vector>
#include "render.h"
#include "btBulletDynamicsCommon.h"
#include "btBulletCollisionCommon.h"
#include "bulletCustom.h"

class SyncObject;
class AsyncObject;

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
	void addSyncObject(SyncObject* o);
	void addAsyncObject(AsyncObject* o);
	void updateShadowMap(render::Light* l);
	void drawObjects(glm::mat4 cameraView);

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

	//render::Camera mainCamera;

	render::Drawer* drawer;

	Physics physics;

	std::vector<Renderer> renderedScene;
	std::vector<SyncObject*> synchronizedObjects;
	std::vector<AsyncObject*> threadedObjects;
};

typedef void (*AsyncMessage)(Scene* scene);

class AsyncObject {
public:
	bool active = true;
	virtual void update(const std::vector<AsyncMessage> messageList, const Scene* scene) {};
};

class debugCollisionSendMessage : AsyncObject {
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

class SyncObject {
public:
	bool active = true;
	virtual void update(Scene::Physics physics) { std::cout << "placeholder" << "\n"; };
};

class debugLogPosition : public SyncObject {
public:
	int attachedRigidbody;
	void update(Scene::Physics physics) {
		std::cout << "transform:" << physics.world->getCollisionObjectArray()[attachedRigidbody]->getWorldTransform().getOrigin().z() << "\n";
	}
};