#define _CRT_SECURE_NO_WARNINGS
#include "iGraphics.h"
#include <stdio.h>
#include <string.h>
#include <windows.h>
#include <GL/gl.h>

// CONFIG 
const int W = 1000;
const int H = 600;

const float GRAV = 0.7f;
const float GROUND_Y = 80.0f;

// TEXTURES 
unsigned int texP1 = 0;
unsigned int texP2 = 0;

// BACKGROUNDS
const int BG_COUNT = 4;
const char* bgFiles[BG_COUNT] = {
	"darkcastlebg.jpg",
	"bg2.jpg",
	"bg3.jpg",
	"bg4.jpg"
};

unsigned int texBG[BG_COUNT] = { 0 };
int bgIndex = 0;                        // currently selected bg
char bgPathUsed[BG_COUNT][MAX_PATH];    // debug path


bool assetsLoaded = false;

// exe folder path
char gExeDir[MAX_PATH] = { 0 };
char p1PathUsed[MAX_PATH] = "none";
char p2PathUsed[MAX_PATH] = "none";

static int myFileExists(const char* path){
	FILE* fp = fopen(path, "rb");
	if (fp){ fclose(fp); return 1; }
	return 0;
}

static void getExeDir(){
	char full[MAX_PATH] = { 0 };
	GetModuleFileNameA(NULL, full, MAX_PATH);
	// strip filename
	char* last = strrchr(full, '\\');
	if (last) *last = '\0';
	strcpy(gExeDir, full);
}

static void joinPath(char* out, const char* dir, const char* file){
	strcpy(out, dir);
	strcat(out, "\\");
	strcat(out, file);
}

static unsigned int loadFirstMatch(char* outUsed, const char* const* list, int n){
	for (int i = 0; i < n; i++){
		if (myFileExists(list[i])){
			strcpy(outUsed, list[i]);
			return iLoadImage((char*)list[i]);
		}
	}
	return 0;
}

static void loadAssets(){

	char p1a[MAX_PATH], p1b[MAX_PATH], p2a[MAX_PATH], p2b[MAX_PATH];
	joinPath(p1a, gExeDir, "Intes.jpg");
	joinPath(p1b, gExeDir, "Intes.png");
	joinPath(p2a, gExeDir, "Mai.jpg");
	joinPath(p2b, gExeDir, "Mai.png");

	const char* p1List[] = {
		"Intes.jpg", "Intes.png",
		p1a, p1b,
		".\\Intes.jpg", ".\\Intes.png",
		"Debug\\Intes.jpg", "Debug\\Intes.png",
		"Debug\\Debug\\Intes.jpg", "Debug\\Debug\\Intes.png"
	};

	const char* p2List[] = {
		"Mai.jpg", "Mai.png",
		p2a, p2b,
		".\\Mai.jpg", ".\\Mai.png",
		"Debug\\Mai.jpg", "Debug\\Mai.png",
		"Debug\\Debug\\Mai.jpg", "Debug\\Debug\\Mai.png"
	};

	texP1 = loadFirstMatch(p1PathUsed, p1List, sizeof(p1List) / sizeof(p1List[0]));
	texP2 = loadFirstMatch(p2PathUsed, p2List, sizeof(p2List) / sizeof(p2List[0]));

	//  LOAD ALL BACKGROUNDS 
	for (int k = 0; k < BG_COUNT; k++){
		bgPathUsed[k][0] = '\0';

		char a[MAX_PATH], b[MAX_PATH];
		joinPath(a, gExeDir, bgFiles[k]);

		const char* list[] = {
			bgFiles[k],
			a,
			".\\darkcastlebg.jpg",
			"Debug\\darkcastlebg.jpg",
			"Debug\\Debug\\darkcastlebg.jpg"
		};


		const char* list2[] = {
			bgFiles[k],
			a,
			".\\",
		};


		const char* tryList[] = {
			bgFiles[k],
			a,
			".\\darkcastlebg.jpg",
			"Debug\\darkcastlebg.jpg",
			"Debug\\Debug\\darkcastlebg.jpg"
		};

		texBG[k] = loadFirstMatch(bgPathUsed[k], tryList, sizeof(tryList) / sizeof(tryList[0]));
	}

}

