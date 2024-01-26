#include <stdlib.h>
#include <stdio.h>

#include <nds.h>
#include <filesystem.h>

void loadTexture(char* file, int* tex1, int* tex2, int* w, int* h) {
	//Load file
	//I use assert because I dont care
	FILE* inf = fopen(file, "rb");
	assert(inf);
	
	fseek(inf, 0, SEEK_END);
	int len = ftell(inf);
	fseek(inf, 0, SEEK_SET);
	
	char* data = malloc(len);
	assert(data);
	
	int read = fread(data, 1, len, inf);
	assert(read == len);
	
	fclose(inf);
	
	//Read texture size
	int texW = data[0] & 0xff, texH = data[1] & 0xff;
	*w = 1 << (texW + 3);
	*h = 1 << (texH + 3);
	
	char* palette = data + 2 + (*w)*(*h)*2;
	
	//Create textures
	//One texture with d2 alpha factor, second with d3 alpha factor
	glGenTextures(1, tex1);
	glBindTexture(0, *tex1);
	
	glTexImage2D(0, 0, GL_RGB32_A3, TEXTURE_SIZE_8 + texW,  TEXTURE_SIZE_8 + texH, 0, TEXGEN_TEXCOORD, data + 2);
	glColorTableEXT(0, 0, 256, 0, 0, (void*) palette);
	
	glGenTextures(1, tex2);
	glBindTexture(0, *tex2);
	
	glTexImage2D(0, 0, GL_RGB32_A3, TEXTURE_SIZE_8 + texW,  TEXTURE_SIZE_8 + texH, 0, TEXGEN_TEXCOORD, data + 2 + (*w)*(*h));
	glColorTableEXT(0, 0, 256, 0, 0, (void*) palette);
	
	glBindTexture(0, 0);
}

int min(int a, int b) {
	return a < b ? a : b;
}

int max(int a, int b) {
	return a > b ? a : b;
}

int32 l1Pos[3] = {-4096, 3172, -4096};
int32 l1Col[3] = {4096 * 2, 4096 * 2, 4096 * 2};

int32 l2Pos[3] = {4096, 4096, 4096};
int32 l2Col[3] = {4096 * 2, 0, 0};

void calcLight(
		int32* col, 
		int32 x, int32 y, int32 z, 
		int32 tNormX, int32 tNormY, int32 tNormZ, 
		int32* lightPos, int32* lightCol
	) {
	
	int32 lightDir[3] = {lightPos[0] - x, lightPos[1] - y, lightPos[2] - z};
	int32 dist = dotf32(lightDir, lightDir);
	
	//Normalize
	lightDir[0] = divf32(lightDir[0], dist);
	lightDir[1] = divf32(lightDir[1], dist);
	lightDir[2] = divf32(lightDir[2], dist);
	
	//Convert light direction to tangent space
	//This is just a dirty hack, the actual conversion is much more complicated
	//just swap z and y direction :)
	int32 tmp = lightDir[1];
	lightDir[1] = lightDir[2];
	lightDir[2] = tmp;
	
	//Surface normal in tangent space
	int32 tNorm[3] = {tNormX, tNormY, tNormZ};
	
	//dot :)
	int32 intensity = max(0, dotf32(tNorm, lightDir));
	//distance attenuation
	intensity = divf32(intensity, mulf32(dist, dist));
	
	//Add color to result
	col[0] += mulf32(lightCol[0], intensity);
	col[1] += mulf32(lightCol[1], intensity);
	col[2] += mulf32(lightCol[2], intensity);
}

