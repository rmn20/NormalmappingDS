#include <stdlib.h>
#include <stdio.h>
#include <math.h>

#include "lodepng.h"

/*
Loads a texture and a normal map with the same resolution.
Normalmap y direction should be in openGL notation.
Resolution should be supported by Nintendo DS.
Texture can have <= 32 colors.

Outputs 2 textures with alpha calculated from dp.y and dp.z based on Valve's radiosity normal mapping.
See more here: https://cdn.cloudflare.steamstatic.com/apps/valve/2007/SIGGRAPH2007_EfficientSelfShadowedRadiosityNormalMapping.pdf
*/

typedef struct {
	unsigned char r, g, b, a;
} Color;

float clamp(float x, float min, float max) {
	if(x < min) return min;
	if(x > max) return max;
	return x;
}

float vecDot(float* vec, float* vec2) {
	return vec[0] * vec2[0] + vec[1] * vec2[1] + vec[2] * vec2[2];
}

void vecNormalize(float* vec) {
	float len = sqrt(vecDot(vec, vec));
	vec[0] /= len;
	vec[1] /= len;
	vec[2] /= len;
}

void colorToVec(Color* col, float* vec) {
	vec[0] = (col->r / 255.0f) * 2 - 1;
	vec[1] = (col->g / 255.0f) * 2 - 1;
	vec[2] = (col->b / 255.0f) * 2 - 1;
	
	vecNormalize(vec);
}

//Nintendo DS supported resolutions
int getSize(int size) {
	if(size == 1024) return 7;
	else if(size == 512) return 6;
	else if(size == 256) return 5;
	else if(size == 128) return 4;
	else if(size == 64) return 3;
	else if(size == 32) return 2;
	else if(size == 16) return 1;
	else if(size == 8) return 0;
	else return -1;
}