// DRAW TEXTURE (FIX FLIP + MIRROR)
static void drawTexture(int x, int y, int w, int h, unsigned int tex, bool flipY, bool mirrorX){
	if (!tex) return;

	glEnable(GL_TEXTURE_2D);
	glBindTexture(GL_TEXTURE_2D, tex);

	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);


	float u0 = mirrorX ? 1.0f : 0.0f;
	float u1 = mirrorX ? 0.0f : 1.0f;


	float v0 = flipY ? 1.0f : 0.0f;
	float v1 = flipY ? 0.0f : 1.0f;

	glBegin(GL_QUADS);

	glTexCoord2f(u0, v0); glVertex2f((float)x, (float)y);
	glTexCoord2f(u1, v0); glVertex2f((float)(x + w), (float)y);
	glTexCoord2f(u1, v1); glVertex2f((float)(x + w), (float)(y + h));
	glTexCoord2f(u0, v1); glVertex2f((float)x, (float)(y + h));

	glEnd();

	glDisable(GL_TEXTURE_2D);
}

// UTIL
float clampf(float v, float lo, float hi){ return (v<lo) ? lo : ((v>hi) ? hi : v); }
bool rectOverlap(float ax, float ay, int aw, int ah, float bx, float by, int bw, int bh){
	return (ax < bx + bw && ax + aw > bx && ay < by + bh && ay + ah > by);
}

//GAME STATE
enum GameState { STATE_SELECT, STATE_FIGHT };
GameState gState = STATE_SELECT;

//CHARACTERS
struct Character {
	char name[32];
	int maxHP;
	int damage;
};

Character chars[4];
int p1Char = 0;
int p2Char = 1;
bool p1Locked = false;
bool p2Locked = false;

void initCharacters(){
	sprintf(chars[0].name, "Intes");  chars[0].maxHP = 120; chars[0].damage = 10;
	sprintf(chars[1].name, "Ran");    chars[1].maxHP = 110; chars[1].damage = 12;
	sprintf(chars[2].name, "Mai");    chars[2].maxHP = 95;  chars[2].damage = 9;
	sprintf(chars[3].name, "Ryo");    chars[3].maxHP = 140; chars[3].damage = 8;
}

//FIGHT DATA
struct Fighter{
	float x, y, vx, vy;
	int w, h;
	int hp, maxHP, damage;
	bool onGround;
	bool facingRight;
	int hurtCD, projCD;
};

struct Projectile{
	float x, y, vx;
	int w, h, dmg;
	bool active;
	int owner;
};

Fighter P1, P2;
Projectile proj[6];
int winner = 0;

//INIT MATCH
void resetProjectiles(){
	for (int i = 0; i<6; i++) proj[i].active = false;
}

void setupFighter(Fighter &F, int cidx, int side){

	F.w = 60; F.h = 120;
	F.y = GROUND_Y; F.vx = 0; F.vy = 0;

	if (side == 1){ F.x = 220; F.facingRight = true; }
	else          { F.x = 720; F.facingRight = false; }

	F.maxHP = chars[cidx].maxHP;
	F.hp = F.maxHP;
	F.damage = chars[cidx].damage;

	F.onGround = true;
	F.hurtCD = 0;
	F.projCD = 0;
}

void startMatch(){
	setupFighter(P1, p1Char, 1);
	setupFighter(P2, p2Char, 2);
	resetProjectiles();
	winner = 0;
	gState = STATE_FIGHT;
}

//COMBAT
void applyHit(Fighter &v, int dmg){
	if (v.hurtCD>0) return;
	v.hp -= dmg;
	if (v.hp<0) v.hp = 0;
	v.hurtCD = 15;
}

void spawnProjectile(int owner){
	Fighter &A = (owner == 1) ? P1 : P2;
	if (A.projCD>0) return;
	A.projCD = 45;

	for (int i = 0; i<6; i++){
		if (!proj[i].active){
			proj[i].active = true;
			proj[i].owner = owner;
			proj[i].w = 18; proj[i].h = 10;
			proj[i].dmg = 14;
			proj[i].y = A.y + A.h*0.55f;

			if (A.facingRight){
				proj[i].x = A.x + A.w + 5;
				proj[i].vx = 10;
			}
			else{
				proj[i].x = A.x - proj[i].w - 5;
				proj[i].vx = -10;
			}
			return;
		}
	}
}

//UPDATE
void updateProjectiles(){
	for (int i = 0; i<6; i++){
		if (!proj[i].active) continue;
		proj[i].x += proj[i].vx;

		if (proj[i].x < -50 || proj[i].x > W + 50){
			proj[i].active = false;
			continue;
		}

		Fighter &B = (proj[i].owner == 1) ? P2 : P1;
		if (rectOverlap(proj[i].x, proj[i].y, proj[i].w, proj[i].h, B.x, B.y, B.w, B.h)){
			applyHit(B, proj[i].dmg);
			proj[i].active = false;
		}
	}
}

