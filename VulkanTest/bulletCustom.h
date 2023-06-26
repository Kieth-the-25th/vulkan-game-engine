#pragma once
#include <glm/glm.hpp>
#include "BulletCollision/CollisionShapes/btBoxShape.h"
#include "btBulletDynamicsCommon.h"
#include "btBulletCollisionCommon.h"
#include <iostream>
#include <stdexcept>

class btBoxCollider2 : public btBoxShape {
public:
	btBoxCollider2(btVector3 halfExtents);
	btVector3 scale;
};

class btCustomMotionState : public btMotionState {
public:
	glm::mat4 scale;
	btTransform m_graphicsWorldTrans;
	btTransform m_centerOfMassOffset;
	btTransform m_startWorldTrans;

	btCustomMotionState(const btTransform startTransform, const btTransform COMoffset = btTransform::getIdentity(), const glm::mat4 scale = glm::mat4(1.0));
	~btCustomMotionState();

	void getWorldTransform(btTransform& centerOfMassWorldTrans) const;
	void setWorldTransform(const btTransform& centerOfMassWorldTrans);
	void getGraphicsTransform(glm::mat4* matrix);
};