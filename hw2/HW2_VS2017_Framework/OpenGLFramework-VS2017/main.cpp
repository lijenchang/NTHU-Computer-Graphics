#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <math.h>
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include "textfile.h"

#include "Vectors.h"
#include "Matrices.h"
#define TINYOBJLOADER_IMPLEMENTATION
#include "tiny_obj_loader.h"

#ifndef max
# define max(a,b) (((a)>(b))?(a):(b))
# define min(a,b) (((a)<(b))?(a):(b))
#endif

#define PI 3.1415926

using namespace std;

// Default window size
const int WINDOW_WIDTH = 800;
const int WINDOW_HEIGHT = 800;

bool mouse_pressed = false;
int starting_press_x = -1;
int starting_press_y = -1;

enum TransMode
{
	GeoTranslation = 0,
	GeoRotation = 1,
	GeoScaling = 2,
	LightEdit = 3,
	ShininessEdit = 4,
};

/* ---------- [HW2] Lighting ---------- */
struct LightingAttrib
{
	Vector3 position;
	Vector3 ambient;
	Vector3 diffuse;
	Vector3 specular;
	Vector3 spot_direction;
	float spot_exponent;
	float spot_cutoff;
	float shininess;
	float constant_attenuation;
	float linear_attenuation;
	float quadratic_attenuation;
};
LightingAttrib lighting_attrib[3];   // for directional, point, and spot lights

struct UniformLightingAttrib
{
	GLuint position;
	GLuint ambient;
	GLuint diffuse;
	GLuint specular;
	GLuint spot_direction;
	GLuint spot_exponent;
	GLuint spot_cutoff;
	GLuint shininess;
	GLuint constant_attenuation;
	GLuint linear_attenuation;
	GLuint quadratic_attenuation;
};

struct UniformPhongMaterial
{
	GLuint Ka;
	GLuint Kd;
	GLuint Ks;
};

int cur_lighting_mode = 0;     // 0: directional, 1: point, 2: spot

int cur_window_width = WINDOW_WIDTH;
int cur_window_height = WINDOW_HEIGHT;
/* ------------------------------------ */

struct Uniform
{
	GLint iLocMVP;
	/* ---------- [HW2] Lighting ---------- */
	GLint iLocMV;     // require ((MV)^(-1))^T for Normal Transformation
	GLint iLocViewingMatrix;     // require V for transforming light position from world space to view space
	GLint iLocLightingMode;
	GLint iLocIsPerPixel;
	UniformLightingAttrib iLocLightingAttrib[3];     // for directional, point, and spot lights
	UniformPhongMaterial iLocPhongMaterial;
	/* ------------------------------------ */
};
Uniform uniform;

vector<string> filenames; // .obj filename list

struct PhongMaterial
{
	Vector3 Ka;
	Vector3 Kd;
	Vector3 Ks;
};

typedef struct
{
	GLuint vao;
	GLuint vbo;
	GLuint vboTex;
	GLuint ebo;
	GLuint p_color;
	int vertex_count;
	GLuint p_normal;
	PhongMaterial material;
	int indexCount;
	GLuint m_texture;
} Shape;

struct model
{
	Vector3 position = Vector3(0, 0, 0);
	Vector3 scale = Vector3(1, 1, 1);
	Vector3 rotation = Vector3(0, 0, 0);	// Euler form

	vector<Shape> shapes;
};
vector<model> models;

struct camera
{
	Vector3 position;
	Vector3 center;
	Vector3 up_vector;
};
camera main_camera;

struct project_setting
{
	GLfloat nearClip, farClip;
	GLfloat fovy;
	GLfloat aspect;
	GLfloat left, right, top, bottom;
};
project_setting proj;

TransMode cur_trans_mode = GeoTranslation;

Matrix4 view_matrix;
Matrix4 project_matrix;

int cur_idx = 0; // represent which model should be rendered now


static GLvoid Normalize(GLfloat v[3])
{
	GLfloat l;

	l = (GLfloat)sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
	v[0] /= l;
	v[1] /= l;
	v[2] /= l;
}

static GLvoid Cross(GLfloat u[3], GLfloat v[3], GLfloat n[3])
{

	n[0] = u[1] * v[2] - u[2] * v[1];
	n[1] = u[2] * v[0] - u[0] * v[2];
	n[2] = u[0] * v[1] - u[1] * v[0];
}


// [TODO] given a translation vector then output a Matrix4 (Translation Matrix)
Matrix4 translate(Vector3 vec)
{
	Matrix4 mat;

	mat = Matrix4(
		1, 0, 0, vec.x,
		0, 1, 0, vec.y,
		0, 0, 1, vec.z,
		0, 0, 0, 1
	);

	return mat;
}