void updatePhysics(Fighter &F){
	if (!F.onGround) F.vy -= GRAV;
	F.y += F.vy;

	if (F.y <= GROUND_Y){
		F.y = GROUND_Y;
		F.vy = 0;
		F.onGround = true;
	}

	F.x += F.vx;
	F.vx *= 0.80f;

	F.x = clampf(F.x, 0, (float)(W - F.w));

	if (F.hurtCD>0) F.hurtCD--;
	if (F.projCD>0) F.projCD--;
}

//SELECTION INPUT
int prevA = 0, prevD = 0, prevF = 0, prev0 = 0, prevL = 0, prevR = 0;

bool pressedEdge(int now, int &prev){
	bool edge = (now && !prev);
	prev = now;
	return edge;
}

void updateSelect(){
	int nowA = (isKeyPressed('a') || isKeyPressed('A')) ? 1 : 0;
	int nowD = (isKeyPressed('d') || isKeyPressed('D')) ? 1 : 0;
	int nowF = (isKeyPressed('f') || isKeyPressed('F')) ? 1 : 0;

	if (!p1Locked){
		if (pressedEdge(nowA, prevA)) p1Char = (p1Char + 4 - 1) % 4;
		if (pressedEdge(nowD, prevD)) p1Char = (p1Char + 1) % 4;
		if (pressedEdge(nowF, prevF)) p1Locked = true;
	}
	else{
		if (pressedEdge(nowF, prevF)) p1Locked = false;
	}

	int nowL = isSpecialKeyPressed(GLUT_KEY_LEFT) ? 1 : 0;
	int nowR = isSpecialKeyPressed(GLUT_KEY_RIGHT) ? 1 : 0;
	int now0 = isKeyPressed('0') ? 1 : 0;

	if (!p2Locked){
		if (pressedEdge(nowL, prevL)) p2Char = (p2Char + 4 - 1) % 4;
		if (pressedEdge(nowR, prevR)) p2Char = (p2Char + 1) % 4;
		if (pressedEdge(now0, prev0)) p2Locked = true;
	}
	else{
		if (pressedEdge(now0, prev0)) p2Locked = false;
	}

	if (p1Locked && p2Locked) startMatch();

	// BG change: [ and ]
	static int prevLB = 0, prevRB = 0;
	int nowLB = isKeyPressed('[') ? 1 : 0;
	int nowRB = isKeyPressed(']') ? 1 : 0;

	if (pressedEdge(nowLB, prevLB)){
		bgIndex = (bgIndex + BG_COUNT - 1) % BG_COUNT;
	}
	if (pressedEdge(nowRB, prevRB)){
		bgIndex = (bgIndex + 1) % BG_COUNT;
	}

}

void updateFight(){
	if (winner) return;

	float speed = 6;

	// P1
	if (isKeyPressed('a') || isKeyPressed('A')) { P1.vx = -speed; P1.facingRight = false; }
	if (isKeyPressed('d') || isKeyPressed('D')) { P1.vx = speed;  P1.facingRight = true; }
	if ((isKeyPressed('w') || isKeyPressed('W')) && P1.onGround){ P1.vy = 13.5f; P1.onGround = false; }
	if (isKeyPressed('g') || isKeyPressed('G')) spawnProjectile(1);

	// P2
	if (isSpecialKeyPressed(GLUT_KEY_LEFT))  { P2.vx = -speed; P2.facingRight = false; }
	if (isSpecialKeyPressed(GLUT_KEY_RIGHT)) { P2.vx = speed;  P2.facingRight = true; }
	if (isSpecialKeyPressed(GLUT_KEY_UP) && P2.onGround){ P2.vy = 13.5f; P2.onGround = false; }
	if (isKeyPressed('5')) spawnProjectile(2);

	updatePhysics(P1);
	updatePhysics(P2);
	updateProjectiles();

	if (P1.hp <= 0) winner = 2;
	if (P2.hp <= 0) winner = 1;
}

void fixedUpdate(){
	if (gState == STATE_SELECT) updateSelect();
	else updateFight();
}

// DRAW HELPERS
void drawHealthBar(int x, int y, int w, int h, int hp, int maxHP){
	iRectangle(x, y, w, h);
	float r = (maxHP>0) ? (float)hp / (float)maxHP : 0.0f;
	r = clampf(r, 0.0f, 1.0f);
	iSetColor(255 * (1.0f - r), 255 * r, 0);
	iFilledRectangle(x, y, (int)(w*r), h);
	iSetColor(255, 255, 255);
}