//surface normal is in tangent space
rgb calcLights(int32 x, int32 y, int32 z, int32 tNormX, int32 tNormY, int32 tNormZ) {
	int32 result[3] = {0, 0, 0};
	
	//Add lights to result
	calcLight(result, x, y, z, tNormX, tNormY, tNormZ, l1Pos, l1Col);
	calcLight(result, x, y, z, tNormX, tNormY, tNormZ, l2Pos, l2Col);
	
	//attempt at good looking brigthness overflow
	/*int32 m = max(result[0], max(result[1], result[2]));
	if(m > 4096) {
		result[0] = divf32(result[0], m);
		result[1] = divf32(result[1], m);
		result[2] = divf32(result[2], m);
	}*/
	
	//Convert to srgb color space (light math are performed in linear color space!)
	//the actual formula is x^(1/2.2) but x^(1/2) is okay too for DS
	result[0] = sqrtf32(result[0]);
	result[1] = sqrtf32(result[1]);
	result[2] = sqrtf32(result[2]);
	
	//12bit -> 5bit conversion with rounding
	result[0] = min(31, (result[0] + 64) >> 7);
	result[1] = min(31, (result[1] + 64) >> 7);
	result[2] = min(31, (result[2] + 64) >> 7);
	
	return (result[2] << 10) | (result[1] << 5) | (result[0]);
}

void addPlane(v16 minX, v16 maxX, v16 y, v16 minZ, v16 maxZ, int32 tNormX, int32 tNormY, int32 tNormZ, int32 texW, int32 texH) {
	//Actually!!! Vertex colors should be precalculated!!!
	//With enough effort this can be achieved with Blender built-in vertex color baking using scripting api
	
	glColor(calcLights(minX, y, maxZ, tNormX, tNormY, tNormZ));
	glTexCoord2t16(0, inttot16(texH));
	glVertex3v16(minX, y, maxZ);
	
	glColor(calcLights(maxX, y, maxZ, tNormX, tNormY, tNormZ));
	glTexCoord2t16(inttot16(texW), inttot16(texH));
	glVertex3v16(maxX, y, maxZ);
	
	glColor(calcLights(maxX, y, minZ, tNormX, tNormY, tNormZ));
	glTexCoord2t16(inttot16(texW), 0);
	glVertex3v16(maxX, y, minZ);
	
	glColor(calcLights(minX, y, minZ, tNormX, tNormY, tNormZ));
	glTexCoord2t16(0, 0);
	glVertex3v16(minX, y, minZ);
}

void renderFloor(int texW, int texH, int32 tNormX, int32 tNormY, int32 tNormZ) {  
	glBegin(GL_QUADS);		
	
	//3*3 quad floor
	for(int xx=0; xx<3; xx++) {
		for(int zz=0; zz<3; zz++) {
			v16 offsetX = (xx - 1) * 4096;
			v16 offsetZ = (zz - 1) * 4096;
			addPlane(
				-2048 + offsetX, 2048 + offsetX, 0, -2048 + offsetZ, 2048 + offsetZ, 
				tNormX, tNormY, tNormZ,
				texW, texH
			);
		}
	}
	
	glEnd();
}

void renderCube(rgb color){
	glBegin(GL_QUADS);		
	
	glColor(color);
	
	glVertex3v16(-4096, -4096,  4096);				
	glVertex3v16( 4096, -4096,  4096);				
	glVertex3v16( 4096,  4096,  4096);					
	glVertex3v16(-4096,  4096,  4096);					

	glVertex3v16(-4096, -4096, -4096);		
	glVertex3v16(-4096,  4096, -4096);					
	glVertex3v16( 4096,  4096, -4096);					
	glVertex3v16( 4096, -4096, -4096);						

	glVertex3v16(-4096,  4096,  4096);					
	glVertex3v16( 4096,  4096,  4096);					
	glVertex3v16( 4096,  4096, -4096);					
	glVertex3v16(-4096,  4096, -4096);					

	glVertex3v16(-4096, -4096, -4096);				
	glVertex3v16( 4096, -4096, -4096);				
	glVertex3v16( 4096, -4096,  4096);			
	glVertex3v16(-4096, -4096,  4096);					

	glVertex3v16( 4096, 4096,  -4096);					
	glVertex3v16( 4096, 4096,   4096);					
	glVertex3v16( 4096,-4096,   4096);					
	glVertex3v16( 4096,-4096,  -4096);					

	glVertex3v16(-4096,-4096,  -4096);				
	glVertex3v16(-4096,-4096,   4096);			
	glVertex3v16(-4096, 4096,   4096);			
	glVertex3v16(-4096, 4096,  -4096);						

	glEnd();
}

