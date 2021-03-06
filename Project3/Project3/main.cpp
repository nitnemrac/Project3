/************************************************************************************

Authors     :   Bradley Austin Davis <bdavis@saintandreas.org>
Copyright   :   Copyright Brad Davis. All Rights reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.

************************************************************************************/


#include <iostream>
#include <memory>
#include <exception>
#include <algorithm>
#include <Windows.h>

#include "shader.h"
#include "Box.h"
#include "Quad.h"
#include "Pyramid.h"

#define __STDC_FORMAT_MACROS 1

#define FAIL(X) throw std::runtime_error(X)

///////////////////////////////////////////////////////////////////////////////
//
// GLM is a C++ math library meant to mirror the syntax of GLSL 
//

#include <glm/glm.hpp>
#include <glm/gtc/noise.hpp>
#include <glm/gtx/rotate_vector.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>

// Import the most commonly used types into the default namespace
using glm::ivec3;
using glm::ivec2;
using glm::uvec2;
using glm::mat3;
using glm::mat4;
using glm::vec2;
using glm::vec3;
using glm::vec4;
using glm::quat;

using namespace std;

///////////////////////////////////////////////////////////////////////////////
//
// GLEW gives cross platform access to OpenGL 3.x+ functionality.  
//

#include <GL/glew.h>

bool checkFramebufferStatus(GLenum target = GL_FRAMEBUFFER) {
	GLuint status = glCheckFramebufferStatus(target);
	switch (status) {
	case GL_FRAMEBUFFER_COMPLETE:
		return true;
		break;

	case GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT:
		std::cerr << "framebuffer incomplete attachment" << std::endl;
		break;

	case GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT:
		std::cerr << "framebuffer missing attachment" << std::endl;
		break;

	case GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER:
		std::cerr << "framebuffer incomplete draw buffer" << std::endl;
		break;

	case GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER:
		std::cerr << "framebuffer incomplete read buffer" << std::endl;
		break;

	case GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE:
		std::cerr << "framebuffer incomplete multisample" << std::endl;
		break;

	case GL_FRAMEBUFFER_INCOMPLETE_LAYER_TARGETS:
		std::cerr << "framebuffer incomplete layer targets" << std::endl;
		break;

	case GL_FRAMEBUFFER_UNSUPPORTED:
		std::cerr << "framebuffer unsupported internal format or image" << std::endl;
		break;

	default:
		std::cerr << "other framebuffer error" << std::endl;
		break;
	}

	return false;
}

bool checkGlError() {
	GLenum error = glGetError();
	if (!error) {
		return false;
	}
	else {
		switch (error) {
		case GL_INVALID_ENUM:
			std::cerr << ": An unacceptable value is specified for an enumerated argument.The offending command is ignored and has no other side effect than to set the error flag.";
			break;
		case GL_INVALID_VALUE:
			std::cerr << ": A numeric argument is out of range.The offending command is ignored and has no other side effect than to set the error flag";
			break;
		case GL_INVALID_OPERATION:
			std::cerr << ": The specified operation is not allowed in the current state.The offending command is ignored and has no other side effect than to set the error flag..";
			break;
		case GL_INVALID_FRAMEBUFFER_OPERATION:
			std::cerr << ": The framebuffer object is not complete.The offending command is ignored and has no other side effect than to set the error flag.";
			break;
		case GL_OUT_OF_MEMORY:
			std::cerr << ": There is not enough memory left to execute the command.The state of the GL is undefined, except for the state of the error flags, after this error is recorded.";
			break;
		case GL_STACK_UNDERFLOW:
			std::cerr << ": An attempt has been made to perform an operation that would cause an internal stack to underflow.";
			break;
		case GL_STACK_OVERFLOW:
			std::cerr << ": An attempt has been made to perform an operation that would cause an internal stack to overflow.";
			break;
		}
		return true;
	}
}

void glDebugCallbackHandler(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar *msg, GLvoid* data) {
	OutputDebugStringA(msg);
	std::cout << "debug call: " << msg << std::endl;
}

//////////////////////////////////////////////////////////////////////
//
// GLFW provides cross platform window creation
//

#include <GLFW/glfw3.h>

namespace glfw {
	inline GLFWwindow * createWindow(const uvec2 & size, const ivec2 & position = ivec2(INT_MIN)) {
		GLFWwindow * window = glfwCreateWindow(size.x, size.y, "glfw", nullptr, nullptr);
		if (!window) {
			FAIL("Unable to create rendering window");
		}
		if ((position.x > INT_MIN) && (position.y > INT_MIN)) {
			glfwSetWindowPos(window, position.x, position.y);
		}
		return window;
	}
}

// A class to encapsulate using GLFW to handle input and render a scene
class GlfwApp {

protected:
	uvec2 windowSize;
	ivec2 windowPosition;
	GLFWwindow * window{ nullptr };
	unsigned int frame{ 0 };

public:
	GlfwApp() {
		// Initialize the GLFW system for creating and positioning windows
		if (!glfwInit()) {
			FAIL("Failed to initialize GLFW");
		}
		glfwSetErrorCallback(ErrorCallback);
	}

	virtual ~GlfwApp() {
		if (nullptr != window) {
			glfwDestroyWindow(window);
		}
		glfwTerminate();
	}

	virtual int run() {
		preCreate();

		window = createRenderingTarget(windowSize, windowPosition);

		if (!window) {
			std::cout << "Unable to create OpenGL window" << std::endl;
			return -1;
		}

		postCreate();

		initGl();

		while (!glfwWindowShouldClose(window)) {
			++frame;
			glfwPollEvents();
			update();
			draw();
			finishFrame();
		}

		shutdownGl();

		return 0;
	}


protected:
	virtual GLFWwindow * createRenderingTarget(uvec2 & size, ivec2 & pos) = 0;

	virtual void draw() = 0;

