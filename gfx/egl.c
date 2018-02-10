#include <assert.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES2/gl2.h>
#include <stdlib.h>
#include <chopsui/util/log.h>
#include "gfx/egl.h"
#include "glapi.h"

// Extension documentation
// https://www.khronos.org/registry/EGL/extensions/KHR/EGL_KHR_image_base.txt.
// https://cgit.freedesktop.org/mesa/mesa/tree/docs/specs/WL_bind_wayland_display.spec

const char *egl_error(void) {
	switch (eglGetError()) {
	case EGL_SUCCESS:
		return "Success";
	case EGL_NOT_INITIALIZED:
		return "Not initialized";
	case EGL_BAD_ACCESS:
		return "Bad access";
	case EGL_BAD_ALLOC:
		return "Bad alloc";
	case EGL_BAD_ATTRIBUTE:
		return "Bad attribute";
	case EGL_BAD_CONTEXT:
		return "Bad Context";
	case EGL_BAD_CONFIG:
		return "Bad Config";
	case EGL_BAD_CURRENT_SURFACE:
		return "Bad current surface";
	case EGL_BAD_DISPLAY:
		return "Bad display";
	case EGL_BAD_SURFACE:
		return "Bad surface";
	case EGL_BAD_MATCH:
		return "Bad match";
	case EGL_BAD_PARAMETER:
		return "Bad parameter";
	case EGL_BAD_NATIVE_PIXMAP:
		return "Bad native pixmap";
	case EGL_BAD_NATIVE_WINDOW:
		return "Bad native window";
	case EGL_CONTEXT_LOST:
		return "Context lost";
	default:
		return "Unknown";
	}
}

static bool egl_get_config(EGLDisplay disp, EGLint *attribs, EGLConfig *out,
		EGLint visual_id) {
	EGLint count = 0, matched = 0, ret;

	ret = eglGetConfigs(disp, NULL, 0, &count);
	if (ret == EGL_FALSE || count == 0) {
		sui_log(L_ERROR, "eglGetConfigs returned no configs");
		return false;
	}

	EGLConfig configs[count];

	ret = eglChooseConfig(disp, attribs, configs, count, &matched);
	if (ret == EGL_FALSE) {
		sui_log(L_ERROR, "eglChooseConfig failed");
		return false;
	}

	for (int i = 0; i < matched; ++i) {
		EGLint visual;
		if (!eglGetConfigAttrib(disp, configs[i],
				EGL_NATIVE_VISUAL_ID, &visual)) {
			continue;
		}

		if (!visual_id || visual == visual_id) {
			*out = configs[i];
			return true;
		}
	}

	sui_log(L_ERROR, "no valid egl config found");
	return false;
}

bool sui_egl_init(struct sui_egl *egl, EGLenum platform, void *remote_display,
		EGLint *config_attribs, EGLint visual_id) {
	if (!load_glapi()) {
		return false;
	}

	if (eglBindAPI(EGL_OPENGL_ES_API) == EGL_FALSE) {
		sui_log(L_ERROR, "Failed to bind to the OpenGL ES API: %s", egl_error());
		goto error;
	}

	if (platform == EGL_PLATFORM_SURFACELESS_MESA) {
		assert(remote_display == NULL);
		egl->display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
	} else {
		egl->display = eglGetPlatformDisplayEXT(platform, remote_display, NULL);
	}
	if (egl->display == EGL_NO_DISPLAY) {
		sui_log(L_ERROR, "Failed to create EGL display: %s", egl_error());
		goto error;
	}

	EGLint major, minor;
	if (eglInitialize(egl->display, &major, &minor) == EGL_FALSE) {
		sui_log(L_ERROR, "Failed to initialize EGL: %s", egl_error());
		goto error;
	}

	if (!egl_get_config(egl->display, config_attribs, &egl->config, visual_id)) {
		sui_log(L_ERROR, "Failed to get EGL config");
		goto error;
	}

	static const EGLint attribs[] = {EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE};

	egl->context = eglCreateContext(egl->display, egl->config,
		EGL_NO_CONTEXT, attribs);

	if (egl->context == EGL_NO_CONTEXT) {
		sui_log(L_ERROR, "Failed to create EGL context: %s", egl_error());
		goto error;
	}

	eglMakeCurrent(egl->display, EGL_NO_SURFACE, EGL_NO_SURFACE, egl->context);
	egl->egl_exts = eglQueryString(egl->display, EGL_EXTENSIONS);
	egl->gl_exts = (const char*) glGetString(GL_EXTENSIONS);
	sui_log(L_INFO, "Using EGL %d.%d", (int)major, (int)minor);
	sui_log(L_INFO, "Supported EGL extensions: %s", egl->egl_exts);
	sui_log(L_INFO, "Using %s", glGetString(GL_VERSION));
	sui_log(L_INFO, "Supported OpenGL ES extensions: %s", egl->gl_exts);
	return true;

error:
	eglMakeCurrent(EGL_NO_DISPLAY, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
	eglTerminate(egl->display);
	eglReleaseThread();
	return false;
}

void sui_egl_finish(struct sui_egl *egl) {
	eglMakeCurrent(EGL_NO_DISPLAY, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
	eglDestroyContext(egl->display, egl->context);
	eglTerminate(egl->display);
	eglReleaseThread();
}

EGLSurface sui_egl_create_surface(struct sui_egl *egl, void *window) {
	EGLSurface surf = eglCreatePlatformWindowSurfaceEXT(
			egl->display, egl->config, window, NULL);
	if (surf == EGL_NO_SURFACE) {
		sui_log(L_ERROR, "Failed to create EGL surface: %s", egl_error());
		return EGL_NO_SURFACE;
	}
	return surf;
}