// [TODO] given a scaling vector then output a Matrix4 (Scaling Matrix)
Matrix4 scaling(Vector3 vec)
{
	Matrix4 mat;

	mat = Matrix4(
		vec.x, 0, 0, 0,
		0, vec.y, 0, 0,
		0, 0, vec.z, 0,
		0, 0, 0, 1
	);

	return mat;
}


// [TODO] given a float value then ouput a rotation matrix alone axis-X (rotate alone axis-X)
Matrix4 rotateX(GLfloat val)
{
	Matrix4 mat;

	val = val * PI / 180;   // convert from degrees into radians

	mat = Matrix4(
		1, 0, 0, 0,
		0, cos(val), -sin(val), 0,
		0, sin(val), cos(val), 0,
		0, 0, 0, 1
	);

	return mat;
}

// [TODO] given a float value then ouput a rotation matrix alone axis-Y (rotate alone axis-Y)
Matrix4 rotateY(GLfloat val)
{
	Matrix4 mat;

	val = val * PI / 180;   // convert from degrees into radians

	mat = Matrix4(
		cos(val), 0, sin(val), 0,
		0, 1, 0, 0,
		-sin(val), 0, cos(val), 0,
		0, 0, 0, 1
	);

	return mat;
}

// [TODO] given a float value then ouput a rotation matrix alone axis-Z (rotate alone axis-Z)
Matrix4 rotateZ(GLfloat val)
{
	Matrix4 mat;

	val = val * PI / 180;   // convert from degrees into radians

	mat = Matrix4(
		cos(val), -sin(val), 0, 0,
		sin(val), cos(val), 0, 0,
		0, 0, 1, 0,
		0, 0, 0, 1
	);

	return mat;
}

Matrix4 rotate(Vector3 vec)
{
	return rotateX(vec.x)*rotateY(vec.y)*rotateZ(vec.z);
}

// [TODO] compute viewing matrix accroding to the setting of main_camera
void setViewingMatrix()
{
	// view_matrix[...] = ...
	Matrix4 R, T;
	Vector3 Rx, Ry, Rz;
	// Find a translation matrix to translate eye position to the origin
	T = Matrix4(
		1, 0, 0, -main_camera.position.x,
		0, 1, 0, -main_camera.position.y,
		0, 0, 1, -main_camera.position.z,
		0, 0, 0, 1
	);

	// Find an orthonormal basis with respect to the new space
	Rz = -(main_camera.center - main_camera.position);
	Rz.normalize();

	Rx = (main_camera.center - main_camera.position).cross(main_camera.up_vector);
	Rx.normalize();

	Ry = Rz.cross(Rx);

	R = Matrix4(
		Rx.x, Rx.y, Rx.z, 0,
		Ry.x, Ry.y, Ry.z, 0,
		Rz.x, Rz.y, Rz.z, 0,
		0, 0, 0, 1
	);

	view_matrix = R * T;
}

// [TODO] compute persepective projection matrix
void setPerspective()
{
	GLfloat fov = proj.fovy * PI / 180;   // convert from degrees into radians
	GLfloat f = 1 / tan(fov / 2);         // f = cot(fov / 2)
	GLfloat m22 = (proj.farClip + proj.nearClip) / (proj.nearClip - proj.farClip);
	GLfloat m23 = (2 * proj.farClip * proj.nearClip) / (proj.nearClip - proj.farClip);

	if (proj.aspect >= 1) {
		project_matrix = Matrix4(
			f / proj.aspect, 0, 0, 0,
			0, f, 0, 0,
			0, 0, m22, m23,
			0, 0, -1, 0
		);
	}
	else {
		project_matrix = Matrix4(
			f, 0, 0, 0,
			0, f * proj.aspect, 0, 0,
			0, 0, m22, m23,
			0, 0, -1, 0
		);
	}
}

void setGLMatrix(GLfloat* glm, Matrix4& m) {
	glm[0] = m[0];  glm[4] = m[1];   glm[8] = m[2];    glm[12] = m[3];
	glm[1] = m[4];  glm[5] = m[5];   glm[9] = m[6];    glm[13] = m[7];
	glm[2] = m[8];  glm[6] = m[9];   glm[10] = m[10];   glm[14] = m[11];
	glm[3] = m[12];  glm[7] = m[13];  glm[11] = m[14];   glm[15] = m[15];
}

// Vertex buffers
GLuint VAO, VBO;

// Call back function for window reshape
void ChangeSize(GLFWwindow* window, int width, int height)
{
	glViewport(0, 0, width, height);
	// [TODO] change your aspect ratio
	proj.aspect = (GLfloat)width / (GLfloat)height;
	setPerspective();
	cur_window_width = width;
	cur_window_height = height;
}