	void preCreate() {
		glfwWindowHint(GLFW_DEPTH_BITS, 16);
		glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
		glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
		glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
		glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, true);
	}


	void postCreate() {
		glfwSetWindowUserPointer(window, this);
		glfwSetKeyCallback(window, KeyCallback);
		glfwSetMouseButtonCallback(window, MouseButtonCallback);
		glfwMakeContextCurrent(window);

		// Initialize the OpenGL bindings
		// For some reason we have to set this experminetal flag to properly
		// init GLEW if we use a core context.
		glewExperimental = GL_TRUE;
		if (0 != glewInit()) {
			FAIL("Failed to initialize GLEW");
		}
		glGetError();

		if (GLEW_KHR_debug) {
			GLint v;
			glGetIntegerv(GL_CONTEXT_FLAGS, &v);
			if (v & GL_CONTEXT_FLAG_DEBUG_BIT) {
				//glDebugMessageCallback(glDebugCallbackHandler, this);
			}
		}
	}

	virtual void initGl() {
	}

	virtual void shutdownGl() {
	}

	virtual void finishFrame() {
		glfwSwapBuffers(window);
	}

	virtual void destroyWindow() {
		glfwSetKeyCallback(window, nullptr);
		glfwSetMouseButtonCallback(window, nullptr);
		glfwDestroyWindow(window);
	}

	virtual void onKey(int key, int scancode, int action, int mods) {
		if (GLFW_PRESS != action) {
			return;
		}

		switch (key) {
		case GLFW_KEY_ESCAPE:
			glfwSetWindowShouldClose(window, 1);
			return;
		}
	}

	virtual void update() {}

	virtual void onMouseButton(int button, int action, int mods) {}

protected:
	virtual void viewport(const ivec2 & pos, const uvec2 & size) {
		glViewport(pos.x, pos.y, size.x, size.y);
	}

private:

	static void KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods) {
		GlfwApp * instance = (GlfwApp *)glfwGetWindowUserPointer(window);
		instance->onKey(key, scancode, action, mods);
	}

	static void MouseButtonCallback(GLFWwindow* window, int button, int action, int mods) {
		GlfwApp * instance = (GlfwApp *)glfwGetWindowUserPointer(window);
		instance->onMouseButton(button, action, mods);
	}

	static void ErrorCallback(int error, const char* description) {
		FAIL(description);
	}
};

//////////////////////////////////////////////////////////////////////
//
// The Oculus VR C API provides access to information about the HMD
//

#include <OVR_CAPI.h>
#include <OVR_CAPI_GL.h>

namespace ovr {

	// Convenience method for looping over each eye with a lambda
	template <typename Function>
	inline void for_each_eye(Function function) {
		for (ovrEyeType eye = ovrEyeType::ovrEye_Left;
			eye < ovrEyeType::ovrEye_Count;
			eye = static_cast<ovrEyeType>(eye + 1)) {
			function(eye);
		}
	}

	inline mat4 toGlm(const ovrMatrix4f & om) {
		return glm::transpose(glm::make_mat4(&om.M[0][0]));
	}

	inline mat4 toGlm(const ovrFovPort & fovport, float nearPlane = 0.01f, float farPlane = 10000.0f) {
		return toGlm(ovrMatrix4f_Projection(fovport, nearPlane, farPlane, true));
	}

	inline vec3 toGlm(const ovrVector3f & ov) {
		return glm::make_vec3(&ov.x);
	}

	inline vec2 toGlm(const ovrVector2f & ov) {
		return glm::make_vec2(&ov.x);
	}

	inline uvec2 toGlm(const ovrSizei & ov) {
		return uvec2(ov.w, ov.h);
	}

	inline quat toGlm(const ovrQuatf & oq) {
		return glm::make_quat(&oq.x);
	}

	inline mat4 toGlm(const ovrPosef & op) {
		mat4 orientation = glm::mat4_cast(toGlm(op.Orientation));
		mat4 translation = glm::translate(mat4(), ovr::toGlm(op.Position));
		return translation * orientation;
	}

	inline ovrMatrix4f fromGlm(const mat4 & m) {
		ovrMatrix4f result;
		mat4 transposed(glm::transpose(m));
		memcpy(result.M, &(transposed[0][0]), sizeof(float) * 16);
		return result;
	}

	inline ovrVector3f fromGlm(const vec3 & v) {
		ovrVector3f result;
		result.x = v.x;
		result.y = v.y;
		result.z = v.z;
		return result;
	}

	inline ovrVector2f fromGlm(const vec2 & v) {
		ovrVector2f result;
		result.x = v.x;
		result.y = v.y;
		return result;
	}

	inline ovrSizei fromGlm(const uvec2 & v) {
		ovrSizei result;
		result.w = v.x;
		result.h = v.y;
		return result;
	}

	inline ovrQuatf fromGlm(const quat & q) {
		ovrQuatf result;
		result.x = q.x;
		result.y = q.y;
		result.z = q.z;
		result.w = q.w;
		return result;
	}
}

class RiftManagerApp {
protected:
	ovrSession _session;
	ovrHmdDesc _hmdDesc;
	ovrGraphicsLuid _luid;

public:
	RiftManagerApp() {
		if (!OVR_SUCCESS(ovr_Create(&_session, &_luid))) {
			FAIL("Unable to create HMD session");
		}

		_hmdDesc = ovr_GetHmdDesc(_session);
	}

	~RiftManagerApp() {
		ovr_Destroy(_session);
		_session = nullptr;
	}
};

class RiftApp : public GlfwApp, public RiftManagerApp {
public:

private:
	GLuint _fbo{ 0 };
	GLuint _depthBuffer{ 0 };
	ovrTextureSwapChain _eyeTexture;

	GLuint _mirrorFbo{ 0 };
	ovrMirrorTexture _mirrorTexture;

	ovrEyeRenderDesc _eyeRenderDescs[2];

	mat4 _eyeProjections[2];

	ovrLayerEyeFov _sceneLayer;
	ovrViewScaleDesc _viewScaleDesc;

	uvec2 _renderTargetSize;
	uvec2 _mirrorSize;

	int viewSelector = 0;
	int trackingSelector = 0;
	int displaySelector = 0;

	ovrPosef lastPoses[2];
	float originalIODL;
	float originalIODR;

	vector<char*> viewmodes = { "Stereo", "Mono", "Left only", "Right only" };
	vector<char*> trackmodes = { "Full Tracking", "No Tracking", "Position", "Orientation"};
	vector<char*> displaymodes = { "Calibration", "Panorama", "Both" };
	bool A_down = false;
	bool B_down = false;
	bool X_down = false;

public:

	RiftApp() {
		using namespace ovr;
		_viewScaleDesc.HmdSpaceToWorldScaleInMeters = 1.0f;

		memset(&_sceneLayer, 0, sizeof(ovrLayerEyeFov));
		_sceneLayer.Header.Type = ovrLayerType_EyeFov;
		_sceneLayer.Header.Flags = ovrLayerFlag_TextureOriginAtBottomLeft;

		ovr::for_each_eye([&](ovrEyeType eye) {
			ovrEyeRenderDesc& erd = _eyeRenderDescs[eye] = ovr_GetRenderDesc(_session, eye, _hmdDesc.DefaultEyeFov[eye]);
			ovrMatrix4f ovrPerspectiveProjection =
				ovrMatrix4f_Projection(erd.Fov, 0.01f, 1000.0f, ovrProjection_ClipRangeOpenGL);
			_eyeProjections[eye] = ovr::toGlm(ovrPerspectiveProjection);
			_viewScaleDesc.HmdToEyeOffset[eye] = erd.HmdToEyeOffset; // cse190: adjust the eye separation here - need to use 3D vector from central point on Rift for each eye

			ovrFovPort & fov = _sceneLayer.Fov[eye] = _eyeRenderDescs[eye].Fov;
			auto eyeSize = ovr_GetFovTextureSize(_session, eye, fov, 1.0f);
			_sceneLayer.Viewport[eye].Size = eyeSize;
			_sceneLayer.Viewport[eye].Pos = { (int)_renderTargetSize.x, 0 };

			_renderTargetSize.y = std::max(_renderTargetSize.y, (uint32_t)eyeSize.h);
			_renderTargetSize.x += eyeSize.w;
		});
		originalIODL = _viewScaleDesc.HmdToEyeOffset[ovrEye_Left].x;
		originalIODR = _viewScaleDesc.HmdToEyeOffset[ovrEye_Right].x;

		// Make the on screen window 1/4 the resolution of the render target
		_mirrorSize = _renderTargetSize;
		_mirrorSize /= 4;
	}

protected:
	GLFWwindow * createRenderingTarget(uvec2 & outSize, ivec2 & outPosition) override {
		return glfw::createWindow(_mirrorSize);
	}

	void initGl() override {
		GlfwApp::initGl();

		// Disable the v-sync for buffer swap
		glfwSwapInterval(0);

		ovrTextureSwapChainDesc desc = {};
		desc.Type = ovrTexture_2D;
		desc.ArraySize = 1;
		desc.Width = _renderTargetSize.x;
		desc.Height = _renderTargetSize.y;
		desc.MipLevels = 1;
		desc.Format = OVR_FORMAT_R8G8B8A8_UNORM_SRGB;
		desc.SampleCount = 1;
		desc.StaticImage = ovrFalse;
		ovrResult result = ovr_CreateTextureSwapChainGL(_session, &desc, &_eyeTexture);
		_sceneLayer.ColorTexture[0] = _eyeTexture;
		if (!OVR_SUCCESS(result)) {
			FAIL("Failed to create swap textures");
		}

		int length = 0;
		result = ovr_GetTextureSwapChainLength(_session, _eyeTexture, &length);
		if (!OVR_SUCCESS(result) || !length) {
			FAIL("Unable to count swap chain textures");
		}
		for (int i = 0; i < length; ++i) {
			GLuint chainTexId;
			ovr_GetTextureSwapChainBufferGL(_session, _eyeTexture, i, &chainTexId);
			glBindTexture(GL_TEXTURE_2D, chainTexId);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		}
		glBindTexture(GL_TEXTURE_2D, 0);

		// Set up the framebuffer object
		glGenFramebuffers(1, &_fbo);
		glGenRenderbuffers(1, &_depthBuffer);
		glBindFramebuffer(GL_DRAW_FRAMEBUFFER, _fbo);
		glBindRenderbuffer(GL_RENDERBUFFER, _depthBuffer);
		glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT16, _renderTargetSize.x, _renderTargetSize.y);
		glBindRenderbuffer(GL_RENDERBUFFER, 0);
		glFramebufferRenderbuffer(GL_DRAW_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, _depthBuffer);
		glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);

		ovrMirrorTextureDesc mirrorDesc;
		memset(&mirrorDesc, 0, sizeof(mirrorDesc));
		mirrorDesc.Format = OVR_FORMAT_R8G8B8A8_UNORM_SRGB;
		mirrorDesc.Width = _mirrorSize.x;
		mirrorDesc.Height = _mirrorSize.y;
		if (!OVR_SUCCESS(ovr_CreateMirrorTextureGL(_session, &mirrorDesc, &_mirrorTexture))) {
			FAIL("Could not create mirror texture");
		}
		glGenFramebuffers(1, &_mirrorFbo);
	}

	void onKey(int key, int scancode, int action, int mods) override {
		if (GLFW_PRESS == action) switch (key) {
		case GLFW_KEY_R:
			ovr_RecenterTrackingOrigin(_session);
			return;
		}

		GlfwApp::onKey(key, scancode, action, mods);
	}

	void draw() final override {
		ovrPosef eyePoses[2];
		ovr_GetEyePoses(_session, frame, true, _viewScaleDesc.HmdToEyeOffset, eyePoses, &_sceneLayer.SensorSampleTime);
		
		int curIndex;
		ovr_GetTextureSwapChainCurrentIndex(_session, _eyeTexture, &curIndex);
		GLuint curTexId;
		ovr_GetTextureSwapChainBufferGL(_session, _eyeTexture, curIndex, &curTexId);
		glBindFramebuffer(GL_DRAW_FRAMEBUFFER, _fbo);
		glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, curTexId, 0);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);


		ovr::for_each_eye([&](ovrEyeType eye) {
			
			const auto& vp = _sceneLayer.Viewport[eye];
			glViewport(vp.Pos.x, vp.Pos.y, vp.Size.w, vp.Size.h);
			_sceneLayer.RenderPose[eye] = eyePoses[eye];

			renderScene(_eyeProjections[eye], ovr::toGlm(eyePoses[eye]), eye, displaySelector, _fbo, _sceneLayer, windowSize);
			
		});
		glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, 0, 0);
		glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
		ovr_CommitTextureSwapChain(_session, _eyeTexture);
		ovrLayerHeader* headerList = &_sceneLayer.Header;
		ovr_SubmitFrame(_session, frame, &_viewScaleDesc, &headerList, 1);

		GLuint mirrorTextureId;
		ovr_GetMirrorTextureBufferGL(_session, _mirrorTexture, &mirrorTextureId);
		glBindFramebuffer(GL_READ_FRAMEBUFFER, _mirrorFbo);
		glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, mirrorTextureId, 0);
		glBlitFramebuffer(0, 0, _mirrorSize.x, _mirrorSize.y, 0, _mirrorSize.y, _mirrorSize.x, 0, GL_COLOR_BUFFER_BIT, GL_NEAREST);
		glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
	}

	void update() final override
	{
		//ovrInputState inputState;
		/*if (OVR_SUCCESS(ovr_GetInputState(_session, ovrControllerType_Touch, &inputState)))
		{			
			// On B press, change head tracking mode
			if (inputState.Buttons & ovrButton_B && !B_down) {
				B_down = true;
				trackingSelector = (trackingSelector + 1)%2;	
				printf("Tracking mode: ", trackingSelector);
			}
			if (!(inputState.Buttons & ovrButton_B)) {
				B_down = false;
			}
		}*/
	}

	virtual void renderScene(const glm::mat4 & projection, const glm::mat4 & headPose, ovrEyeType eye, int displayMode, GLuint hmd_fbo, ovrLayerEyeFov _sceneLayer, uvec2 windowSize) = 0;
};

