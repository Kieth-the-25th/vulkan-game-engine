#pragma once

#include <GLFW/glfw3.h>
#include <cstdlib>
#include <algorithm>
#include <optional>
#include <functional>

class Input
{
public:
	struct ControlMode {
		GLFWkeyfun keyboardControl;
		GLFWcursorposfun cursorControl;
		int mouseInputMode;

		ControlMode(GLFWkeyfun func, GLFWcursorposfun cursorFunc, int mouseInputMode);
	};

	Input(GLFWwindow* window);
	~Input();

	void setCallback(GLFWkeyfun callback);
	void addControlMode(ControlMode mode);
	void setControlMode(int mode);
private:
	GLFWwindow* window;
	int activeControlMode = 0;
	std::vector<ControlMode> controlModes;
};

