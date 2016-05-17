/*********************************************************************
How to compile this on different platforms:

gcc Main.c DrawUtils.c `pkg-config --cflags --libs sdl2 gl glew` -o CFramework
*/
#define SDL_MAIN_HANDLED
#include<SDL.h>
#include<GL/glew.h>
#include<stdio.h>
#include<string.h>
#include<assert.h>
#include"DrawUtils.h"
#include<math.h>
struct Camera {
	int x;
	int y;
};

// Define a single frame used in an animation
struct AnimFrameDef {
	// combined with the AnimDef's name to make
	// the actual texture name
	int image;
	float frameTimeSecs;
};

struct AnimDef {
	const char* name;
	struct AnimFrameDef frames[20];
	int numFrames;
};

// Runtime state for an animation
struct AnimData {
	struct AnimDef def;
	int curFrame;
	float timeToNextFrame;
	bool isPlaying;
};

struct TileDef {
	int tile;
	bool hasCollision;
};

struct BackgroundDef {
	int width;
	int height;
	struct TileDef tiles[100];
};

struct AABB {
	int x, y, w, h;
};

struct Shape {
	int x;
	int y;
	int r;
};

struct SpriteData {
	int spritePos[2];
	int spriteIndex[2];
	int velocity[2];
	int size[2];
	GLuint texture;
	struct AnimData adata;
	struct Shape collider;
	struct AABB spriteBox;
	bool collision;
	bool isAlive;
};

/* Set this to true to force the game to exit */
char shouldExit = 0;

/* The previous frame's keyboard state */
unsigned char kbPrevState[SDL_NUM_SCANCODES] = { 0 };

/* The current frame's keyboard state */
const unsigned char* kbState = NULL;

/* Size of screen*/
const int SCREEN_HEIGHT = 480;
const int SCREEN_WIDTH = 640;
const int LEVEL_HEIGHT = 1280;
const int LEVEL_WIDTH = 1280;
const int TILE_SIZE = 32;
struct TileDef tileIndex[40][40];

/* position of the sprite */
int backgroundPos[2] = { 0, 0 };
int dirtbg1Pos[2] = { 100, 100 };
int floorTile1Pos[2] = { 0, 0 };
/* Texture for the sprite */
GLuint spriteTex;
GLuint spriteTex2;
GLuint backgroundTex;
GLuint dirtbg1Tex;
GLuint floortile1Tex;
/* size of the sprite */
int spriteSize[2];
int backgroundSize[2];
int dirtbg1Size[2];
int floorTile1Size[2];

bool AABBIntersect(const struct AABB* box1, const struct AABB* box2)
{
	// box1 to the right
	if (box1->x > box2->x + box2->w) {
		return false;
	}
	// box1 to the left
	if (box1->x + box1->w < box2->x) {
		return false;
	}
	// box1 below
	if (box1->y > box2->y + box2->h) {
		return false;
	}
	// box1 above
	if (box1->y + box1->h < box2->y) {
		return false;
	}
	return true;
}

void initializeTile(struct AABB tile, int h, int w, int x, int y) {
	tile.h = h;
	tile.w = w;
	tile.x = x;
	tile.y = y;
}

int sq(int n) {
	return n*n;
}