///////////////////////////////////////////////////////////////////////////////////////////
//										LOAD PPM
///////////////////////////////////////////////////////////////////////////////////////////

//! Load a ppm file from disk.
// @input filename The location of the PPM file.  If the file is not found, an error message
//		will be printed and this function will return 0
// @input width This will be modified to contain the width of the loaded image, or 0 if file not found
// @input height This will be modified to contain the height of the loaded image, or 0 if file not found
//
// @return Returns the RGB pixel data as interleaved unsigned chars (R0 G0 B0 R1 G1 B1 R2 G2 B2 .... etc) or 0 if an error ocured

unsigned char* loadPPM(const char* filename, int& width, int& height)
{
	const int BUFSIZE = 128;
	FILE* fp;
	unsigned int read;
	unsigned char* rawData;
	char buf[3][BUFSIZE];
	char* retval_fgets;
	size_t retval_sscanf;

	if ((fp = fopen(filename, "rb")) == NULL)
	{
		std::cerr << "error reading ppm file, could not locate " << filename << std::endl;
		width = 0;
		height = 0;
		return 0;
	}

	// Read magic number:
	retval_fgets = fgets(buf[0], BUFSIZE, fp);

	// Read width and height:
	do
	{
		retval_fgets = fgets(buf[0], BUFSIZE, fp);
	} while (buf[0][0] == '#');
	retval_sscanf = sscanf(buf[0], "%s %s", buf[1], buf[2]);
	width = atoi(buf[1]);
	height = atoi(buf[2]);

	// Read maxval:
	do
	{
		retval_fgets = fgets(buf[0], BUFSIZE, fp);
	} while (buf[0][0] == '#');

	// Read image data:
	rawData = new unsigned char[width * height * 3];
	read = (unsigned int) fread(rawData, width * height * 3, 1, fp);
	fclose(fp);
	if (read != 1)
	{
		std::cerr << "error parsing ppm file, incomplete data" << std::endl;
		delete[] rawData;
		width = 0;
		height = 0;

		return 0;
	}

	return rawData;
}

//////////////////////////////////////////////////////////////////////
//
// The remainder of this code is specific to the scene we want to 
// render.  I use oglplus to render an array of cubes, but your 
// application would perform whatever rendering you want
//


//////////////////////////////////////////////////////////////////////
//
// OGLplus is a set of wrapper classes for giving OpenGL a more object
// oriented interface
//
#define OGLPLUS_USE_GLCOREARB_H 0
#define OGLPLUS_USE_GLEW 1
#define OGLPLUS_USE_BOOST_CONFIG 0
#define OGLPLUS_NO_SITE_CONFIG 1
#define OGLPLUS_LOW_PROFILE 1
#define LEFT 0
#define RIGHT 1

#pragma warning( disable : 4068 4244 4267 4065)
#include <oglplus/config/basic.hpp>
#include <oglplus/config/gl.hpp>
#include <oglplus/all.hpp>
#include <oglplus/interop/glm.hpp>
#include <oglplus/bound/texture.hpp>
#include <oglplus/bound/framebuffer.hpp>
#include <oglplus/bound/renderbuffer.hpp>
#include <oglplus/bound/buffer.hpp>
#include <oglplus/shapes/cube.hpp>
#include <oglplus/shapes/wrapper.hpp>
#pragma warning( default : 4068 4244 4267 4065)


namespace Attribute {
	enum {
		Position = 0,
		TexCoord0 = 1,
		Normal = 2,
		Color = 3,
		TexCoord1 = 4,
		InstanceTransform = 5,
	};
}

// a class for encapsulating building and rendering an RGB cube
struct ColorCubeScene {

	// Program
	oglplus::shapes::ShapeWrapper cube;
	oglplus::Program prog;
	oglplus::VertexArray vao;
	GLuint instanceCount;
	oglplus::Buffer instances;

	GLuint shaderProg;
	GLuint screenShaderProg;
	GLuint pyrShaderProg;
	GLuint texture_box;
	GLuint texture_skybox[2];
	GLuint texture_biggerskybox;
	
	unsigned char * imgData; 
	int imgWidth;
	int imgHeight;

	Box * box;
	glm::mat4 boxtransform;
	float boxScale = 0.2f;
	Box * skybox;
	Box * biggerSkyBox;

	Box * x;
	Box * y;
	Box * z;

	Quad * leftwall;
	GLuint leftTextures[2];
	Quad * rightwall;
	GLuint rightTextures[2];
	Quad * floor;
	GLuint floorTextures[2];

	glm::vec3 leftWallVerts[4];
	glm::vec3 rightWallVerts[4];
	glm::vec3 floorVerts[4];
	glm::vec3 eyePos[2];

	Pyramid * lefteye_wireFrames [3];
	Pyramid * righteye_wireFrames[3];

	mat4 quadProjections[3];
	GLuint renderedTextures[6];
	GLuint fbo;
	GLuint renderedTexture;

	mat4 posOnly = mat4(1.0f);

	bool B_down = false;
	bool A_down = false;
	bool X_down = false;
	bool track = true;
	bool debug = false;
	bool broken = false;
	bool viewFromController = false;
	// For controller input
	ovrTrackingState trackstate;
	ovrPosef handPoses[2];
	ovrInputState inputstate;
	bool triggerPressed[2] = { false, false };