// [HW2] use glUniform to send lighting attributes to vertex shader
void UpdateLighting()
{
	// i = 0: directional light, i = 1: point light, i = 2: spot light
	for (int i = 0; i < 3; ++i) {
		// for 3 light sources
		glUniform3f(uniform.iLocLightingAttrib[i].position, lighting_attrib[i].position.x, lighting_attrib[i].position.y, lighting_attrib[i].position.z);
		glUniform3f(uniform.iLocLightingAttrib[i].ambient, lighting_attrib[i].ambient.x, lighting_attrib[i].ambient.y, lighting_attrib[i].ambient.z);
		glUniform3f(uniform.iLocLightingAttrib[i].diffuse, lighting_attrib[i].diffuse.x, lighting_attrib[i].diffuse.y, lighting_attrib[i].diffuse.z);
		glUniform3f(uniform.iLocLightingAttrib[i].specular, lighting_attrib[i].specular.x, lighting_attrib[i].specular.y, lighting_attrib[i].specular.z);
		glUniform1f(uniform.iLocLightingAttrib[i].shininess, lighting_attrib[i].shininess);
		// for point & spot lights
		if (i != 0) {
			glUniform1f(uniform.iLocLightingAttrib[i].constant_attenuation, lighting_attrib[i].constant_attenuation);
			glUniform1f(uniform.iLocLightingAttrib[i].linear_attenuation, lighting_attrib[i].linear_attenuation);
			glUniform1f(uniform.iLocLightingAttrib[i].quadratic_attenuation, lighting_attrib[i].quadratic_attenuation);
		}
		// for spot light only
		if (i == 2) {
			glUniform3f(uniform.iLocLightingAttrib[i].spot_direction, lighting_attrib[i].spot_direction.x, lighting_attrib[i].spot_direction.y, lighting_attrib[i].spot_direction.z);
			glUniform1f(uniform.iLocLightingAttrib[i].spot_exponent, lighting_attrib[i].spot_exponent);
			glUniform1f(uniform.iLocLightingAttrib[i].spot_cutoff, lighting_attrib[i].spot_cutoff);
		}
	}
}


// Render function for display rendering
void RenderScene(void) {
	// clear canvas
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

	Matrix4 T, R, S;
	// [TODO] update translation, rotation and scaling
	T = translate(models[cur_idx].position);
	R = rotate(models[cur_idx].rotation);
	S = scaling(models[cur_idx].scale);

	Matrix4 MVP, MV;
	GLfloat mvp[16], mv[16], v[16];

	// [TODO] multiply all the matrix
	MVP = project_matrix * view_matrix * T * R * S;
	MV = view_matrix * T * R * S;

	// row-major ---> column-major
	setGLMatrix(mvp, MVP);
	setGLMatrix(mv, MV);
	setGLMatrix(v, view_matrix);

	// [HW2] use glUniform to send mvp, mv, and lighting attrib to vertex shader
	glUniformMatrix4fv(uniform.iLocMVP, 1, GL_FALSE, mvp);
	glUniformMatrix4fv(uniform.iLocMV, 1, GL_FALSE, mv);
	glUniformMatrix4fv(uniform.iLocViewingMatrix, 1, GL_FALSE, v);
	glUniform1i(uniform.iLocLightingMode, cur_lighting_mode);
	UpdateLighting();

	for (int i = 0; i < models[cur_idx].shapes.size(); i++)
	{
		// [HW2] use glUniform to send material info (Ka, Kd, Ks) to vertex shader
		glUniform3f(uniform.iLocPhongMaterial.Ka, models[cur_idx].shapes[i].material.Ka.x, models[cur_idx].shapes[i].material.Ka.y, models[cur_idx].shapes[i].material.Ka.z);
		glUniform3f(uniform.iLocPhongMaterial.Kd, models[cur_idx].shapes[i].material.Kd.x, models[cur_idx].shapes[i].material.Kd.y, models[cur_idx].shapes[i].material.Kd.z);
		glUniform3f(uniform.iLocPhongMaterial.Ks, models[cur_idx].shapes[i].material.Ks.x, models[cur_idx].shapes[i].material.Ks.y, models[cur_idx].shapes[i].material.Ks.z);

		// [HW2] set glViewport and draw twice (side-by-side)
		glUniform1i(uniform.iLocIsPerPixel, 0);
		glViewport(0, 0, cur_window_width / 2, cur_window_height);
		glBindVertexArray(models[cur_idx].shapes[i].vao);
		glDrawArrays(GL_TRIANGLES, 0, models[cur_idx].shapes[i].vertex_count);

		glUniform1i(uniform.iLocIsPerPixel, 1);
		glViewport(cur_window_width / 2, 0, cur_window_width / 2, cur_window_height);
		glDrawArrays(GL_TRIANGLES, 0, models[cur_idx].shapes[i].vertex_count);
	}

}


void KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
	// [TODO] Call back function for keyboard
	if (key == GLFW_KEY_Z && action == GLFW_PRESS)
		cur_idx = (cur_idx == 0) ? models.size() - 1 : cur_idx - 1;
	if (key == GLFW_KEY_X && action == GLFW_PRESS)
		cur_idx = (cur_idx + 1) % models.size();
	if (key == GLFW_KEY_T && action == GLFW_PRESS)
		cur_trans_mode = GeoTranslation;
	if (key == GLFW_KEY_S && action == GLFW_PRESS)
		cur_trans_mode = GeoScaling;
	if (key == GLFW_KEY_R && action == GLFW_PRESS)
		cur_trans_mode = GeoRotation;
	if (key == GLFW_KEY_L && action == GLFW_PRESS) {
		cur_lighting_mode = (cur_lighting_mode + 1) % 3;
		if (cur_lighting_mode == 0) cout << "\nCurrent Light Source: Directional Light\n";
		else if (cur_lighting_mode == 1) cout << "\nCurrent Light Source: Point Light\n";
		else if (cur_lighting_mode == 2) cout << "\nCurrent Light Source: Spot Light\n";
	}
	if (key == GLFW_KEY_K && action == GLFW_PRESS)
		cur_trans_mode = LightEdit;
	if (key == GLFW_KEY_J && action == GLFW_PRESS)
		cur_trans_mode = ShininessEdit;
}

void scroll_callback(GLFWwindow* window, double xoffset, double yoffset)
{
	// [TODO] scroll up positive, otherwise it would be negtive
	// The magic numbers are used to control the proper unit of each update
	switch (cur_trans_mode)
	{
	case GeoTranslation:
		models[cur_idx].position.z += (yoffset * 0.02);   // scroll up -> move the object closer to camera (+z direction)
		break;
	case GeoScaling:
		models[cur_idx].scale.z += (yoffset * 0.02);   // scroll up -> scale up in z-axis
		break;
	case GeoRotation:
		models[cur_idx].rotation.z += yoffset;   // scroll up -> increase rotation degrees in z-axis
		break;
	case LightEdit:
		if (cur_lighting_mode == 0 || cur_lighting_mode == 1) {
			lighting_attrib[cur_lighting_mode].diffuse += yoffset * Vector3(0.05, 0.05, 0.05);
			// clamp diffuse intensity to be >= 0
			lighting_attrib[cur_lighting_mode].diffuse.x = max(lighting_attrib[cur_lighting_mode].diffuse.x, 0.0);
			lighting_attrib[cur_lighting_mode].diffuse.y = max(lighting_attrib[cur_lighting_mode].diffuse.y, 0.0);
			lighting_attrib[cur_lighting_mode].diffuse.z = max(lighting_attrib[cur_lighting_mode].diffuse.z, 0.0);
		}
		else {
			lighting_attrib[cur_lighting_mode].spot_cutoff += yoffset;
			// clamp spotlight's cutoff angle into the range [0.0, 180.0]
			lighting_attrib[cur_lighting_mode].spot_cutoff = min(max(lighting_attrib[cur_lighting_mode].spot_cutoff, 0.0), 180.0);
		}
		break;
	case ShininessEdit:
		// apply to all lighting modes
		for (int i = 0; i < 3; ++i) {
			lighting_attrib[i].shininess += yoffset * 10;
			// make sure shininess >= 0
			lighting_attrib[i].shininess = max(lighting_attrib[i].shininess, 0);
		}
		break;
	default:
		break;
	}
}

void mouse_button_callback(GLFWwindow* window, int button, int action, int mods)
{
	// [TODO] mouse press callback function
	if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS) mouse_pressed = true;
	if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_RELEASE) mouse_pressed = false;
}

static void cursor_pos_callback(GLFWwindow* window, double xpos, double ypos)
{
	// [TODO] cursor position callback function
	if (starting_press_x == -1) starting_press_x = xpos;
	if (starting_press_y == -1) starting_press_y = ypos;

	// Note: cursor position is measured in screen coordinates relative to the top-left corner of the window!!
	// The magic numbers are used to control the proper unit of each update
	if (mouse_pressed) {
		switch (cur_trans_mode)
		{
		case GeoTranslation:
			models[cur_idx].position.x += (xpos - starting_press_x) * 0.002;
			models[cur_idx].position.y -= (ypos - starting_press_y) * 0.002;
			break;
		case GeoScaling:
			models[cur_idx].scale.x += (xpos - starting_press_x) * 0.002;
			models[cur_idx].scale.y -= (ypos - starting_press_y) * 0.002;
			break;
		case GeoRotation:
			// mouse drag up -> modify by positive delta_y -> counter-clockwise rotation about x-axis
			models[cur_idx].rotation.x -= (ypos - starting_press_y);
			// mouse drag right -> modify by negative delta_x -> clockwise rotation about y-axis
			models[cur_idx].rotation.y -= (xpos - starting_press_x);
			break;
		case LightEdit:
			lighting_attrib[cur_lighting_mode].position.x += (xpos - starting_press_x) * 0.002;
			lighting_attrib[cur_lighting_mode].position.y -= (ypos - starting_press_y) * 0.002;
			break;
		default:
			break;
		}
	}

	// Update cursor position no matter the mouse is pressed or not
	starting_press_x = xpos;
	starting_press_y = ypos;
}