int main(void)
{
	/* Initialize SDL */
	if (SDL_Init(SDL_INIT_VIDEO) < 0) {
		fprintf(stderr, "Could not initialize SDL. ErrorCode=%s\n", SDL_GetError());
		return 1;
	}

	/* Create the window, OpenGL context */
	SDL_GL_SetAttribute(SDL_GL_BUFFER_SIZE, 32);
	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
	SDL_Window* window = SDL_CreateWindow(
		"TestSDL",
		SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
		640, 480,
		SDL_WINDOW_OPENGL);
	if (!window) {
		fprintf(stderr, "Could not create window. ErrorCode=%s\n", SDL_GetError());
		SDL_Quit();
		return 1;
	}
	SDL_GL_CreateContext(window);

	/* Make sure we have a recent version of OpenGL */
	GLenum glewError = glewInit();
	if (glewError != GLEW_OK) {
		fprintf(stderr, "Could not initialize glew. ErrorCode=%s\n", glewGetErrorString(glewError));
		SDL_Quit();
		return 1;
	}
	if (GLEW_VERSION_3_0) {
		fprintf(stderr, "OpenGL 3.0 or greater supported: Version=%s\n",
			glGetString(GL_VERSION));
	}
	else {
		fprintf(stderr, "OpenGL max supported version is too low.\n");
		SDL_Quit();
		return 1;
	}

	/* Setup OpenGL state */
	glViewport(0, 0, 640, 480);
	glMatrixMode(GL_PROJECTION);
	glOrtho(0, 640, 480, 0, 0, 100);
	glEnable(GL_TEXTURE_2D);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	/* Load the texture */
	floortile1Tex = glTexImageTGAFile("floortile1.tga", &floorTile1Size[0], &floorTile1Size[1]);
	dirtbg1Tex = glTexImageTGAFile("dirt1.tga", &dirtbg1Size[0], &dirtbg1Size[1]);
	backgroundTex = glTexImageTGAFile("floortile2.tga", &backgroundSize[0], &backgroundSize[1]);
	spriteTex = glTexImageTGAFile("spriteFrame1.tga", &spriteSize[0], &spriteSize[1]);
	spriteTex2 = glTexImageTGAFile("spriteFrame2.tga", &spriteSize[0], &spriteSize[1]);
	struct SpriteData projectile;
	projectile.isAlive = false;
	projectile.collision = false;
	projectile.collider.r = 16;
	int projectileDuration = 3;
	projectile.spritePos[0] = -10;
	projectile.spritePos[1] = -10;
	projectile.texture = glTexImageTGAFile("projectile.tga", &projectile.size[0], &projectile.size[1]);
	struct SpriteData enemy;
	enemy.collider.r = 16;
	enemy.isAlive = true;
	enemy.collision = false;
	enemy.spritePos[0] = 200;
	enemy.spritePos[1] = 200;

	enemy.texture = glTexImageTGAFile("enemy.tga", &enemy.size[0], &enemy.size[1]);
	struct SpriteData player;
	player.isAlive = true;
	player.collision = false;
	player.spritePos[0] = 0;
	player.spritePos[1] = 0;
	player.spriteIndex[0] = 0;
	player.spriteIndex[1] = 0;
	player.texture = glTexImageTGAFile("spriteFrame1.tga", &player.size[0], &player.size[1]);

	struct AnimDef playerIdleDef;
	playerIdleDef.frames[0].image = spriteTex;
	playerIdleDef.frames[1].image = spriteTex2;
	playerIdleDef.frames[0].frameTimeSecs = 1;
	playerIdleDef.frames[1].frameTimeSecs = 1;
	playerIdleDef.numFrames = 2;
	playerIdleDef.name = "idle";

	player.adata.def = playerIdleDef;
	player.adata.curFrame = 0;
	player.adata.isPlaying = true;
	player.adata.timeToNextFrame = 1;

	player.velocity[0] = 0;
	player.velocity[1] = 0;
	enemy.velocity[0] = 1;
	enemy.velocity[1] = 0;
	projectile.velocity[0] = 0;
	projectile.velocity[1] = 0;

	struct BackgroundDef floortile1BG;
	floortile1BG.height = LEVEL_HEIGHT;
	floortile1BG.width = LEVEL_WIDTH;
	struct BackgroundDef bg;
	bg.height = 1280;
	bg.width = 1280;

	struct TileDef floorTile;
	floorTile.tile = floortile1Tex;
	floorTile.hasCollision = true;

	struct TileDef dirtTile;
	dirtTile.tile = dirtbg1Tex;
	dirtTile.hasCollision = false;

	struct TileDef bgTile;
	bgTile.tile = backgroundTex;
	bgTile.hasCollision = true;

	bg.tiles[1] = floorTile;
	bg.tiles[0] = dirtTile;
	bg.tiles[2] = bgTile;
	
	kbState = SDL_GetKeyboardState(NULL);

	Uint32 lastFrameMs;
	Uint32 currentFrameMs = SDL_GetTicks();

	struct Camera camera;
	camera.x = 0;
	camera.y = 0;

	struct AABB cameraBox;
	cameraBox.h = SCREEN_HEIGHT;
	cameraBox.w = SCREEN_WIDTH;
	cameraBox.x = camera.x;
	cameraBox.y = camera.y;

	player.spriteBox.h = player.size[0];
	player.spriteBox.w = player.size[1];
	enemy.spriteBox.h = enemy.size[0];
	enemy.spriteBox.w = enemy.size[1];
	projectile.spriteBox.h = projectile.size[0];
	projectile.spriteBox.w = projectile.size[1];

	//Tile indexes
	int b, c = 0;
	for (b = 0; b < 40; b++) {
		c = 0;
		for (c = 0; c < 40; c++) {
			tileIndex[b][c] = bg.tiles[0];
		}
	}

	for (b = 20; b < 40; b++) {
		for (c = 30; c < 40; c++) {
			tileIndex[b][c] = bg.tiles[1];
		}
	}

	int physicsDeltaMs = 10;
	int lastPhysicsFrameMs = 0;
	int lastPlayerPos[2];

	/* The game loop */
	while (!shouldExit) {
		assert(glGetError() == GL_NO_ERROR);
		memcpy(kbPrevState, kbState, sizeof(kbPrevState));

		lastFrameMs = currentFrameMs;
		
		/* Handle OS message pump */
		SDL_Event event;
		while (SDL_PollEvent(&event)) {
			switch (event.type) {
			case SDL_QUIT:
				shouldExit = 1;
			}
		}



		// Physics update
		do {
			// 1. Physics movement
			player.spritePos[0] += player.velocity[0];
			player.spritePos[1] += player.velocity[1];
			enemy.spritePos[0] += enemy.velocity[0];
			enemy.spritePos[1] += enemy.velocity[1];
			if (projectile.isAlive) {
				projectile.spritePos[0] += projectile.velocity[0];
				projectile.spritePos[1] += projectile.velocity[1];
			}
			// 2. Physics collision detection
			enemy.collider.x = enemy.spritePos[0];
			enemy.collider.y = enemy.spritePos[1];
			projectile.collider.x = projectile.spritePos[0];
			projectile.collider.y = projectile.spritePos[1];

			if (projectile.isAlive && enemy.isAlive) {
				if (sq(projectile.collider.x - enemy.collider.x) + sq(projectile.collider.y - enemy.collider.y) < sq(projectile.collider.r + enemy.collider.r)) {
					printf("Is %d less than %d\n", (sq(projectile.collider.x - enemy.collider.x) + sq(projectile.collider.y - enemy.collider.y)), (sq(projectile.collider.r + enemy.collider.r)));
					projectile.collision = true;
					enemy.collision = true;
				}
				else {
					projectile.collision = false;
					enemy.collision = false;
				}
			}
			// 3. Physics collision resolution
			if (projectile.collision) {
				projectile.isAlive = false;
				projectile.collision = false;
			}
			if (enemy.collision) {
				enemy.isAlive = false;
				enemy.collision = false;
			}

			// 4. Background Resolution
			player.spriteIndex[0] = player.spritePos[0] / 32;
			player.spriteIndex[1] = player.spritePos[1] / 32;

			if (player.isAlive) {
				if (tileIndex[player.spriteIndex[0]][player.spriteIndex[1]].hasCollision ||
					tileIndex[player.spriteIndex[0] - 1][player.spriteIndex[1]].hasCollision ||
					tileIndex[player.spriteIndex[0] + 1][player.spriteIndex[1]].hasCollision ||
					tileIndex[player.spriteIndex[0]][player.spriteIndex[1] - 1].hasCollision ||
					tileIndex[player.spriteIndex[0]][player.spriteIndex[1] + 1].hasCollision) {
					struct AABB topTile2 = { .h = TILE_SIZE,.w = TILE_SIZE,.x = player.spritePos[0], .y = player.spritePos[1] - 16 };
					struct AABB bottomTile2 = { .h = TILE_SIZE,.w = TILE_SIZE,.x = player.spritePos[0],.y = player.spritePos[1] + 16 };
					struct AABB rightTile2 = { .h = TILE_SIZE,.w = TILE_SIZE,.x = player.spritePos[0] + 16,.y = player.spritePos[1] };
					struct AABB leftTile2 = { .h = TILE_SIZE,.w = TILE_SIZE,.x = player.spritePos[0] - 16,.y = player.spritePos[1] };
					struct AABB currentTile2 = { .h = TILE_SIZE,.w = TILE_SIZE,.x = player.spritePos[0],.y = player.spritePos[1] };

					if (AABBIntersect(&topTile2, &player.spriteBox)) {
						//printf("top\n");
						player.collision = true;
					} else if (AABBIntersect(&currentTile2, &player.spriteBox)) {
						//printf("cur\n");
						player.collision = true;
					} else if (AABBIntersect(&bottomTile2, &player.spriteBox)) {
						//printf("bot\n");
						player.collision = true;
					} else if (AABBIntersect(&rightTile2, &player.spriteBox)) {
						//printf("r\n");
						player.collision = true;
					} else if (AABBIntersect(&leftTile2, &player.spriteBox)) {
						//printf("l\n");
						player.collision = true;
					}
					else {
						player.collision = false;
					}

					if (player.collision) {
						//printf("x: %d, y: %d", player.spritePos[0], player.spritePos[1]);
						player.spritePos[0] += player.velocity[0] * -16;
						player.spritePos[1] += player.velocity[1] * -16;
					}
				}
			}

			if (player.collision) {
			}

			lastPhysicsFrameMs += physicsDeltaMs;
		} while (lastPhysicsFrameMs + physicsDeltaMs < currentFrameMs);





		currentFrameMs = SDL_GetTicks();
		float deltaTime = (currentFrameMs - lastFrameMs) / 1000.0f;
		/* Game logic */
		if (kbState[SDL_SCANCODE_ESCAPE]) {
			shouldExit = 1;
		}

		/* Sprite Controls Arrow Keys */
		if (kbState[SDL_SCANCODE_LEFT] && player.spritePos[0] > 0) {
			player.velocity[0] = -1;
		} else if (kbState[SDL_SCANCODE_RIGHT] && player.spritePos[0] < camera.x + SCREEN_WIDTH - player.size[0]) {
			player.velocity[0] = 1;
		}
		else {
			player.velocity[0] = 0;
		}
		if (kbState[SDL_SCANCODE_UP] && player.spritePos[1] > 0) {
			player.velocity[1] = -1;
		}
		else if (kbState[SDL_SCANCODE_DOWN] && player.spritePos[1] < camera.y + SCREEN_HEIGHT - player.size[1]) {
			player.velocity[1] = 1;
		}
		else {
			player.velocity[1] = 0;
		}
		if (kbState[SDL_SCANCODE_SPACE]) {
			projectile.spritePos[0] = player.spritePos[0] + 1;
			projectile.spritePos[1] = player.spritePos[1];
			projectile.velocity[0] = 1;
			projectileDuration = 500;
			projectile.isAlive = true;
		}
		else if (projectile.isAlive) {
			projectileDuration -= deltaTime;
		}
		if (projectileDuration <= 0) {
			projectileDuration = 500;
			projectile.isAlive = false;
		}

		/* Enemy Logic */
		if (enemy.spritePos[0] >= 400) {
			enemy.velocity[0] = -1;
		}else if (enemy.spritePos[0] <= 100) {
			enemy.velocity[0] = 1;
		}

		/* Camera Controls WASD */
		if (kbState[SDL_SCANCODE_A] && camera.x > 0) {
			camera.x -= 1;
		}
		if (kbState[SDL_SCANCODE_D] && camera.x < LEVEL_WIDTH - SCREEN_WIDTH) {
			camera.x += 1;
		}
		if (kbState[SDL_SCANCODE_W] && camera.y > 0) {
			camera.y -= 1;
		}
		if (kbState[SDL_SCANCODE_S] && camera.y < LEVEL_HEIGHT - SCREEN_HEIGHT) {
			camera.y += 1;
		}

		if (player.adata.isPlaying) {
			int numFrames = player.adata.def.numFrames;
			player.adata.timeToNextFrame -= deltaTime;
			if (player.adata.timeToNextFrame < 0) {
				player.adata.curFrame++;
				if (player.adata.curFrame >= numFrames) {
					player.adata.curFrame = numFrames--;
					player.adata.timeToNextFrame = 1;
					player.adata.curFrame = 0;
				}
				else {
					struct AnimFrameDef curFrame = player.adata.def.frames[player.adata.curFrame];
					player.adata.timeToNextFrame += curFrame.frameTimeSecs;
				}
			}
		}

		int start_x = floor(camera.x / TILE_SIZE);
		int start_y = floor(camera.y / TILE_SIZE);
		int end_x = floor((SCREEN_WIDTH + camera.x) / TILE_SIZE);
		int end_y = floor((SCREEN_HEIGHT + camera.y) / TILE_SIZE);

		cameraBox.x = camera.x;
		cameraBox.y = camera.y;
		player.spriteBox.x = player.spritePos[0];
		player.spriteBox.y = player.spritePos[1];
		enemy.spriteBox.x = enemy.spritePos[0];
		enemy.spriteBox.y = enemy.spritePos[1];
		projectile.spriteBox.x = projectile.spritePos[0];
		projectile.spriteBox.y = projectile.spritePos[1];

		glClearColor(0, 0, 0, 1);
		glClear(GL_COLOR_BUFFER_BIT);


		/* Background Optimization #3 */
		int r = 0;
		for (r = start_x; r < end_x; r++) {
			int d = 0;
			for (d = start_y; d < end_y; d++) {
				glDrawSprite(tileIndex[r][d].tile, -camera.x + r * TILE_SIZE, -camera.y + d * TILE_SIZE, TILE_SIZE, TILE_SIZE);
			}
		}

		/* Optimization AABB #2 */
		//Player Sprite
		if (player.isAlive) {
			if (AABBIntersect(&cameraBox, &player.spriteBox)) {
				glDrawSprite(player.adata.def.frames[player.adata.curFrame].image, player.spritePos[0] - camera.x, player.spritePos[1] - camera.y, player.size[0], player.size[1]);
			}
		}
		if (enemy.isAlive) {
			if (AABBIntersect(&cameraBox, &enemy.spriteBox)) {
				glDrawSprite(enemy.texture, enemy.spritePos[0] - camera.x, enemy.spritePos[1] - camera.y, enemy.size[0], enemy.size[1]);
			}
		}

		if (projectile.isAlive) {
			glDrawSprite(projectile.texture, projectile.spritePos[0] - camera.x, projectile.spritePos[1] - camera.y, projectile.size[0], projectile.size[1]);
		}

		/* Present to the player */
		SDL_GL_SwapWindow(window);
	}

	SDL_Quit();

	return 0;
}