	// VBOs for the cube's vertices and normals
	//const unsigned int GRID_SIZE{ 5 };

public:
	ColorCubeScene() : cube({ "Position", "Normal" }, oglplus::shapes::Cube()) {
		shaderProg = LoadShaders("shader.vert", "shader.frag");
		screenShaderProg = LoadShaders("screenShader.vert", "screenShader.frag");
		pyrShaderProg = LoadShaders("pyrShader.vert", "pyrShader.frag");
		leftwall = new Quad();
		rightwall = new Quad();
		floor = new Quad();

		x = new Box();
		y = new Box();
		z = new Box();

		box = new Box();
		boxtransform = glm::translate(boxtransform, glm::vec3(0.0f, 0.f, -1.f));
		skybox = new Box();		
		biggerSkyBox = new Box();

		//Acquire the width, height, and data

		//Load calibration cube textures
		imgData = loadPPM("../Project3-Assets/vr_test_pattern.ppm", imgWidth, imgHeight);
		vector<unsigned char*> cubeDataVec(6, imgData);
		texture_box = box->loadBoxTexture(cubeDataVec, imgWidth, imgHeight);

		//Load skybox textures
		vector<unsigned char*> skyboxDataVec;
		skyboxDataVec.push_back(loadPPM("../Project3-Assets/left-ppm/px.ppm", imgWidth, imgHeight));
		skyboxDataVec.push_back(loadPPM("../Project3-Assets/left-ppm/nx.ppm", imgWidth, imgHeight));
		skyboxDataVec.push_back(loadPPM("../Project3-Assets/left-ppm/py.ppm", imgWidth, imgHeight));
		skyboxDataVec.push_back(loadPPM("../Project3-Assets/left-ppm/ny.ppm", imgWidth, imgHeight));
		skyboxDataVec.push_back(loadPPM("../Project3-Assets/left-ppm/pz.ppm", imgWidth, imgHeight));
		skyboxDataVec.push_back(loadPPM("../Project3-Assets/left-ppm/nz.ppm", imgWidth, imgHeight));
		texture_skybox[0] = skybox->loadBoxTexture(skyboxDataVec, imgWidth, imgWidth);

		vector<unsigned char*> skyboxDataVec2;
		skyboxDataVec2.push_back(loadPPM("../Project3-Assets/right-ppm/px.ppm", imgWidth, imgHeight));
		skyboxDataVec2.push_back(loadPPM("../Project3-Assets/right-ppm/nx.ppm", imgWidth, imgHeight));
		skyboxDataVec2.push_back(loadPPM("../Project3-Assets/right-ppm/py.ppm", imgWidth, imgHeight));
		skyboxDataVec2.push_back(loadPPM("../Project3-Assets/right-ppm/ny.ppm", imgWidth, imgHeight));
		skyboxDataVec2.push_back(loadPPM("../Project3-Assets/right-ppm/pz.ppm", imgWidth, imgHeight));
		skyboxDataVec2.push_back(loadPPM("../Project3-Assets/right-ppm/nz.ppm", imgWidth, imgHeight));
		texture_skybox[1] = skybox->loadBoxTexture(skyboxDataVec2, imgWidth, imgWidth);
		
		//Load biggerskybox textures
		vector<unsigned char*> biggerSkyboxDataVec;
		biggerSkyboxDataVec.push_back(loadPPM("../Project3-Assets/bsk/SunSetLeft2048.ppm", imgWidth, imgHeight));
		biggerSkyboxDataVec.push_back(loadPPM("../Project3-Assets/bsk/SunSetRight2048.ppm", imgWidth, imgHeight));
		biggerSkyboxDataVec.push_back(loadPPM("../Project3-Assets/bsk/SunSetUp2048.ppm", imgWidth, imgHeight));
		biggerSkyboxDataVec.push_back(loadPPM("../Project3-Assets/bsk/SunSetDown2048.ppm", imgWidth, imgHeight));
		biggerSkyboxDataVec.push_back(loadPPM("../Project3-Assets/bsk/SunSetFront2048.ppm", imgWidth, imgHeight));
		biggerSkyboxDataVec.push_back(loadPPM("../Project3-Assets/bsk/SunSetBack2048.ppm", imgWidth, imgHeight));
		texture_biggerskybox = biggerSkyBox->loadBoxTexture(biggerSkyboxDataVec, imgWidth, imgWidth);

		unsigned char* data = loadPPM("../Project3-Assets/left-ppm/nx.ppm", imgWidth, imgHeight);
		leftTextures[0] = leftwall->loadQuadTexture(data, imgWidth, imgHeight);
		data = loadPPM("../Project3-Assets/right-ppm/nx.ppm", imgWidth, imgHeight);
		leftTextures[1] = leftwall->loadQuadTexture(data, imgWidth, imgHeight);

		data = loadPPM("../Project3-Assets/left-ppm/pz.ppm", imgWidth, imgHeight);
		rightTextures[0] = rightwall->loadQuadTexture(data, imgWidth, imgHeight);
		data = loadPPM("../Project3-Assets/right-ppm/pz.ppm", imgWidth, imgHeight);
		rightTextures[1] = rightwall->loadQuadTexture(data, imgWidth, imgHeight);

		data = loadPPM("../Project3-Assets/left-ppm/ny.ppm", imgWidth, imgHeight);
		floorTextures[0] = floor->loadQuadTexture(data, imgWidth, imgHeight);
		data = loadPPM("../Project3-Assets/right-ppm/ny.ppm", imgWidth, imgHeight);
		floorTextures[1] = floor->loadQuadTexture(data, imgWidth, imgHeight);


		//Set up frame buffer
		glGenFramebuffers(1, &fbo);
		glBindFramebuffer(GL_FRAMEBUFFER, fbo);

		// The texture we're going to render to
		glGenTextures(1, &renderedTexture);

		// "Bind" the newly created texture : all future texture functions will modify this texture
		glBindTexture(GL_TEXTURE_2D, renderedTexture);

		// Give an empty image to OpenGL ( the last "0" )
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 1024, 1024, 0, GL_RGB, GL_UNSIGNED_BYTE, 0);

		// Poor filtering. Needed !
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

