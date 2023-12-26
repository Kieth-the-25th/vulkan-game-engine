#pragma once

#include <vector>
#include <string>
#include <set>

#include <chrono>
#include <iostream>
#include <fstream>
#include <stdexcept>
#include <algorithm>
#include <optional>
#include <random>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "btBulletDynamicsCommon.h"
#include "btBulletCollisionCommon.h"
#include "bulletCustom.h"

#include "threading.h"
#include "input.h"
#include "scene.h"
#include "time.h"

union rendererState;
union physicsState;

union playerControl {
public:
	glm::vec3 movement = glm::vec3(0, 0, 0);
	//std::chrono::steady_clock::time_point jumpCooldown = std::chrono::steady_clock::now();
	//std::chrono::steady_clock::duration cooldownTime = std::chrono::steady_clock::duration(1);

	void keyMovement(GLFWwindow* window, int key, int scancode, int action, int mods) {
		if (action == GLFW_PRESS) {
			if (key == GLFW_KEY_W) {
				movement.z = 1;
			}
			else if (key == GLFW_KEY_S) {
				movement.z = -1;
			}
			if (key == GLFW_KEY_SPACE) {
				movement.y = 1;
			}
			else {
				movement.y = 0;
			}
			if (key == GLFW_KEY_A) {
				movement.x = 1;
			}
			else if (key == GLFW_KEY_D) {
				movement.x = -1;
			}
		}
		else if (action == GLFW_RELEASE) {
			if (key == GLFW_KEY_W) {
				movement.z = 0;
			}
			else if (key == GLFW_KEY_S) {
				movement.z = 0;
			}
			if (key == GLFW_KEY_SPACE) {
				movement.y = 0;
			}
			if (key == GLFW_KEY_A) {
				movement.x = 0;
			}
			else if (key == GLFW_KEY_D) {
				movement.x = 0;
			}
		}
		else if (action == GLFW_REPEAT) {

		}
	};
};