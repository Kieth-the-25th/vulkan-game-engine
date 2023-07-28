#include "input.h"

Input::ControlMode::ControlMode(GLFWkeyfun func, GLFWcursorposfun cursorFunc, int mouseInputMode) {
	keyboardControl = func;
	cursorControl = cursorFunc;
	this->mouseInputMode = mouseInputMode;
}

Input::Input(GLFWwindow* window) {
	this->window = window;
}

Input::~Input() {

}

void Input::setCallback(GLFWkeyfun callback) {
	glfwSetKeyCallback(window, callback);
}

void Input::addControlMode(ControlMode mode) {
	controlModes.push_back(mode);
}

void Input::setControlMode(int mode) {
	activeControlMode = mode;
	glfwSetKeyCallback(window, controlModes[mode].keyboardControl);
	glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
	glfwSetInputMode(window, controlModes[mode].mouseInputMode, GLFW_TRUE);
	glfwSetCursorPosCallback(window, controlModes[mode].cursorControl);
}