		// The depth buffer
		GLuint depthrenderbuffer;
		glGenRenderbuffers(1, &depthrenderbuffer);
		glBindRenderbuffer(GL_RENDERBUFFER, depthrenderbuffer);
		glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, 1024, 1024);
		glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, 
			GL_RENDERBUFFER, depthrenderbuffer);

		// Set "renderedTexture" as our colour attachement #0
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, renderedTexture, 0);

		// Set the list of draw buffers.
		GLenum DrawBuffers[1] = { GL_COLOR_ATTACHMENT0 };
		glDrawBuffers(1, DrawBuffers); // "1" is the size of DrawBuffers

		glBindTexture(GL_TEXTURE_2D, 0);
		glBindFramebuffer(GL_FRAMEBUFFER, 0);

		for (int i = 0; i < 6; i++) {
			glGenTextures(1, &renderedTextures[i]);
			glBindTexture(GL_TEXTURE_2D, renderedTextures[i]);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 1024, 1024, 0, GL_RGB, GL_UNSIGNED_BYTE, 0);

			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
			glBindTexture(GL_TEXTURE_2D, 0);
		}

		glLineWidth(2.f);
	}

	void render(const mat4 & projection, const mat4 & modelview, ovrSession session, ovrEyeType eye, int displayMode, GLuint hmd_fbo, ovrLayerEyeFov _sceneLayer, uvec2 windowSize) {
		checkInput(session, track, B_down, A_down, debug);

		//Check controller input
		// Position + Orientation
		double displayMidpointSeconds = ovr_GetPredictedDisplayTime(session, 0);
		trackstate = ovr_GetTrackingState(session, displayMidpointSeconds, ovrTrue);

		//handPoses[LEFT] = trackstate.HandPoses[ovrHand_Left].ThePose;
		handPoses[RIGHT] = trackstate.HandPoses[ovrHand_Right].ThePose;


		// T R I G G E R E D
		// finger triggers
		if (OVR_SUCCESS(ovr_GetInputState(session, ovrControllerType_Touch, &inputstate))) {
			//triggerPressed[LEFT] = inputstate.HandTrigger[ovrHand_Left] > 0.5f;
			triggerPressed[RIGHT] = inputstate.HandTrigger[ovrHand_Right] > 0.5f;
		}

		viewFromController = triggerPressed[RIGHT];

		glUseProgram(shaderProg);

		//Draw CAVE
		GLuint uProjection = glGetUniformLocation(shaderProg, "projection");
		GLuint uModelview = glGetUniformLocation(shaderProg, "modelview");
		GLuint uTransform = glGetUniformLocation(shaderProg, "transform");
		GLuint uColor = glGetUniformLocation(shaderProg, "incolor");

		glUniformMatrix4fv(uProjection, 1, GL_FALSE, (&projection[0][0]));
		glUniformMatrix4fv(uModelview, 1, GL_FALSE, &modelview[0][0]);

		glm::mat4 bskTransform = glm::scale(glm::mat4(1.0f), glm::vec3(20.0f));
		glUniformMatrix4fv(uTransform, 1, GL_FALSE, &(bskTransform[0][0]));
		biggerSkyBox->draw(shaderProg, texture_biggerskybox);

		
		
		if (viewFromController) {
			glm::mat4 mv = ovr::toGlm(handPoses[RIGHT]);
			mv = glm::inverse(mv);
			glUniformMatrix4fv(uModelview, 1, GL_FALSE, &(mv[0][0]));
		}
		else {
			if (track)
				posOnly[3] = modelview[3];
			glUniformMatrix4fv(uModelview, 1, GL_FALSE, &(posOnly[0][0]));
		}		
		

		//---------------Coordinate axes---------------//
		/*glm::mat4 transform;
		glm::vec3 color;

		color = vec3(1, 0, 0);
		transform = glm::scale(mat4(1.f), vec3(10, 0.0005f, 0.0005f));
		glUniformMatrix4fv(uTransform, 1, GL_FALSE, &(transform[0][0]));
		glUniform3fv(uColor, 1, &(color[0]));
		x->draw(shaderProg);
		color = vec3(0, 1, 0);
		transform = glm::scale(mat4(1.f), vec3(0.0005f, 10, 0.0005f));
		glUniformMatrix4fv(uTransform, 1, GL_FALSE, &(transform[0][0]));
		glUniform3fv(uColor, 1, &(color[0]));
		y->draw(shaderProg);
		color = vec3(0, 0, 1);
		transform = glm::scale(mat4(1.f), vec3(0.0005f, 0.0005f, 10));
		glUniformMatrix4fv(uTransform, 1, GL_FALSE, &(transform[0][0]));
		glUniform3fv(uColor, 1, &(color[0]));
		z->draw(shaderProg);*/
		//-----------------END AXES--------------------//


		//-----------------MATHEMATICS-----------------//
		glm::mat4 leftTransform;
		leftTransform = glm::rotate((float)glm::radians(45.f), glm::vec3(0, 1, 0));
		leftTransform = glm::scale(leftTransform, vec3(1.2f));
		leftTransform = glm::translate(leftTransform, vec3(-0.f, 0, -1));

		glm::mat4 rightTransform;
		rightTransform = glm::rotate((float)glm::radians(-45.f), glm::vec3(0, 1, 0));
		rightTransform = glm::scale(rightTransform, vec3(1.2f));
		rightTransform = glm::translate(rightTransform, vec3(0.f, 0, -1));

		glm::mat4 floorTransform;
		floorTransform = glm::rotate(glm::radians(-90.f), glm::vec3(1, 0, 0));
		floorTransform = glm::rotate(floorTransform, glm::radians(45.f), glm::vec3(0, 0, 1));
		floorTransform = glm::scale(floorTransform, glm::vec3(1.2f));
		floorTransform = glm::translate(floorTransform, glm::vec3(0, 0, -1.f));

		//LEFT WALL MATH
		if (viewFromController) {
			if (track) {
				if (!eye) {	//Left eye
					eyePos[eye] = vec3(ovr::toGlm(handPoses[RIGHT].Position));
					eyePos[eye].x -= 0.0325f;
				}
				else { //Right
					eyePos[eye] = vec3(ovr::toGlm(handPoses[RIGHT].Position));
					eyePos[eye].x += 0.0325f;
				}
				
			}
		}
		else {
			if (track)
				eyePos[eye] = vec3(ovr::toGlm(_sceneLayer.RenderPose[eye].Position));
		} 
		leftWallVerts[0] = leftTransform * vec4(leftwall->vertices[0], 1.0f);
		leftWallVerts[1] = leftTransform * vec4(leftwall->vertices[1], 1.0f);
		leftWallVerts[2] = leftTransform * vec4(leftwall->vertices[2], 1.0f);
		leftWallVerts[3] = leftTransform * vec4(leftwall->vertices[3], 1.0f);
		vec3 va = leftWallVerts[0] - eyePos[eye];
		vec3 vb = leftWallVerts[1] - eyePos[eye];
		vec3 vc = leftWallVerts[3] - eyePos[eye];

		vec3 vr = glm::normalize(leftWallVerts[1] - leftWallVerts[0]);
		vec3 vu = glm::normalize(leftWallVerts[3] - leftWallVerts[0]);
		vec3 vn = glm::normalize(glm::cross(vr, vu));
		float dist = -glm::dot(vn, va);
		float l = glm::dot(vr, va) * 0.001f / dist;
		float r = glm::dot(vr, vb) * 0.001f / dist;
		float b = glm::dot(vu, va) * 0.001f / dist;
		float t = glm::dot(vu, vc) * 0.001f / dist;
		mat4 M = mat4(1.0f);
		M[0] = vec4(vr, 0.f);
		M[1] = vec4(vu, 0.f);
		M[2] = vec4(vn, 0.f);
		M = glm::transpose(M);
		mat4 T = mat4(1.0f);
		T[3] = vec4(-eyePos[eye].x, -eyePos[eye].y, -eyePos[eye].z, 1.0f);
		quadProjections[0] = glm::frustum(l, r, b, t, 0.001f, 1000.f) * M * T;

		//RIGHT WALL MATH
		rightWallVerts[0] = rightTransform * vec4(rightwall->vertices[0], 1.0f);
		rightWallVerts[1] = rightTransform * vec4(rightwall->vertices[1], 1.0f);
		rightWallVerts[2] = rightTransform * vec4(rightwall->vertices[2], 1.0f);
		rightWallVerts[3] = rightTransform * vec4(rightwall->vertices[3], 1.0f);
		va = rightWallVerts[0] - eyePos[eye];
		vb = rightWallVerts[1] - eyePos[eye];
		vc = rightWallVerts[3] - eyePos[eye];

		vr = glm::normalize(rightWallVerts[1] - rightWallVerts[0]);
		vu = glm::normalize(rightWallVerts[3] - rightWallVerts[0]);
		vn = glm::normalize(glm::cross(vr, vu));
		dist = -glm::dot(vn, va);
		l = glm::dot(vr, va) * 0.001f / dist;
		r = glm::dot(vr, vb) * 0.001f / dist;
		b = glm::dot(vu, va) * 0.001f / dist;
		t = glm::dot(vu, vc) * 0.001f / dist;
		M = mat4(1.0f);
		M[0] = vec4(vr, 0.f);
		M[1] = vec4(vu, 0.f);
		M[2] = vec4(vn, 0.f);
		M = glm::transpose(M);
		T = mat4(1.0f);
		T[3] = vec4(-eyePos[eye].x, -eyePos[eye].y, -eyePos[eye].z, 1.0f);
		quadProjections[1] = glm::frustum(l, r, b, t, 0.001f, 1000.f) * M * T;

		//FLOOR WALL MATH
		floorVerts[0] = floorTransform * vec4(floor->vertices[0], 1.0f);
		floorVerts[1] = floorTransform * vec4(floor->vertices[1], 1.0f);
		floorVerts[2] = floorTransform * vec4(floor->vertices[2], 1.0f);
		floorVerts[3] = floorTransform * vec4(floor->vertices[3], 1.0f);
		va = floorVerts[0] - eyePos[eye];
		vb = floorVerts[1] - eyePos[eye];
		vc = floorVerts[3] - eyePos[eye];

		vr = glm::normalize(floorVerts[1] - floorVerts[0]);
		vu = glm::normalize(floorVerts[3] - floorVerts[0]);
		vn = glm::normalize(glm::cross(vr, vu));
		dist = -glm::dot(vn, va);
		l = glm::dot(vr, va) * 0.001f / dist;
		r = glm::dot(vr, vb) * 0.001f / dist;
		b = glm::dot(vu, va) * 0.001f / dist;
		t = glm::dot(vu, vc) * 0.001f / dist;
		M = mat4(1.0f);
		M[0] = vec4(vr, 0.f);
		M[1] = vec4(vu, 0.f);
		M[2] = vec4(vn, 0.f);
		M = glm::transpose(M);
		T = mat4(1.0f);
		T[3] = vec4(-eyePos[eye].x, -eyePos[eye].y, -eyePos[eye].z, 1.0f);
		quadProjections[2] = glm::frustum(l, r, b, t, 0.001f, 1000.f) * M * T;
		//---------------MATHEMATICS END---------------//

		//if (track) {
			glEnable(GL_DEPTH_TEST);

			glBindFramebuffer(GL_FRAMEBUFFER, fbo);
			//glViewport(0, 0, windowSize.x, windowSize.y);
			glViewport(0, 0, 1024, 1024);
			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
			for (int i = 0; i < 3; i++) {


				//Setup texture
				glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, renderedTextures[eye * 3 + i], 0);
				glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

				glUniformMatrix4fv(uProjection, 1, GL_FALSE, (&quadProjections[i][0][0]));
				//Render cubes to walls

				mat4 scaledBoxTransform = glm::scale(boxtransform, glm::vec3(boxScale));
				glUniformMatrix4fv(uTransform, 1, GL_FALSE, &(scaledBoxTransform[0][0]));
				box->draw(shaderProg, texture_box);

				glDepthMask(GL_FALSE);
				glm::mat4 skyboxTransform = glm::scale(glm::mat4(1.0f), glm::vec3(20.0f));
				glUniformMatrix4fv(uTransform, 1, GL_FALSE, &(skyboxTransform[0][0]));
				skybox->draw(shaderProg, texture_skybox[eye]);
				glDepthMask(GL_TRUE);
			}

			glBindFramebuffer(GL_FRAMEBUFFER, 0);
			//glViewport(0, 0, imgWidth, imgHeight);
			
			glClear(GL_COLOR_BUFFER_BIT);

			glDisable(GL_DEPTH_TEST);
		//}

		
		//---------------Draw the CAVE---------------//
		glBindFramebuffer(GL_FRAMEBUFFER, hmd_fbo);
		const auto& vp = _sceneLayer.Viewport[eye];
		glViewport(vp.Pos.x, vp.Pos.y, vp.Size.w, vp.Size.h);

		glUseProgram(screenShaderProg);
		uProjection = glGetUniformLocation(screenShaderProg, "projection");
		uModelview = glGetUniformLocation(screenShaderProg, "modelview");
		uTransform = glGetUniformLocation(screenShaderProg, "transform");
		uColor = glGetUniformLocation(screenShaderProg, "incolor");
		GLuint uBroken = glGetUniformLocation(screenShaderProg, "broken");

		glUniformMatrix4fv(uProjection, 1, GL_FALSE, (&projection[0][0]));
		glUniformMatrix4fv(uModelview, 1, GL_FALSE, &(modelview[0][0]));
		glUniform1i(uBroken, 0);

		//left wall
		const glm::vec3 leftColor(0, 0.7f, 0);
		glUniformMatrix4fv(uTransform, 1, GL_FALSE, &(leftTransform[0][0]));
		glUniform3fv(uColor, 1, &(leftColor[0]));
		leftwall->draw(screenShaderProg, renderedTextures[eye * 3]);//leftTextures[eye]);

		//right wall
		const glm::vec3 rightColor(0, 0, 0.7f);
		glUniformMatrix4fv(uTransform, 1, GL_FALSE, &(rightTransform[0][0]));
		glUniform3fv(uColor, 1, &(rightColor[0]));
		rightwall->draw(screenShaderProg, renderedTextures[eye * 3 + 1]);//rightTextures[eye]);

		//floor
		const glm::vec3 floorColor(0.7f, 0, 0);
		glUniformMatrix4fv(uTransform, 1, GL_FALSE, &(floorTransform[0][0]));
		glUniform3fv(uColor, 1, &(floorColor[0]));
		if(eye && broken)
			glUniform1i(uBroken, 2);
		floor->draw(screenShaderProg, renderedTextures[eye * 3 + 2]);//floorTextures[eye]);
		

		
		//---------------Wireframes---------------//
		//If A is pressed
		if (debug) {
			glUseProgram(pyrShaderProg);

			//Draw wireframes
			uProjection = glGetUniformLocation(pyrShaderProg, "projection");
			uModelview = glGetUniformLocation(pyrShaderProg, "modelview");
			uTransform = glGetUniformLocation(pyrShaderProg, "transform");
			uColor = glGetUniformLocation(pyrShaderProg, "incolor");

			glUniformMatrix4fv(uProjection, 1, GL_FALSE, (&projection[0][0]));
			glUniformMatrix4fv(uModelview, 1, GL_FALSE, &(modelview[0][0]));
						
			//Pyramid coordinates
			glm::mat4 pyr_transform;
			glUniformMatrix4fv(uTransform, 1, GL_FALSE, &(pyr_transform[0][0]));

			//LEFT EYE WIREFRAMES 
			//LEFT WALL WIREFRAME
			glm::vec3 wireframe_color(0, 0, 1);
			glUniform3fv(uColor, 1, &(wireframe_color[0]));
			std::vector<glm::vec3> leftWall_vertices = {
				leftWallVerts[0], leftWallVerts[1], leftWallVerts[3], leftWallVerts[2]
			};
			leftWall_vertices.insert(leftWall_vertices.begin(), vec3(eyePos[eye].x, eyePos[eye].y, eyePos[eye].z));
			lefteye_wireFrames[0] = new Pyramid(leftWall_vertices);
			lefteye_wireFrames[0]->draw(pyrShaderProg);

			//RIGHT WALL WIREFRAME
			wireframe_color = vec3(1, 0, 0);
			glUniform3fv(uColor, 1, &(wireframe_color[0]));
			std::vector<glm::vec3> rightWall_vertices = {
				rightWallVerts[0], rightWallVerts[1], rightWallVerts[3], rightWallVerts[2]
			};
			rightWall_vertices.insert(rightWall_vertices.begin(), vec3(eyePos[eye].x, eyePos[eye].y, eyePos[eye].z));
			lefteye_wireFrames[1] = new Pyramid(rightWall_vertices);
			lefteye_wireFrames[1]->draw(pyrShaderProg);

			//FLOOR WIREFRAME
			wireframe_color = vec3(0, 1, 0);
			glUniform3fv(uColor, 1, &(wireframe_color[0]));
			std::vector<glm::vec3> floor_vertices = {
				floorVerts[0], floorVerts[1], floorVerts[3], floorVerts[2]
			};
			floor_vertices.insert(floor_vertices.begin(), vec3(eyePos[eye].x, eyePos[eye].y, eyePos[eye].z));
			lefteye_wireFrames[2] = new Pyramid(floor_vertices);
			lefteye_wireFrames[2]->draw(pyrShaderProg);
		}

	}

	void checkInput(ovrSession session, bool &track, bool &B_down, bool& A_down, bool& debug) {
		ovrInputState inputState;
		if (OVR_SUCCESS(ovr_GetInputState(session, ovrControllerType_Touch, &inputState)))
		{
			// On left thumbstick movement, change box size
			if (inputState.Thumbstick[ovrHand_Left].x != 0) {
				float temp = boxScale + (inputState.Thumbstick[ovrHand_Left].x * 0.01f);
				if (temp > 0.01f && temp < 1.0f)
					boxScale = temp;
			}

			// On left thumbstick press, reset box size
			if (inputState.Buttons & ovrButton_LThumb) {
				boxScale = 0.2f;
			}

			// On right thumbstick movement or left thumbstick vertical movement, translate box
			if (inputState.Thumbstick[ovrHand_Right].x != 0 || inputState.Thumbstick[ovrHand_Right].y != 0 
					|| inputState.Thumbstick[ovrHand_Left].y != 0) {
				boxtransform = glm::translate(boxtransform, vec3(inputState.Thumbstick[ovrHand_Right].x * 0.01f, 
					inputState.Thumbstick[ovrHand_Right].y * 0.01f, inputState.Thumbstick[ovrHand_Left].y * -0.01f));
			}

			// On B press, change head tracking mode
			if (inputState.Buttons & ovrButton_B && !B_down) {
				B_down = true;
				track = !track;
				printf("Tracking mode:%d", track);
			}
			if (!(inputState.Buttons & ovrButton_B)) {
				B_down = false;
			}

			// Check A press
			if (inputState.Buttons & ovrButton_A && !A_down) {
				A_down = true;
				debug = !debug;				
			}
			if (!(inputState.Buttons & ovrButton_A)) {
				A_down = false;
			}

			// Check X press
			if (inputState.Buttons & ovrButton_X && !X_down) {
				X_down = true;
				broken = !broken;
			}
			if (!(inputState.Buttons & ovrButton_X)) {
				X_down = false;
			}

		}
	}
};



