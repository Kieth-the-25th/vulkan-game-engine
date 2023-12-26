#include "bulletCustom.h"

btBoxCollider2::btBoxCollider2(btVector3 halfExtents) : btBoxShape(halfExtents) {
	scale = halfExtents;
};

btCustomMotionState::btCustomMotionState(const btTransform startTransform, const btTransform COMoffset, glm::mat4 scale) :
	m_graphicsWorldTrans(startTransform),
	m_centerOfMassOffset(COMoffset),
	m_startWorldTrans(startTransform),
	scale(scale)
{
	this->scale[3][3] = 1;
};

btCustomMotionState::~btCustomMotionState() {

};

void btCustomMotionState::getWorldTransform(btTransform& centerOfMassWorldTrans) const
{
	centerOfMassWorldTrans = m_graphicsWorldTrans * m_centerOfMassOffset.inverse();
}

void btCustomMotionState::setWorldTransform(const btTransform& centerOfMassWorldTrans)
{
	m_graphicsWorldTrans = centerOfMassWorldTrans * m_centerOfMassOffset;
}

void btCustomMotionState::getGraphicsTransform(glm::mat4* matrix) {
	*matrix = scale;
	glm::mat4 physicsTransform;
	btTransform COMadjustedTransform = m_graphicsWorldTrans * m_centerOfMassOffset.inverse();
	COMadjustedTransform.getOpenGLMatrix(reinterpret_cast<btScalar*>(&physicsTransform));
	*matrix *= physicsTransform;
}