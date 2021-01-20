#include <iostream>

#define GLEW_STATIC
#include <GL/glew.h>
#include <GLFW/glfw3.h>

// GLM
#include <glm/glm.hpp>
#include <glm/ext.hpp>
#include <glm/gtc/matrix_transform.hpp>

using namespace glm;

#include "graphics/Shader.h"
#include "graphics/Texture.h"
#include "graphics/Mesh.h"
#include "graphics/VoxelRenderer.h"
#include "graphics/LineBatch.h"
#include "window/Window.h"
#include "window/Events.h"
#include "window/Camera.h"
#include "loaders/png_loading.h"
#include "voxels/voxel.h"
#include "voxels/Chunk.h"
#include "voxels/Chunks.h"
#include "files/files.h"
#include "lighting/LightSolver.h"
#include "lighting/Lightmap.h"

int WIDTH = 1280;
int HEIGHT = 720;

float vertices[] = {
		// x    y
	   -0.01f,-0.01f,
	    0.01f, 0.01f,

	   -0.01f, 0.01f,
	    0.01f,-0.01f,
};

int attrs[] = {
		2,  0 //null terminator
};

int main() {
	Window::initialize(WIDTH, HEIGHT, "Window 2.0");
	Events::initialize();

	Shader* shader = load_shader("res/main.glslv", "res/main.glslf");
	if (shader == nullptr){
		std::cerr << "failed to load shader" << std::endl;
		Window::terminate();
		return 1;
	}

	Shader* crosshairShader = load_shader("res/crosshair.glslv", "res/crosshair.glslf");
	if (crosshairShader == nullptr){
		std::cerr << "failed to load crosshair shader" << std::endl;
		Window::terminate();
		return 1;
	}

	Shader* linesShader = load_shader("res/lines.glslv", "res/lines.glslf");
	if (linesShader == nullptr){
		std::cerr << "failed to load lines shader" << std::endl;
		Window::terminate();
		return 1;
	}

	Texture* texture = load_texture("res/block.png");
	if (texture == nullptr){
		std::cerr << "failed to load texture" << std::endl;
		delete shader;
		Window::terminate();
		return 1;
	}

	Chunks* chunks = new Chunks(16,16,16);
	Mesh** meshes = new Mesh*[chunks->volume];
	for (size_t i = 0; i < chunks->volume; i++)
		meshes[i] = nullptr;
	VoxelRenderer renderer(1024*1024*8);
	LineBatch* lineBatch = new LineBatch(4096);

	glClearColor(0.0f,0.0f,0.0f,1);

	glEnable(GL_DEPTH_TEST);
	glEnable(GL_CULL_FACE);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	Mesh* crosshair = new Mesh(vertices, 4, attrs);
	Camera* camera = new Camera(vec3(0,0,20), radians(90.0f));

	float lastTime = glfwGetTime();
	float delta = 0.0f;

	float camX = 0.0f;
	float camY = 0.0f;

	float speed = 15;

	int choosenBlock = 1;

	LightSolver* solverR = new LightSolver(chunks, 0);
	LightSolver* solverG = new LightSolver(chunks, 1);
	LightSolver* solverB = new LightSolver(chunks, 2);
	LightSolver* solverS = new LightSolver(chunks, 3);

	for (int y = 0; y < chunks->h*CHUNK_H; y++){
		for (int z = 0; z < chunks->d*CHUNK_D; z++){
			for (int x = 0; x < chunks->w*CHUNK_W; x++){
				voxel* vox = chunks->get(x,y,z);
				if (vox->id == 3){
					solverR->add(x,y,z,15);
					solverG->add(x,y,z,15);
					solverB->add(x,y,z,15);
				}
			}
		}
	}

	for (int z = 0; z < chunks->d*CHUNK_D; z++){
		for (int x = 0; x < chunks->w*CHUNK_W; x++){
			for (int y = chunks->h*CHUNK_H-1; y >= 0; y--){
				voxel* vox = chunks->get(x,y,z);
				if (vox->id != 0){
					break;
				}
				chunks->getChunkByVoxel(x,y,z)->lightmap->setS(x % CHUNK_W, y % CHUNK_H, z % CHUNK_D, 0xF);
			}
		}
	}

	for (int z = 0; z < chunks->d*CHUNK_D; z++){
		for (int x = 0; x < chunks->w*CHUNK_W; x++){
			for (int y = chunks->h*CHUNK_H-1; y >= 0; y--){
				voxel* vox = chunks->get(x,y,z);
				if (vox->id != 0){
					break;
				}
				if (
						chunks->getLight(x-1,y,z, 3) == 0 ||
						chunks->getLight(x+1,y,z, 3) == 0 ||
						chunks->getLight(x,y-1,z, 3) == 0 ||
						chunks->getLight(x,y+1,z, 3) == 0 ||
						chunks->getLight(x,y,z-1, 3) == 0 ||
						chunks->getLight(x,y,z+1, 3) == 0
						){
					solverS->add(x,y,z);
				}
				chunks->getChunkByVoxel(x,y,z)->lightmap->setS(x % CHUNK_W, y % CHUNK_H, z % CHUNK_D, 0xF);
			}
		}
	}

	solverR->solve();
	solverG->solve();
	solverB->solve();
	solverS->solve();

	while (!Window::isShouldClose()){
		float currentTime = glfwGetTime();
		delta = currentTime - lastTime;
		lastTime = currentTime;

		if (Events::jpressed(GLFW_KEY_ESCAPE)){
			Window::setShouldClose(true);
		}
		if (Events::jpressed(GLFW_KEY_TAB)){
			Events::toogleCursor();
		}

		for (int i = 1; i < 4; i++){
			if (Events::jpressed(GLFW_KEY_0+i)){
				choosenBlock = i;
			}
		}
		if (Events::jpressed(GLFW_KEY_F1)){
			unsigned char* buffer = new unsigned char[chunks->volume * CHUNK_VOL];
			chunks->write(buffer);
			write_binary_file("world.bin", (const char*)buffer, chunks->volume * CHUNK_VOL);
			delete[] buffer;
			std::cout << "world saved in " << (chunks->volume * CHUNK_VOL) << " bytes" << std::endl;
		}
		if (Events::jpressed(GLFW_KEY_F2)){
			unsigned char* buffer = new unsigned char[chunks->volume * CHUNK_VOL];
			read_binary_file("world.bin", (char*)buffer, chunks->volume * CHUNK_VOL);
			chunks->read(buffer);
			delete[] buffer;
		}

		if (Events::pressed(GLFW_KEY_W)){
			camera->position += camera->front * delta * speed;
		}
		if (Events::pressed(GLFW_KEY_S)){
			camera->position -= camera->front * delta * speed;
		}
		if (Events::pressed(GLFW_KEY_D)){
			camera->position += camera->right * delta * speed;
		}
		if (Events::pressed(GLFW_KEY_A)){
			camera->position -= camera->right * delta * speed;
		}

		if (Events::_cursor_locked){
			camY += -Events::deltaY / Window::height * 2;
			camX += -Events::deltaX / Window::height * 2;

			if (camY < -radians(89.0f)){
				camY = -radians(89.0f);
			}
			if (camY > radians(89.0f)){
				camY = radians(89.0f);
			}

			camera->rotation = mat4(1.0f);
			camera->rotate(camY, camX, 0);
		}

		{
			vec3 end;
			vec3 norm;
			vec3 iend;
			voxel* vox = chunks->rayCast(camera->position, camera->front, 10.0f, end, norm, iend);
			if (vox != nullptr){
				lineBatch->box(iend.x+0.5f, iend.y+0.5f, iend.z+0.5f, 1.005f,1.005f,1.005f, 0,0,0,0.5f);

				if (Events::jclicked(GLFW_MOUSE_BUTTON_1)){
					int x = (int)iend.x;
					int y = (int)iend.y;
					int z = (int)iend.z;
					chunks->set(x,y,z, 0);

					solverR->remove(x,y,z);
					solverG->remove(x,y,z);
					solverB->remove(x,y,z);

					solverR->solve();
					solverG->solve();
					solverB->solve();

					if (chunks->getLight(x,y+1,z, 3) == 0xF){
						for (int i = y; i >= 0; i--){
							if (chunks->get(x,i,z)->id != 0)
								break;
							solverS->add(x,i,z, 0xF);
						}
					}

					solverR->add(x,y+1,z); solverG->add(x,y+1,z); solverB->add(x,y+1,z); solverS->add(x,y+1,z);
					solverR->add(x,y-1,z); solverG->add(x,y-1,z); solverB->add(x,y-1,z); solverS->add(x,y-1,z);
					solverR->add(x+1,y,z); solverG->add(x+1,y,z); solverB->add(x+1,y,z); solverS->add(x+1,y,z);
					solverR->add(x-1,y,z); solverG->add(x-1,y,z); solverB->add(x-1,y,z); solverS->add(x-1,y,z);
					solverR->add(x,y,z+1); solverG->add(x,y,z+1); solverB->add(x,y,z+1); solverS->add(x,y,z+1);
					solverR->add(x,y,z-1); solverG->add(x,y,z-1); solverB->add(x,y,z-1); solverS->add(x,y,z-1);

					solverR->solve();
					solverG->solve();
					solverB->solve();
					solverS->solve();
				}
				if (Events::jclicked(GLFW_MOUSE_BUTTON_2)){
					int x = (int)(iend.x)+(int)(norm.x);
					int y = (int)(iend.y)+(int)(norm.y);
					int z = (int)(iend.z)+(int)(norm.z);
					chunks->set(x, y, z, choosenBlock);
					solverR->remove(x,y,z);
					solverG->remove(x,y,z);
					solverB->remove(x,y,z);
					solverS->remove(x,y,z);
					for (int i = y-1; i >= 0; i--){
						solverS->remove(x,i,z);
						if (i == 0 || chunks->get(x,i-1,z)->id != 0){
							break;
						}
					}
					solverR->solve();
					solverG->solve();
					solverB->solve();
					solverS->solve();
					if (choosenBlock == 3){
						solverR->add(x,y,z,10);
						solverG->add(x,y,z,10);
						solverB->add(x,y,z,0);
						solverR->solve();
						solverG->solve();
						solverB->solve();
					}
				}
			}
		}

		Chunk* closes[27];
		for (size_t i = 0; i < chunks->volume; i++){
			Chunk* chunk = chunks->chunks[i];
			if (!chunk->modified)
				continue;
			chunk->modified = false;
			if (meshes[i] != nullptr)
				delete meshes[i];

			for (int i = 0; i < 27; i++)
				closes[i] = nullptr;
			for (size_t j = 0; j < chunks->volume; j++){
				Chunk* other = chunks->chunks[j];

				int ox = other->x - chunk->x;
				int oy = other->y - chunk->y;
				int oz = other->z - chunk->z;

				if (abs(ox) > 1 || abs(oy) > 1 || abs(oz) > 1)
					continue;

				ox += 1;
				oy += 1;
				oz += 1;
				closes[(oy * 3 + oz) * 3 + ox] = other;
			}
			Mesh* mesh = renderer.render(chunk, (const Chunk**)closes);
			meshes[i] = mesh;
		}

		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		// Draw VAO
		shader->use();
		shader->uniformMatrix("projview", camera->getProjection()*camera->getView());
		texture->bind();
		mat4 model(1.0f);
		for (size_t i = 0; i < chunks->volume; i++){
			Chunk* chunk = chunks->chunks[i];
			Mesh* mesh = meshes[i];
			model = glm::translate(mat4(1.0f), vec3(chunk->x*CHUNK_W+0.5f, chunk->y*CHUNK_H+0.5f, chunk->z*CHUNK_D+0.5f));
			shader->uniformMatrix("model", model);
			mesh->draw(GL_TRIANGLES);
		}

		crosshairShader->use();
		crosshair->draw(GL_LINES);

		linesShader->use();
		linesShader->uniformMatrix("projview", camera->getProjection()*camera->getView());
		glLineWidth(2.0f);
		lineBatch->render();

		Window::swapBuffers();
		Events::pullEvents();
	}

	delete solverS;
	delete solverB;
	delete solverG;
	delete solverR;

	delete shader;
	delete texture;
	delete chunks;
	delete crosshair;
	delete crosshairShader;
	delete linesShader;
	delete lineBatch;

	Window::terminate();
	return 0;
}