// An example application that renders a simple cube
class ExampleApp : public RiftApp {
	std::shared_ptr<ColorCubeScene> cubeScene;

public:
	ExampleApp() { }

protected:
	void initGl() override {
		RiftApp::initGl();
		glClearColor(0.5, 0.5, 0.5, 0);
		glEnable(GL_DEPTH_TEST);
		ovr_RecenterTrackingOrigin(_session);
		cubeScene = std::shared_ptr<ColorCubeScene>(new ColorCubeScene());
	}

	void shutdownGl() override {
		cubeScene.reset();
	}

	void renderScene(const glm::mat4 & projection, const glm::mat4 & headPose, ovrEyeType eye, int displaymode, GLuint hmd_fbo, ovrLayerEyeFov _sceneLayer, uvec2 windowSize) override {
		cubeScene->render(projection, glm::inverse(headPose), _session, eye, displaymode, hmd_fbo, _sceneLayer, windowSize);
	}
};

// Execute our example class
int main(int argc, char** argv)
{
	int result = -1;
	try {
		if (!OVR_SUCCESS(ovr_Initialize(nullptr))) {
			FAIL("Failed to initialize the Oculus SDK");
		}
		result = ExampleApp().run();
	}
	catch (std::exception & error) {
		OutputDebugStringA(error.what());
		std::cerr << error.what() << std::endl;
	}
	ovr_Shutdown();
	return result;
}