void setShaders()
{
	GLuint v, f, p;
	char *vs = NULL;
	char *fs = NULL;

	v = glCreateShader(GL_VERTEX_SHADER);
	f = glCreateShader(GL_FRAGMENT_SHADER);

	vs = textFileRead("shader.vs");
	fs = textFileRead("shader.fs");

	glShaderSource(v, 1, (const GLchar**)&vs, NULL);
	glShaderSource(f, 1, (const GLchar**)&fs, NULL);

	free(vs);
	free(fs);

	GLint success;
	char infoLog[1000];
	// compile vertex shader
	glCompileShader(v);
	// check for shader compile errors
	glGetShaderiv(v, GL_COMPILE_STATUS, &success);
	if (!success)
	{
		glGetShaderInfoLog(v, 1000, NULL, infoLog);
		std::cout << "ERROR: VERTEX SHADER COMPILATION FAILED\n" << infoLog << std::endl;
	}

	// compile fragment shader
	glCompileShader(f);
	// check for shader compile errors
	glGetShaderiv(f, GL_COMPILE_STATUS, &success);
	if (!success)
	{
		glGetShaderInfoLog(f, 1000, NULL, infoLog);
		std::cout << "ERROR: FRAGMENT SHADER COMPILATION FAILED\n" << infoLog << std::endl;
	}

	// create program object
	p = glCreateProgram();

	// attach shaders to program object
	glAttachShader(p, f);
	glAttachShader(p, v);

	// link program
	glLinkProgram(p);
	// check for linking errors
	glGetProgramiv(p, GL_LINK_STATUS, &success);
	if (!success) {
		glGetProgramInfoLog(p, 1000, NULL, infoLog);
		std::cout << "ERROR: SHADER PROGRAM LINKING FAILED\n" << infoLog << std::endl;
	}

	glDeleteShader(v);
	glDeleteShader(f);

	uniform.iLocMVP = glGetUniformLocation(p, "mvp");

	/* ---------- [HW2] Query the locations (in shader) of uniform variables needed for lighting ---------- */
	uniform.iLocMV = glGetUniformLocation(p, "mv");     // API: glGetUniformLocation(program, name_of_uniform_variable_in_shader)
	uniform.iLocViewingMatrix = glGetUniformLocation(p, "viewing_matrix");
	uniform.iLocLightingMode = glGetUniformLocation(p, "lighting_mode");
	uniform.iLocIsPerPixel = glGetUniformLocation(p, "is_perpixel");

	uniform.iLocPhongMaterial.Ka = glGetUniformLocation(p, "material.Ka");
	uniform.iLocPhongMaterial.Kd = glGetUniformLocation(p, "material.Kd");
	uniform.iLocPhongMaterial.Ks = glGetUniformLocation(p, "material.Ks");

	uniform.iLocLightingAttrib[0].position = glGetUniformLocation(p, "lighting_attrib[0].position");
	uniform.iLocLightingAttrib[0].ambient = glGetUniformLocation(p, "lighting_attrib[0].ambient");
	uniform.iLocLightingAttrib[0].diffuse = glGetUniformLocation(p, "lighting_attrib[0].diffuse");
	uniform.iLocLightingAttrib[0].specular = glGetUniformLocation(p, "lighting_attrib[0].specular");
	uniform.iLocLightingAttrib[0].shininess = glGetUniformLocation(p, "lighting_attrib[0].shininess");

	uniform.iLocLightingAttrib[1].position = glGetUniformLocation(p, "lighting_attrib[1].position");
	uniform.iLocLightingAttrib[1].ambient = glGetUniformLocation(p, "lighting_attrib[1].ambient");
	uniform.iLocLightingAttrib[1].diffuse = glGetUniformLocation(p, "lighting_attrib[1].diffuse");
	uniform.iLocLightingAttrib[1].specular = glGetUniformLocation(p, "lighting_attrib[1].specular");
	uniform.iLocLightingAttrib[1].shininess = glGetUniformLocation(p, "lighting_attrib[1].shininess");
	uniform.iLocLightingAttrib[1].constant_attenuation = glGetUniformLocation(p, "lighting_attrib[1].constant_attenuation");
	uniform.iLocLightingAttrib[1].linear_attenuation = glGetUniformLocation(p, "lighting_attrib[1].linear_attenuation");
	uniform.iLocLightingAttrib[1].quadratic_attenuation = glGetUniformLocation(p, "lighting_attrib[1].quadratic_attenuation");

	uniform.iLocLightingAttrib[2].position = glGetUniformLocation(p, "lighting_attrib[2].position");
	uniform.iLocLightingAttrib[2].ambient = glGetUniformLocation(p, "lighting_attrib[2].ambient");
	uniform.iLocLightingAttrib[2].diffuse = glGetUniformLocation(p, "lighting_attrib[2].diffuse");
	uniform.iLocLightingAttrib[2].specular = glGetUniformLocation(p, "lighting_attrib[2].specular");
	uniform.iLocLightingAttrib[2].shininess = glGetUniformLocation(p, "lighting_attrib[2].shininess");
	uniform.iLocLightingAttrib[2].constant_attenuation = glGetUniformLocation(p, "lighting_attrib[2].constant_attenuation");
	uniform.iLocLightingAttrib[2].linear_attenuation = glGetUniformLocation(p, "lighting_attrib[2].linear_attenuation");
	uniform.iLocLightingAttrib[2].quadratic_attenuation = glGetUniformLocation(p, "lighting_attrib[2].quadratic_attenuation");
	uniform.iLocLightingAttrib[2].spot_direction = glGetUniformLocation(p, "lighting_attrib[2].spot_direction");
	uniform.iLocLightingAttrib[2].spot_exponent = glGetUniformLocation(p, "lighting_attrib[2].spot_exponent");
	uniform.iLocLightingAttrib[2].spot_cutoff = glGetUniformLocation(p, "lighting_attrib[2].spot_cutoff");
	/* ---------------------------------------------------------------------------------------- */

	if (success)
		glUseProgram(p);
	else
	{
		system("pause");
		exit(123);
	}
}