void renderScene(int tex1, int tex2, int texW, int texH, int normalmapping) {
	glMatrixMode(GL_MODELVIEW);
	
	glPolyFmt(POLY_ID(0) | POLY_ALPHA(31) | POLY_CULL_BACK);
	//Render light 1
	glPushMatrix();
	glTranslatef32(l1Pos[0], l1Pos[1], l1Pos[2]);
	glScalef32(128, 128, 128);
	
	renderCube(min(31, l1Col[0] >> 7) | (min(31, l1Col[1] >> 7) << 5) | (min(31, l1Col[2] >> 7) << 10));
	
	glPopMatrix(1);
	
	//Render light 2
	glPushMatrix();
	glTranslatef32(l2Pos[0], l2Pos[1], l2Pos[2]);
	glScalef32(128, 128, 128);
	
	renderCube(min(31, l2Col[0] >> 7) | (min(31, l2Col[1] >> 7) << 5) | (min(31, l2Col[2] >> 7) << 10));
	
	glPopMatrix(1);
	
	if(normalmapping) {  
		//Render floor with normal mapping
		
		//From Valve's radiosity mapping
		/*int32 bases[9] = {
			5016, 0, 2364,
			-1672, 2896, 2364,
			-1672, -2896, 2364
		};*/
		//Better (calculated from texconv/main.c, x dir is 60 deg, y dir is 0, 120, 240 deg)
		int32 bases[9] = {
			3547, 0, 2048,
			-1774, 3072, 2048,
			-1774, -3072, 2048,
		};
		
		for(int basis=0; basis<3; basis++) {
			
			//Different poly ids are required for proper alpha testing/sorting I guess???
			if(basis == 0) {
				glPolyFmt(POLY_ID(0) | POLY_ALPHA(31) | POLY_CULL_BACK);
				glBindTexture(0, tex1);
				
				//Disable texture alpha (set texture format to GL_RGB256)
				gl_texture_data *tex = (gl_texture_data*) DynamicArrayGet(&glGlob->texturePtrs, tex1);
				GFX_TEX_FORMAT = (tex->texFormat & (~(0b11 << 26))) | (GL_RGB256 << 26);
			} else if(basis == 1) {
				//Set z test to equal
				glPolyFmt(POLY_ID(1) | POLY_ALPHA(31) | POLY_CULL_BACK | (1 << 14));
				glBindTexture(0, tex1);
				
				//ENABLE texture alpha (set texture format to GL_RGB32_A3)
				gl_texture_data *tex = (gl_texture_data*) DynamicArrayGet(&glGlob->texturePtrs, tex1);
				GFX_TEX_FORMAT = (tex->texFormat & (~(0b11 << 26))) | (GL_RGB32_A3 << 26);
			} else if(basis == 2) {
				//Set z test to equal
				glPolyFmt(POLY_ID(2) | POLY_ALPHA(31) | POLY_CULL_BACK | (1 << 14));
				glBindTexture(0, tex2);
			}
			
			renderFloor(texW, texH, bases[basis*3 + 0], bases[basis*3 + 1], bases[basis*3 + 2]);
		}
		
		glPolyFmt(POLY_ALPHA(31) | POLY_CULL_BACK);
		glBindTexture(0, 0);
	} else {
		glPolyFmt(POLY_ID(0) | POLY_ALPHA(31) | POLY_CULL_BACK);
		glBindTexture(0, tex1);
		
		//Disable texture alpha (set texture format to GL_RGB256)
		gl_texture_data *tex = (gl_texture_data*) DynamicArrayGet(&glGlob->texturePtrs, tex1);
		GFX_TEX_FORMAT = (tex->texFormat & (~(0b11 << 26))) | (GL_RGB256 << 26);
		
		renderFloor(texW, texH, 0, 0, 4096);
	}
	
}

