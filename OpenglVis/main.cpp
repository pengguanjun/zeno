#include "stdafx.hpp"
#include "ViewNode.hpp"
#include "main.hpp"
#include <sstream>
#include <cstdlib>
#include <thread>

namespace zenvis {

static GLFWwindow *window;
static int nx = 1024, ny = 768;
static std::string title = "quicksilver";

std::set<IViewNode *> IViewNode::instances;

static void error_callback(int error, const char *msg) {
  fprintf(stderr, "error %d: %s\n", error, msg);
}

static double last_xpos, last_ypos;
static double theta = 0.0, phi = 0.0, radius = 3.0, fov = 60.0;
static bool mmb_pressed, shift_pressed, ortho_mode;
static glm::dvec3 center;

void set_program_uniforms(Program *pro) {
  double cos_t = glm::cos(theta), sin_t = glm::sin(theta);
  double cos_p = glm::cos(phi), sin_p = glm::sin(phi);
  glm::dvec3 back(cos_t * sin_p, sin_t, -cos_t * cos_p);
  glm::dvec3 up(-sin_t * sin_p, cos_t, sin_t * cos_p);

  auto view = glm::lookAt(center - back * radius, center, up);
  auto proj = glm::perspective(glm::radians(fov), nx * 1.0 / ny, 0.05, 500.0);
  if (ortho_mode) {
    view = glm::lookAt(center - back, center, up);
    proj = glm::ortho(-radius * nx / ny, radius * nx / ny, -radius, radius,
                      -100.0, 100.0);
  }

  pro->use();

  auto pers = proj * view;
  pro->set_uniform("mVP", pers);
  pro->set_uniform("mInvVP", glm::inverse(pers));
  pro->set_uniform("mView", view);
  pro->set_uniform("mProj", proj);

  pro->set_uniform("light.dir", glm::normalize(glm::vec3(1, 2, 3)));
  pro->set_uniform("light.color", glm::vec3(1, 1, 1));
}

static void key_callback(GLFWwindow *window, int key, int scancode, int action,
                         int mods) {
  if (key == GLFW_KEY_TAB && action == GLFW_RELEASE)
    ortho_mode = !ortho_mode;
  if (key == GLFW_KEY_ESCAPE)
    glfwSetWindowShouldClose(window, GLFW_TRUE);
}

static void click_callback(GLFWwindow *window, int button, int action,
                           int mods) {
  if (button == GLFW_MOUSE_BUTTON_MIDDLE) {
    mmb_pressed = action == GLFW_PRESS;
    shift_pressed = !!(mods & GLFW_MOD_SHIFT);
  }

  if (mmb_pressed)
    glfwGetCursorPos(window, &last_xpos, &last_ypos);
}

static void scroll_callback(GLFWwindow *window, double xoffset,
                            double yoffset) {
  radius *= glm::pow(0.89, yoffset);
}

static void motion_callback(GLFWwindow *window, double xpos, double ypos) {
  double dx = (xpos - last_xpos) / nx, dy = (ypos - last_ypos) / ny;

  if (mmb_pressed && !shift_pressed) {
    theta = glm::clamp(theta - dy * M_PI, -M_PI / 2, M_PI / 2);
    phi = phi + dx * M_PI;
  }

  if (mmb_pressed && shift_pressed) {
    double cos_t = glm::cos(theta), sin_t = glm::sin(theta);
    double cos_p = glm::cos(phi), sin_p = glm::sin(phi);
    glm::dvec3 back(cos_t * sin_p, sin_t, -cos_t * cos_p);
    glm::dvec3 up(-sin_t * sin_p, cos_t, sin_t * cos_p);
    auto right = glm::cross(up, back);
    up = glm::cross(back, right);
    glm::dvec3 delta = glm::normalize(right) * dx + glm::normalize(up) * dy;
    center = center + delta * radius;
  }

  last_xpos = xpos;
  last_ypos = ypos;
}

static std::unique_ptr<VAO> vao;

static void initialize() {
  glfwInit();
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);

  window = glfwCreateWindow(nx, ny, title.c_str(), NULL, NULL);
  glfwMakeContextCurrent(window);
  glfwSetErrorCallback(error_callback);
  glfwSetKeyCallback(window, key_callback);
  glfwSetMouseButtonCallback(window, click_callback);
  glfwSetCursorPosCallback(window, motion_callback);
  glfwSetScrollCallback(window, scroll_callback);