void normalization(tinyobj::attrib_t* attrib, vector<GLfloat>& vertices, vector<GLfloat>& colors, vector<GLfloat>& normals, tinyobj::shape_t* shape)
{
	vector<float> xVector, yVector, zVector;
	float minX = 10000, maxX = -10000, minY = 10000, maxY = -10000, minZ = 10000, maxZ = -10000;

	// find out min and max value of X, Y and Z axis
	for (int i = 0; i < attrib->vertices.size(); i++)
	{
		//maxs = max(maxs, attrib->vertices.at(i));
		if (i % 3 == 0)
		{

			xVector.push_back(attrib->vertices.at(i));

			if (attrib->vertices.at(i) < minX)
			{
				minX = attrib->vertices.at(i);
			}

			if (attrib->vertices.at(i) > maxX)
			{
				maxX = attrib->vertices.at(i);
			}
		}
		else if (i % 3 == 1)
		{
			yVector.push_back(attrib->vertices.at(i));

			if (attrib->vertices.at(i) < minY)
			{
				minY = attrib->vertices.at(i);
			}

			if (attrib->vertices.at(i) > maxY)
			{
				maxY = attrib->vertices.at(i);
			}
		}
		else if (i % 3 == 2)
		{
			zVector.push_back(attrib->vertices.at(i));

			if (attrib->vertices.at(i) < minZ)
			{
				minZ = attrib->vertices.at(i);
			}

			if (attrib->vertices.at(i) > maxZ)
			{
				maxZ = attrib->vertices.at(i);
			}
		}
	}

	float offsetX = (maxX + minX) / 2;
	float offsetY = (maxY + minY) / 2;
	float offsetZ = (maxZ + minZ) / 2;

	for (int i = 0; i < attrib->vertices.size(); i++)
	{
		if (offsetX != 0 && i % 3 == 0)
		{
			attrib->vertices.at(i) = attrib->vertices.at(i) - offsetX;
		}
		else if (offsetY != 0 && i % 3 == 1)
		{
			attrib->vertices.at(i) = attrib->vertices.at(i) - offsetY;
		}
		else if (offsetZ != 0 && i % 3 == 2)
		{
			attrib->vertices.at(i) = attrib->vertices.at(i) - offsetZ;
		}
	}

	float greatestAxis = maxX - minX;
	float distanceOfYAxis = maxY - minY;
	float distanceOfZAxis = maxZ - minZ;

	if (distanceOfYAxis > greatestAxis)
	{
		greatestAxis = distanceOfYAxis;
	}

	if (distanceOfZAxis > greatestAxis)
	{
		greatestAxis = distanceOfZAxis;
	}

	float scale = greatestAxis / 2;

	for (int i = 0; i < attrib->vertices.size(); i++)
	{
		//std::cout << i << " = " << (double)(attrib.vertices.at(i) / greatestAxis) << std::endl;
		attrib->vertices.at(i) = attrib->vertices.at(i) / scale;
	}
	size_t index_offset = 0;
	for (size_t f = 0; f < shape->mesh.num_face_vertices.size(); f++) {
		int fv = shape->mesh.num_face_vertices[f];

		// Loop over vertices in the face.
		for (size_t v = 0; v < fv; v++) {
			// access to vertex
			tinyobj::index_t idx = shape->mesh.indices[index_offset + v];
			vertices.push_back(attrib->vertices[3 * idx.vertex_index + 0]);
			vertices.push_back(attrib->vertices[3 * idx.vertex_index + 1]);
			vertices.push_back(attrib->vertices[3 * idx.vertex_index + 2]);
			// Optional: vertex colors
			colors.push_back(attrib->colors[3 * idx.vertex_index + 0]);
			colors.push_back(attrib->colors[3 * idx.vertex_index + 1]);
			colors.push_back(attrib->colors[3 * idx.vertex_index + 2]);
			// Optional: vertex normals
			if (idx.normal_index >= 0) {
				normals.push_back(attrib->normals[3 * idx.normal_index + 0]);
				normals.push_back(attrib->normals[3 * idx.normal_index + 1]);
				normals.push_back(attrib->normals[3 * idx.normal_index + 2]);
			}
		}
		index_offset += fv;
	}
}

