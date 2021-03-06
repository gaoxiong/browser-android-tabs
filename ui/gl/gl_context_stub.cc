// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/gl_context_stub.h"

#include "ui/gl/gl_gl_api_implementation.h"
#include "ui/gl/gl_stub_api.h"

namespace gl {

GLContextStub::GLContextStub() : GLContextStub(nullptr) {}
GLContextStub::GLContextStub(GLShareGroup* share_group)
    : GLContextReal(share_group),
      use_stub_api_(false),
      version_str_("OpenGL ES 3.0"),
      extensions_("GL_EXT_framebuffer_object") {}

bool GLContextStub::Initialize(GLSurface* compatible_surface,
                               const GLContextAttribs& attribs) {
  return true;
}

bool GLContextStub::MakeCurrent(GLSurface* surface) {
  BindGLApi();
  SetCurrent(surface);
  InitializeDynamicBindings();
  return true;
}

void GLContextStub::ReleaseCurrent(GLSurface* surface) {
  SetCurrent(nullptr);
}

bool GLContextStub::IsCurrent(GLSurface* surface) {
  return GetRealCurrent() == this;
}

void* GLContextStub::GetHandle() {
  return nullptr;
}

void GLContextStub::OnSetSwapInterval(int interval) {
}

std::string GLContextStub::GetGLVersion() {
  return version_str_;
}

std::string GLContextStub::GetGLRenderer() {
  return std::string("CHROMIUM");
}

bool GLContextStub::WasAllocatedUsingRobustnessExtension() {
  return HasExtension("GL_ARB_robustness") ||
         HasExtension("GL_KHR_robustness") || HasExtension("GL_EXT_robustness");
}

std::string GLContextStub::GetExtensions() {
  return extensions_;
}

void GLContextStub::SetUseStubApi(bool stub_api) {
  use_stub_api_ = stub_api;
}

void GLContextStub::SetExtensionsString(const char* extensions) {
  extensions_ = extensions;
}

void GLContextStub::SetGLVersionString(const char* version_str) {
  version_str_ = std::string(version_str ? version_str : "");
}

GLContextStub::~GLContextStub() {}

GLApi* GLContextStub::CreateGLApi(DriverGL* driver) {
  if (use_stub_api_) {
    GLStubApi* stub_api = new GLStubApi();
    if (!version_str_.empty()) {
      stub_api->set_version(version_str_);
    }
    if (!extensions_.empty()) {
      stub_api->set_extensions(extensions_);
    }
    return stub_api;
  }

  return GLContext::CreateGLApi(driver);
}

}  // namespace gl