  glewInit();

  CHECK_GL(glEnable(GL_BLEND));
  CHECK_GL(glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA));
  CHECK_GL(glEnable(GL_DEPTH_TEST));
  CHECK_GL(glEnable(GL_PROGRAM_POINT_SIZE));

  vao = std::make_unique<VAO>();

#if 0
  zen::addNode("ReadObjMesh", "No1");
  zen::setNodeParam("No1", "path", std::string("../monkey.obj"));
  zen::applyNode("No1");
  zen::addNode("ViewMesh", "No2");
  zen::setNodeInput("No2", "mesh", "No1::mesh");
  zen::applyNode("No2");
#else
#if 0
  zen::addNode("ReadParticles", "No1");
  zen::setNodeParam("No1", "path", std::string("../monkey.obj"));
  zen::applyNode("No1");
  zen::addNode("ViewParticles", "No2");
  zen::setNodeInput("No2", "pars", "No1::pars");
  zen::applyNode("No2");
#endif
#endif

#if 0
  auto mesh = std::make_unique<Mesh>("monkey.obj");
  objects.push_back(std::move(mesh));

  auto pars = std::make_unique<Particles>("fake0100.txt");
  objects.push_back(std::move(pars));

  auto voxl = std::make_unique<Volume>("assets/smoke.vdb");
  objects.push_back(std::move(voxl));
#else
#if 0
  auto pars = std::make_unique<Particles>("fake0100.txt");
  auto voxl = std::make_unique<ParticlesToVolume>(pars.get());
  auto mesh = std::make_unique<VolumeToMesh>(voxl.get());
  //objects.push_back(std::move(pars));
  //objects.push_back(std::move(voxl));
  objects.push_back(std::move(mesh));
#else
#if 0
  auto mesh = std::make_unique<Mesh>("monkey.obj");
  auto voxl = std::make_unique<MeshToVolume>(mesh.get());
  objects.push_back(std::move(voxl));
#else
#if 0
  //auto voxl = std::make_unique<Volume>("assets/smoke.vdb");
  auto voxl = std::make_unique<Volume>("../RobustMPM/rock.vdb");
  auto mesh = std::make_unique<VolumeToMesh>(voxl.get());
  objects.push_back(std::move(mesh));
#else
#endif
#endif
#endif
#endif

#if 0
  for (int i = 1; i <= 60; i++) {
    std::stringstream ss;
    ss << "/tmp/pars" << i << ".txt";
    auto css = ss.str().c_str();
    printf("Loading particles from %s\n", css);
    auto pars = std::make_unique<Particles>(css);
    objects.push_back(std::move(pars));
  }
#endif
}

static void draw_contents(void) {
  CHECK_GL(glClearColor(0.3f, 0.2f, 0.1f, 0.0f));
  CHECK_GL(glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT));
  vao->bind();
  for (auto const &node: IViewNode::instances) {
    node->draw();
  }
  vao->unbind();
  CHECK_GL(glFlush());
}

static void measure_fps() {
  char buf[128];
  static int nframes;
  static double last_time;
  double curr_time = glfwGetTime();

  nframes++;
  if (last_time == 0)
    last_time = curr_time;
  if (curr_time - last_time >= 1.0) {
    sprintf(buf, "%s (%d FPS)\n", title.c_str(), nframes);
    glfwSetWindowTitle(window, buf);
    last_time += 1.0;
    nframes = 0;
  }
}

static void render_main() {
  initialize();

  while (!glfwWindowShouldClose(window)) {
    glfwPollEvents();
    measure_fps();

    glfwGetFramebufferSize(window, &nx, &ny);
    CHECK_GL(glViewport(0, 0, nx, ny));

    draw_contents();
    glfwSwapBuffers(window);
  }

  glfwDestroyWindow(window);
  glfwTerminate();
}

std::unique_ptr<std::thread> render_thread;

static void render_start() {
  render_thread = std::make_unique<std::thread>(render_main);
}

static void render_stop() {
  render_thread->join();
}

static int defStartStop = zen::defStartStop(render_start, render_stop);

}