string GetBaseDir(const string& filepath) {
	if (filepath.find_last_of("/\\") != std::string::npos)
		return filepath.substr(0, filepath.find_last_of("/\\"));
	return "";
}

void LoadModels(string model_path)
{
	vector<tinyobj::shape_t> shapes;
	vector<tinyobj::material_t> materials;
	tinyobj::attrib_t attrib;
	vector<GLfloat> vertices;
	vector<GLfloat> colors;
	vector<GLfloat> normals;

	string err;
	string warn;

	string base_dir = GetBaseDir(model_path); // handle .mtl with relative path

#ifdef _WIN32
	base_dir += "\\";
#else
	base_dir += "/";
#endif

	bool ret = tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, model_path.c_str(), base_dir.c_str());

	if (!warn.empty()) {
		cout << warn << std::endl;
	}

	if (!err.empty()) {
		cerr << err << std::endl;
	}

	if (!ret) {
		exit(1);
	}

	printf("Load Models Success ! Shapes size %d Material size %d\n", int(shapes.size()), int(materials.size()));
	model tmp_model;

	vector<PhongMaterial> allMaterial;
	for (int i = 0; i < materials.size(); i++)
	{
		PhongMaterial material;
		material.Ka = Vector3(materials[i].ambient[0], materials[i].ambient[1], materials[i].ambient[2]);
		material.Kd = Vector3(materials[i].diffuse[0], materials[i].diffuse[1], materials[i].diffuse[2]);
		material.Ks = Vector3(materials[i].specular[0], materials[i].specular[1], materials[i].specular[2]);
		allMaterial.push_back(material);
	}

	for (int i = 0; i < shapes.size(); i++)
	{

		vertices.clear();
		colors.clear();
		normals.clear();
		normalization(&attrib, vertices, colors, normals, &shapes[i]);
		// printf("Vertices size: %d", vertices.size() / 3);

		Shape tmp_shape;
		glGenVertexArrays(1, &tmp_shape.vao);
		glBindVertexArray(tmp_shape.vao);

		glGenBuffers(1, &tmp_shape.vbo);
		glBindBuffer(GL_ARRAY_BUFFER, tmp_shape.vbo);
		glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(GL_FLOAT), &vertices.at(0), GL_STATIC_DRAW);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, 0);
		tmp_shape.vertex_count = vertices.size() / 3;

		glGenBuffers(1, &tmp_shape.p_color);
		glBindBuffer(GL_ARRAY_BUFFER, tmp_shape.p_color);
		glBufferData(GL_ARRAY_BUFFER, colors.size() * sizeof(GL_FLOAT), &colors.at(0), GL_STATIC_DRAW);
		glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 0, 0);

		glGenBuffers(1, &tmp_shape.p_normal);
		glBindBuffer(GL_ARRAY_BUFFER, tmp_shape.p_normal);
		glBufferData(GL_ARRAY_BUFFER, normals.size() * sizeof(GL_FLOAT), &normals.at(0), GL_STATIC_DRAW);
		glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 0, 0);

		glEnableVertexAttribArray(0);
		glEnableVertexAttribArray(1);
		glEnableVertexAttribArray(2);

		// not support per face material, use material of first face
		if (allMaterial.size() > 0)
			tmp_shape.material = allMaterial[shapes[i].mesh.material_ids[0]];
		tmp_model.shapes.push_back(tmp_shape);
	}
	shapes.clear();
	materials.clear();
	models.push_back(tmp_model);
}