int main() {
	consoleDemoInit();
	
	//Print info
	printf("Radiosity Normal Mapping on DS\nBy \e[46mgithub.com/rmn20\e[47m\n\n");
	
	printf("DPAD - Move white light source\n(Hold L to move up/down)\n");
	printf("ABXY - Move red light source\n(Hold R to move up/down)\n\n");
	printf("Select - Toggle normal mapping\n");
	
	if(!nitroFSInit(NULL)) return 1;
	
	//set mode 0, enable BG0 and set it to 3D
	videoSetMode(MODE_5_3D);
	
	vramSetBankD(VRAM_A_TEXTURE_SLOT0);
	vramSetBankD(VRAM_B_TEXTURE_SLOT1);
	vramSetBankD(VRAM_D_TEXTURE_SLOT3);
	vramSetBankE(VRAM_E_TEX_PALETTE);
	
	// initialize gl
	glInit();
	glEnable(GL_TEXTURE_2D);
	glEnable(GL_ANTIALIAS);
	
	//Enable alpha blending
	glAlphaFunc(0);
	glEnable(GL_ALPHA_TEST);
	glEnable(GL_BLEND);
 
	//Clear screen
	glClearColor(0,0,10,31); 
	glClearPolyID(63);
	glClearDepth(0x7FFF);

	//Load textures
	int tex1 = 0, tex2 = 0;
	int texW = 0, texH = 0;
	int normalmapping = 1;
	
	loadTexture("nitro://output.bin", &tex1, &tex2, &texW, &texH);
	
	//Init camera
	int camPos[3] = {0, 4096, -8192};
	s16 camRotX = -2048, camRotY = 0;
	s16 camDir[3] = {0, 0, 0};
	
	while(1) {
		scanKeys();
		u16 held = keysHeld();
		u16 press = keysDown();
		u16 release = keysUp();
		
		//Toggle normal mapping
		if(press & KEY_SELECT) normalmapping = 1 - normalmapping;
		
		//White light control
		if((held & KEY_UP) && !(held & KEY_L)) l1Pos[2] += 64;
		if((held & KEY_DOWN) && !(held & KEY_L)) l1Pos[2] -= 64;
		if((held & KEY_UP) && (held & KEY_L)) l1Pos[1] += 64;
		if((held & KEY_DOWN) && (held & KEY_L)) l1Pos[1] -= 64;
		if(held & KEY_LEFT) l1Pos[0] += 64;
		if(held & KEY_RIGHT) l1Pos[0] -= 64;
		
		//Red light control
		if((held & KEY_X) && !(held & KEY_R)) l2Pos[2] += 64;
		if((held & KEY_B) && !(held & KEY_R)) l2Pos[2] -= 64;
		if((held & KEY_X) && (held & KEY_R)) l2Pos[1] += 64;
		if((held & KEY_B) && (held & KEY_R)) l2Pos[1] -= 64;
		if(held & KEY_Y) l2Pos[0] += 64;
		if(held & KEY_A) l2Pos[0] -= 64;
		
		//Render!
		glViewport(0, 0, 255, 191);
		
		glMatrixMode(GL_PROJECTION);
		glLoadIdentity();
		gluPerspective(70, 256.0 / 192.0, 0.1, 40);
		
		glMatrixMode(GL_POSITION);
		glLoadIdentity();
		
		//Rotation to direction vector
		//camRotY += 64;
		camDir[0] = sinLerp(camRotY);
		camDir[2] = cosLerp(camRotY);
		camDir[1] = sinLerp(camRotX);
		
		s16 mul = cosLerp(camRotX);
		camDir[0] = mulf32(camDir[0], mul);
		camDir[2] = mulf32(camDir[2], mul);

		gluLookAtf32(
			camPos[0], camPos[1], camPos[2], 
			camPos[0] + camDir[0], camPos[1] + camDir[1], camPos[2] + camDir[2], 
			0, 4096, 0
		);
		
		renderScene(tex1, tex2, texW, texH, normalmapping);
		
		glFlush(GL_TRANS_MANUALSORT);
		swiWaitForVBlank();
		if(held & KEY_START) break;
	}

	return 0;
}