//DRAW
void drawSelectScreen(){
	if (texBG[bgIndex])
		drawTexture(0, 0, W, H, texBG[bgIndex], true, false);

	iSetColor(255, 255, 255);
	iText(W / 2 - 120, H - 60, (char*)"CHARACTER SELECT", GLUT_BITMAP_HELVETICA_18);

	// P1 box
	iRectangle(120, 250, 300, 200);
	iText(130, 420, (char*)"P1: A/D choose, F lock", GLUT_BITMAP_HELVETICA_12);
	iText(130, 380, (char*)"Selected:", GLUT_BITMAP_HELVETICA_12);
	iText(210, 380, chars[p1Char].name, GLUT_BITMAP_HELVETICA_18);

	// P2 box
	iRectangle(580, 250, 300, 200);
	iText(590, 420, (char*)"P2: <-/-> choose, 0 lock", GLUT_BITMAP_HELVETICA_12);
	iText(590, 380, (char*)"Selected:", GLUT_BITMAP_HELVETICA_12);
	iText(670, 380, chars[p2Char].name, GLUT_BITMAP_HELVETICA_18);

	// preview 
	if (texP1) drawTexture(320, 270, 90, 130, texP1, true, false);
	if (texP2) drawTexture(780, 270, 90, 130, texP2, true, true);
	// mirror for P2

	// debug text
	char dbg[400];
	sprintf(dbg, "texP1=%u (%s) | texP2=%u (%s)", texP1, p1PathUsed, texP2, p2PathUsed);
	iText(20, H - 120, dbg, GLUT_BITMAP_HELVETICA_12);

	iSetColor(255, 255, 255);
	char bgtxt[128];
	sprintf(bgtxt, "BG: %s   ([ / ] to change)", bgFiles[bgIndex]);
	iText(20, 20, bgtxt, GLUT_BITMAP_HELVETICA_12);

}

void drawFightScreen(){
	if (texBG[bgIndex])
		drawTexture(0, 0, W, H, texBG[bgIndex], true, false);

	// ground
	//iSetColor(255, 255, 255);
	//iFilledRectangle(0, 0, W, (int)GROUND_Y);

	// HP bars
	iSetColor(255, 255, 255);
	iText(20, H - 40, (char*)"P1 HP", GLUT_BITMAP_HELVETICA_12);
	drawHealthBar(80, H - 45, 300, 18, P1.hp, P1.maxHP);

	iText(W - 380, H - 40, (char*)"P2 HP", GLUT_BITMAP_HELVETICA_12);
	drawHealthBar(W - 300, H - 45, 300, 18, P2.hp, P2.maxHP);

	// names
	iText(80, H - 70, chars[p1Char].name, GLUT_BITMAP_HELVETICA_12);
	iText(W - 300, H - 70, chars[p2Char].name, GLUT_BITMAP_HELVETICA_12);

	//fighters image size
	int drawW = 150;
	int drawH = 130;

	//P1 draw (
	if (texP1) drawTexture((int)P1.x, (int)P1.y, drawW, drawH, texP1, true, false);
	else iRectangle((int)P1.x, (int)P1.y, P1.w, P1.h);

	// P2 draw
	if (texP2) drawTexture((int)P2.x, (int)P2.y, drawW, drawH, texP2, true, true);
	else iRectangle((int)P2.x, (int)P2.y, P2.w, P2.h);

	// projectiles
	iSetColor(255, 255, 255);
	for (int i = 0; i<6; i++){
		if (proj[i].active) iRectangle((int)proj[i].x, (int)proj[i].y, proj[i].w, proj[i].h);
	}

	if (winner == 1) iText(W / 2 - 60, H / 2, (char*)"P1 WINS!", GLUT_BITMAP_HELVETICA_18);
	if (winner == 2) iText(W / 2 - 60, H / 2, (char*)"P2 WINS!", GLUT_BITMAP_HELVETICA_18);
}

void iDraw(){
	if (!assetsLoaded){
		getExeDir();
		loadAssets();
		assetsLoaded = true;
	}

	iClear();
	if (gState == STATE_SELECT) drawSelectScreen();
	else drawFightScreen();
}

//REQUIRED CALLBACKS
void iMouseMove(int mx, int my){}
void iMouse(int button, int state, int mx, int my){}
void iPassiveMouseMove(int mx, int my){}

int main(){
	initCharacters();

	iInitialize(W, H, (char*)"Myths Unleashed");

	gState = STATE_SELECT;
	p1Locked = p2Locked = false;
	winner = 0;

	iStart();
	return 0;
}