void initParameter()
{
	// [TODO] Setup some parameters if you need
	proj.left = -1;
	proj.right = 1;
	proj.top = 1;
	proj.bottom = -1;
	proj.nearClip = 0.001;
	proj.farClip = 100.0;
	proj.fovy = 80;
	proj.aspect = (float)WINDOW_WIDTH / (float)WINDOW_HEIGHT;

	main_camera.position = Vector3(0.0f, 0.0f, 2.0f);
	main_camera.center = Vector3(0.0f, 0.0f, 0.0f);
	main_camera.up_vector = Vector3(0.0f, 1.0f, 0.0f);

	setViewingMatrix();
	setPerspective();	//set default projection matrix as perspective matrix

	/* ---------- [HW2] Setup initial lighting attributes ---------- */
	// directional light
	lighting_attrib[0].position = Vector3(1.0f, 1.0f, 1.0f);

	// point light
	lighting_attrib[1].position = Vector3(0.0f, 2.0f, 1.0f);
	lighting_attrib[1].constant_attenuation = 0.01;
	lighting_attrib[1].linear_attenuation = 0.8;
	lighting_attrib[1].quadratic_attenuation = 0.1;

	// spot light
	lighting_attrib[2].position = Vector3(0.0f, 0.0f, 2.0f);
	lighting_attrib[2].constant_attenuation = 0.05;
	lighting_attrib[2].linear_attenuation = 0.3;
	lighting_attrib[2].quadratic_attenuation = 0.6;
	lighting_attrib[2].spot_direction = Vector3(0.0f, 0.0f, -1.0f);
	lighting_attrib[2].spot_exponent = 50.0;
	lighting_attrib[2].spot_cutoff = 30.0;

	// common attributes
	for (int i = 0; i < 3; ++i) {
		lighting_attrib[i].ambient = Vector3(0.15f, 0.15f, 0.15f);
		lighting_attrib[i].diffuse = Vector3(1.0f, 1.0f, 1.0f);
		lighting_attrib[i].specular = Vector3(1.0f, 1.0f, 1.0f);
		lighting_attrib[i].shininess = 64.0;
	}
	/* ------------------------------------------------------------- */
}

void setupRC()
{
	// setup shaders
	setShaders();
	initParameter();

	// OpenGL States and Values
	glClearColor(0.2, 0.2, 0.2, 1.0);
	vector<string> model_list{ "../NormalModels/bunny5KN.obj", "../NormalModels/dragon10KN.obj", "../NormalModels/lucy25KN.obj", "../NormalModels/teapot4KN.obj", "../NormalModels/dolphinN.obj" };
	// [TODO] Load five model at here
	for (int i = 0; i < model_list.size(); ++i)
		LoadModels(model_list[i]);
}

void glPrintContextInfo(bool printExtension)
{
	cout << "GL_VENDOR = " << (const char*)glGetString(GL_VENDOR) << endl;
	cout << "GL_RENDERER = " << (const char*)glGetString(GL_RENDERER) << endl;
	cout << "GL_VERSION = " << (const char*)glGetString(GL_VERSION) << endl;
	cout << "GL_SHADING_LANGUAGE_VERSION = " << (const char*)glGetString(GL_SHADING_LANGUAGE_VERSION) << endl;
	if (printExtension)
	{
		GLint numExt;
		glGetIntegerv(GL_NUM_EXTENSIONS, &numExt);
		cout << "GL_EXTENSIONS =" << endl;
		for (GLint i = 0; i < numExt; i++)
		{
			cout << "\t" << (const char*)glGetStringi(GL_EXTENSIONS, i) << endl;
		}
	}
}


int main(int argc, char **argv)
{
	// initial glfw
	glfwInit();
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

#ifdef __APPLE__
	glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE); // fix compilation on OS X
#endif


	// create window
	GLFWwindow* window = glfwCreateWindow(WINDOW_WIDTH, WINDOW_HEIGHT, "Student ID HW2", NULL, NULL);
	if (window == NULL)
	{
		std::cout << "Failed to create GLFW window" << std::endl;
		glfwTerminate();
		return -1;
	}
	glfwMakeContextCurrent(window);


	// load OpenGL function pointer
	if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
	{
		std::cout << "Failed to initialize GLAD" << std::endl;
		return -1;
	}

	// register glfw callback functions
	glfwSetKeyCallback(window, KeyCallback);
	glfwSetScrollCallback(window, scroll_callback);
	glfwSetMouseButtonCallback(window, mouse_button_callback);
	glfwSetCursorPosCallback(window, cursor_pos_callback);

	glfwSetFramebufferSizeCallback(window, ChangeSize);
	glEnable(GL_DEPTH_TEST);
	// Setup render context
	setupRC();

	// main loop
	while (!glfwWindowShouldClose(window))
	{
		// render
		RenderScene();

		// swap buffer from back to front
		glfwSwapBuffers(window);

		// Poll input event
		glfwPollEvents();
	}

	// just for compatibiliy purposes
	return 0;
}