int main(int argc, char **argv) {
	if(argc != 3) {
		printf("Please provide a texture and normal map\n");
		return 1;
	}
	
	unsigned char* image = NULL;
	unsigned char* normalMap = NULL;
	
	//Load texture
	int w, h;
	{
		int error;
		error = lodepng_decode32_file(&image, &w, &h, argv[1]);
		if(error) {
			printf("Can't load texture %u: %s\n", error, lodepng_error_text(error));
			return 1;
		}
	}
	
	//Load normalmap
	{
		int w2, h2;
		int error = lodepng_decode32_file(&normalMap, &w2, &h2, argv[2]);
		if(error) {
			printf("Can't load normal map %u: %s\n", error, lodepng_error_text(error));
			free(image);
			return 1;
		}
		
		if(w2 != w || h2 != h) {
			printf("Textures resolution should be the same!\n");
			free(image);
			free(normalMap);
			return 1;
		}
	}

	//Check texture resolution
	if(getSize(w) == -1 || getSize(h) == -1) {
		printf("Unsupported resolution!\n");
		free(image);
		free(normalMap);
		return 1;
	}
	
	//Palette for texture
	Color palette[32] = {0};
	int colorsCount = 0;
	
	for(int i=0; i<w*h; i++) {
		Color col = *((Color*) (image + i*4));
		
		int idx = 0;
		for(; idx<colorsCount; idx++) {
			Color palCol = palette[idx];
			if(col.r == palCol.r && col.g == palCol.g && col.b == palCol.b) break;
		}
		
		if(idx == colorsCount) {
			if(colorsCount == 32) {
				printf("Only 32 color images supported!\n");
				free(image);
				free(normalMap);
				return 1;
			}
			
			palette[colorsCount] = col;
			idx = colorsCount;
			colorsCount++;
		}
		
		image[i] = idx;
	}
	
	//Calculate texture data
	unsigned char* outIdx = malloc(sizeof(char) * w * h * 2);
	if(outIdx == NULL) {
		printf("Can't allocate memory for output image!\n");
		free(image);
		free(normalMap);
		return 1;
	}
	
	//From Valve's radiosity normal mapping
	/*float basis1[3] = {sqrt(3.0/2.0), 0.0, 1 / sqrt(3.0)};
	float basis2[3] = {-1 / sqrt(6.0), 1 / sqrt(2.0), 1 / sqrt(3.0)};
	float basis3[3] = {-1 / sqrt(6.0), -1 / sqrt(2.0), 1 / sqrt(3.0)};
	
	//Just testing
	{
		float len = sqrt(basis1[0]*basis1[0] + basis1[1]*basis1[1] + basis1[2]*basis1[2]);
		for(int xyz=0; xyz<3; xyz++) basis1[xyz] /= len;
	}
	{
		float len = sqrt(basis2[0]*basis2[0] + basis2[1]*basis2[1] + basis2[2]*basis2[2]);
		for(int xyz=0; xyz<3; xyz++) basis2[xyz] /= len;
	}
	{
		float len = sqrt(basis3[0]*basis3[0] + basis3[1]*basis3[1] + basis3[2]*basis3[2]);
		for(int xyz=0; xyz<3; xyz++) basis3[xyz] /= len;
	}
	
	printf("%f %f %f\n", basis1[0], basis1[1], basis1[2]);
	printf("%f %f %f\n", basis2[0], basis2[1], basis2[2]);
	printf("%f %f %f\n", basis3[0], basis3[1], basis3[2]);*/
	
	
	//Vectors provided by valve are weird.... These vectors are similar to the vectors provided by valve, but they offer uniform coverage of the vector space
	float xDir = 60;
	float xSin = sin(xDir * M_PI / 180);
	float xCos = cos(xDir * M_PI / 180);

	float basis1[3] = {cos(0) * xSin, sin(0) * xSin, xCos};
	float basis2[3] = {cos(120 * M_PI / 180) * xSin, sin(120 * M_PI / 180) * xSin, xCos};
	float basis3[3] = {cos(240 * M_PI / 180) * xSin, sin(240 * M_PI / 180) * xSin, xCos};
	
	/*printf("%f %f %f\n", roundf(4096 * basis1[0]), roundf(4096 * basis1[1]), roundf(4096 * basis1[2]));
	printf("%f %f %f\n", roundf(4096 * basis2[0]), roundf(4096 * basis2[1]), roundf(4096 * basis2[2]));
	printf("%f %f %f\n", roundf(4096 * basis3[0]), roundf(4096 * basis3[1]), roundf(4096 * basis3[2]));*/
	
	for(int x=0; x<w; x++) {
		for(int y=0; y<h; y++) {
			int pxPos = x + y*w;
			
			//Read normal (in openGL notation)
			float norm[3] = {0};
			colorToVec((Color*) (normalMap + pxPos*4), norm);
			norm[1] *= -1;
			
			//Based on Valve code
			float dp[3] = {
				vecDot(basis1, norm), 
				vecDot(basis2, norm), 
				vecDot(basis3, norm)
			};
			
			for(int axis=0; axis<3; axis++) {
				dp[axis] = clamp(dp[axis], 0, 1);
				dp[axis] = powf(dp[axis], 2); //gamma correction??? or just a hack to make normals look more sharp?
			}
			
			float len = dp[0] + dp[1] + dp[2];
			dp[0] /= len;
			dp[1] /= len;
			dp[2] /= len;
			
			//Additive blending can be replaced with alpha blending, since summ of dp's are always equal to 1
			//Calculate dp2 and dp3 alpha factors
			float alphadp2 = 0;
			if(dp[1] > 0) alphadp2 = dp[1] / (dp[0] + dp[1]);
			
			float alphadp3 = 0;
			if(dp[2] > 0) alphadp3 = dp[2] / (dp[0] + dp[1] + dp[2]);
			
			//Alpha in 3 bit format
			int adp2_3b = (int) roundf(alphadp2 * 7);
			int adp3_3b = (int) roundf(alphadp3 * 7);
			
			int colIdx = image[pxPos];
			outIdx[pxPos] = (adp2_3b << 5) | colIdx;
			outIdx[pxPos + w*h] = (adp3_3b << 5) | colIdx;
		}
	}
	
	//Write file
	FILE* outf = fopen("output.bin", "wb");
	fputc(getSize(w), outf);
	fputc(getSize(h), outf);
	
	//Write indexes
	fwrite(outIdx, sizeof(char), w*h*2, outf);
	free(outIdx);
	
	//Write palette
	//Write it 8 times so texture can be rendered both as GL_RGB32_A3 and GL_RGB256
	for(int i=0; i<8; i++) {
		for(int t=0; t<32; t++) {
			Color col = palette[t];
			
			//NDS 15 bit color format
			int r = roundf(col.r * 31 / 255.0f);
			int g = roundf(col.g * 31 / 255.0f);
			int b = roundf(col.b * 31 / 255.0f);
			int col_16b = (1 << 15) | (b << 10) | (g << 5) | (r);
			
			//little endian
			fputc(col_16b, outf);
			fputc(col_16b >> 8, outf);
		}
	}
	
	fclose(outf);
	
	free(image);
	free(normalMap);
	return 0;
}