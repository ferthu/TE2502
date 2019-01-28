// Mountains. By David Hoskins - 2013
// License Creative Commons Attribution-NonCommercial-ShareAlike 3.0 Unported License.

#version 450 core

#define WIDTH 1080
#define HEIGHT 720
#define WORKGROUP_SIZE 32
layout(local_size_x = WORKGROUP_SIZE, local_size_y = WORKGROUP_SIZE, local_size_z = 1) in;
//layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;

layout(set = 0, binding = 0, rgba8) uniform image2D image;

vec2 iResolution = vec2(1080, 720);

float treeCol = 0.0;

vec3 sunLight = normalize(vec3(0.4, 0.4, 0.48));
vec3 sunColour = vec3(1.0, .9, .83);
float specular = 0.0;
vec3 cameraPos;
float ambient;
vec2 add = vec2(1.0, 0.0);
#define HASHSCALE1 .1031
#define HASHSCALE3 vec3(.1031, .1030, .0973)
#define HASHSCALE4 vec4(1031, .1030, .0973, .1099)

// This peturbs the fractal positions for each iteration down...
// Helps make nice twisted landscapes...
const mat2 rotate2D = mat2(1.3623, 1.7531, -1.7131, 1.4623);


//  1 out, 2 in...
float Hash12(vec2 p)
{
	vec3 p3 = fract(vec3(p.xyx) * HASHSCALE1);
	p3 += dot(p3, p3.yzx + 19.19);
	return fract((p3.x + p3.y) * p3.z);
}
vec2 Hash22(vec2 p)
{
	vec3 p3 = fract(vec3(p.xyx) * HASHSCALE3);
	p3 += dot(p3, p3.yzx + 19.19);
	return fract((p3.xx + p3.yz)*p3.zy);

}

float Noise(in vec2 x)
{
	vec2 p = floor(x);
	vec2 f = fract(x);
	f = f * f*(3.0 - 2.0*f);

	float res = mix(mix(Hash12(p), Hash12(p + add.xy), f.x),
		mix(Hash12(p + add.yx), Hash12(p + add.xx), f.x), f.y);
	return res;
}

vec2 Noise2(in vec2 x)
{
	vec2 p = floor(x);
	vec2 f = fract(x);
	f = f * f*(3.0 - 2.0*f);
	float n = p.x + p.y * 57.0;
	vec2 res = mix(mix(Hash22(p), Hash22(p + add.xy), f.x),
		mix(Hash22(p + add.yx), Hash22(p + add.xx), f.x), f.y);
	return res;
}


//--------------------------------------------------------------------------
// Low def version for ray-marching through the height field...
// Thanks to IQ for all the noise stuff...

float Terrain(in vec2 p)
{
	vec2 pos = p * 0.05;
	float w = (Noise(pos*.25)*0.75 + .15);
	w = 66.0 * w * w;
	vec2 dxy = vec2(0.0, 0.0);
	float f = .0;
	for (int i = 0; i < 5; i++)
	{
		f += w * Noise(pos);
		w = -w * 0.4;	//...Flip negative and positive for variation
		pos = rotate2D * pos;
	}
	float ff = Noise(pos*.002);

	f += pow(abs(ff), 5.0)*275. - 5.0;
	return f;
}

//--------------------------------------------------------------------------
// Map to lower resolution for height field mapping for Scene function...
float Map(in vec3 p)
{
	float h = Terrain(p.xz);

	float ff = Noise(p.xz*.3) + Noise(p.xz*3.3)*.5;
	treeCol = 0.f;
	h += treeCol;

	return p.y - h;
}

//--------------------------------------------------------------------------
// High def version only used for grabbing normal information.
float Terrain2(in vec2 p)
{
	// There's some real magic numbers in here! 
	// The Noise calls add large mountain ranges for more variation over distances...
	vec2 pos = p * 0.05;
	float w = (Noise(pos*.25)*0.75 + .15);
	w = 66.0 * w * w;
	vec2 dxy = vec2(0.0, 0.0);
	float f = .0;
	for (int i = 0; i < 5; i++)
	{
		f += w * Noise(pos);
		w = -w * 0.4;	//...Flip negative and positive for varition	   
		pos = rotate2D * pos;
	}
	float ff = Noise(pos*.002);
	f += pow(abs(ff), 5.0)*275. - 5.0;


	treeCol = 0.f;
	f += treeCol;
	if (treeCol > 0.0) return f;


	// That's the last of the low resolution, now go down further for the Normal data...
	for (int i = 0; i < 6; i++)
	{
		f += w * Noise(pos);
		w = -w * 0.4;
		pos = rotate2D * pos;
	}


	return f;
}

//--------------------------------------------------------------------------
float FractalNoise(in vec2 xy)
{
	float w = .7;
	float f = 0.0;

	for (int i = 0; i < 4; i++)
	{
		f += Noise(xy) * w;
		w *= 0.5;
		xy *= 2.7;
	}
	return f;
}

//--------------------------------------------------------------------------
// Calculate sun light...
void DoLighting(inout vec3 mat, in vec3 pos, in vec3 normal, in vec3 eyeDir, in float dis)
{
	float h = dot(sunLight, normal);
	float c = max(h, 0.0) + ambient;
	mat = mat * sunColour * c;
	// Specular...
	if (h > 0.0)
	{
		vec3 R = reflect(sunLight, normal);
		float specAmount = pow(max(dot(R, normalize(eyeDir)), 0.0), 3.0)*specular;
		mat = mix(mat, sunColour, specAmount);
	}
}

//--------------------------------------------------------------------------
// Hack the height, position, and normal data to create the coloured landscape
vec3 TerrainColour(vec3 pos, vec3 normal, float dis)
{
	vec3 mat;
	specular = .0;
	ambient = .1;
	vec3 dir = normalize(pos - cameraPos);

	vec3 matPos = pos * 2.0;// ... I had change scale halfway though, this lazy multiply allow me to keep the graphic scales I had

	float disSqrd = dis * dis;// Squaring it gives better distance scales.

	float f = clamp(Noise(matPos.xz*.05), 0.0, 1.0);//*10.8;
	f += Noise(matPos.xz*.1 + normal.yz*1.08)*.85;
	f *= .55;
	vec3 m = mix(vec3(.63*f + .2, .7*f + .1, .7*f + .1), vec3(f*.43 + .1, f*.3 + .2, f*.35 + .1), f*.65);
	mat = m * vec3(f*m.x + .36, f*m.y + .30, f*m.z + .28);
	// Should have used smoothstep to add colours, but left it using 'if' for sanity...
	if (normal.y < .5)
	{
		float v = normal.y;
		float c = (.5 - normal.y) * 4.0;
		c = clamp(c*c, 0.1, 1.0);
		f = Noise(vec2(matPos.x*.09, matPos.z*.095 + matPos.yy*0.15));
		f += Noise(vec2(matPos.x*2.233, matPos.z*2.23))*0.5;
		mat = mix(mat, vec3(.4*f), c);
		specular += .1;
	}

	// Grass. Use the normal to decide when to plonk grass down...
	if (/*matPos.y < 45.35 &&*/ normal.y > .65)
	{

		m = vec3(Noise(matPos.xz*.023)*.5 + .15, Noise(matPos.xz*.03)*.6 + .25, 0.0);
		m *= (normal.y - 0.65)*.6;
		mat = mix(mat, m, clamp((normal.y - .65)*1.3 * (45.35 - matPos.y)*0.1, 0.0, 1.0));
	}

	if (treeCol > 0.0)
	{
		mat = vec3(.02 + Noise(matPos.xz*5.0)*.03, .05, .0);
		normal = normalize(normal + vec3(Noise(matPos.xz*33.0)*1.0 - .5, .0, Noise(matPos.xz*33.0)*1.0 - .5));
		specular = .0;
	}

	// Snow topped mountains...
	if (matPos.y > 80.0 && normal.y > .42)
	{
		float snow = clamp((matPos.y - 80.0 - Noise(matPos.xz * .1)*28.0) * 0.035, 0.0, 1.0);
		mat = mix(mat, vec3(.7, .7, .8), snow);
		specular += snow;
		ambient += snow * .3;
	}
	// Beach effect...
	/*if (matPos.y < 1.45)
	{
		if (normal.y > .4)
		{
			f = Noise(matPos.xz * .084)*1.5;
			f = clamp((1.45-f-matPos.y) * 1.34, 0.0, .67);
			float t = (normal.y-.4);
			t = (t*t);
			mat = mix(mat, vec3(.09+t, .07+t, .03+t), f);
		}
		// Cheap under water darkening...it's wet after all...
		if (matPos.y < 0.0)
		{
			mat *= .2;
		}
	}*/

	DoLighting(mat, pos, normal, dir, disSqrd);

	// Do the water...
	/*if (matPos.y < 0.0)
	{
		// Pull back along the ray direction to get water surface point at y = 0.0 ...
		float time = (iTime)*.03;
		vec3 watPos = matPos;
		watPos += -dir * (watPos.y/dir.y);
		// Make some dodgy waves...
		float tx = cos(watPos.x*.052) *4.5;
		float tz = sin(watPos.z*.072) *4.5;
		vec2 co = Noise2(vec2(watPos.x*4.7+1.3+tz, watPos.z*4.69+time*35.0-tx));
		co += Noise2(vec2(watPos.z*8.6+time*13.0-tx, watPos.x*8.712+tz))*.4;
		vec3 nor = normalize(vec3(co.x, 20.0, co.y));
		nor = normalize(reflect(dir, nor));//normalize((-2.0*(dot(dir, nor))*nor)+dir);
		// Mix it in at depth transparancy to give beach cues..
				tx = watPos.y-matPos.y;
		// Add some extra water glint...
				mat += vec3(.1)*clamp(1.-pow(tx+.5, 3.)*texture(iChannel1, watPos.xz*.1, -2.).x, 0.,1.0);
		float sunAmount = max( dot(nor, sunLight), 0.0 );
		mat = mat + sunColour * pow(sunAmount, 228.5)*.6;
				vec3 temp = (watPos-cameraPos*2.)*.5;
				disSqrd = dot(temp, temp);
	}*/
	return mat;
}

//--------------------------------------------------------------------------
float BinarySubdivision(in vec3 rO, in vec3 rD, vec2 t)
{
	// Home in on the surface by dividing by two and split...
	float halfwayT;

	for (int i = 0; i < 5; i++)
	{

		halfwayT = dot(t, vec2(.5));
		float d = Map(rO + halfwayT * rD);
		t = mix(vec2(t.x, halfwayT), vec2(halfwayT, t.y), step(0.5, d));

	}
	return halfwayT;
}

//--------------------------------------------------------------------------
bool Scene(in vec3 rO, in vec3 rD, out float resT, in vec2 fragCoord)
{
	float t = 1. + Hash12(fragCoord.xy)*5.;
	float oldT = 0.0;
	float delta = 0.0;
	bool fin = false;
	bool res = false;
	vec2 distances;
	for (int j = 0; j < 150; j++)
	{
		if (fin || t > 240.0) break;
		vec3 p = rO + t * rD;
		//if (t > 240.0 || p.y > 195.0) break;
		float h = Map(p); // ...Get this positions height mapping.
		// Are we inside, and close enough to fudge a hit?...
		if (h < 0.5)
		{
			fin = true;
			distances = vec2(oldT, t);
			break;
		}
		// Delta ray advance - a fudge between the height returned
		// and the distance already travelled.
		// It's a really fiddly compromise between speed and accuracy
		// Too large a step and the tops of ridges get missed.
		delta = max(0.01, 0.3*h) + (t*0.0045);
		oldT = t;
		t += delta;
	}
	if (fin) resT = BinarySubdivision(rO, rD, distances);

	return fin;
}

//--------------------------------------------------------------------------
vec3 CameraPath(float t)
{
	float m = 140.0;
	float iTime = 34.0;
	t = (iTime*1.5 + m + 657.0)*.006 + t;
	vec2 p = 476.0*vec2(sin(3.5*t), cos(1.5*t));
	return vec3(35.0 - p.x, 0.6, 4108.0 + p.y);
}

//--------------------------------------------------------------------------
// Some would say, most of the magic is done in post! :D
vec3 PostEffects(vec3 rgb, vec2 uv)
{
	//#define CONTRAST 1.1
	//#define SATURATION 1.12
	//#define BRIGHTNESS 1.3
	//rgb = pow(abs(rgb), vec3(0.45));
	//rgb = mix(vec3(.5), mix(vec3(dot(vec3(.2125, .7154, .0721), rgb*BRIGHTNESS)), rgb*BRIGHTNESS, SATURATION), CONTRAST);
	rgb = (1.0 - exp(-rgb * 6.0)) * 1.0024;
	//rgb = clamp(rgb+hash12(fragCoord.xy*rgb.r)*0.1, 0.0, 1.0);
	return rgb;
}

//--------------------------------------------------------------------------
void main(void)
{
	if (gl_GlobalInvocationID.x >= WIDTH || gl_GlobalInvocationID.y >= HEIGHT)
		return;

	vec2 xy = 1.0 + -2.0*gl_GlobalInvocationID.xy / iResolution.xy;
	vec2 uv = xy * vec2(iResolution.x / iResolution.y, 1.0);
	vec3 camTar;

	// Use several forward heights, of decreasing influence with distance from the camera.
	float h = 0.0;
	float f = 1.0;
	for (int i = 0; i < 7; i++)
	{
		h += Terrain(CameraPath((.6 - f)*.008).xz) * f;
		f -= .1;
	}
	cameraPos.xz = CameraPath(0.0).xz;
	camTar.xyz = CameraPath(.005).xyz;
	float iTime = 34.f;
	camTar.y = cameraPos.y = max((h*.25) + 3.5, 1.5 + sin(iTime*5.)*.5);

	float roll = 0.15*sin(iTime*.2);
	vec3 cw = normalize(camTar - cameraPos);
	vec3 cp = vec3(sin(roll), cos(roll), 0.0);
	vec3 cu = normalize(cross(cw, cp));
	vec3 cv = normalize(cross(cu, cw));
	vec3 rd = normalize(uv.x*cu + uv.y*cv + 1.5*cw);

	vec3 col;
	float distance;
	if (!Scene(cameraPos, rd, distance, gl_GlobalInvocationID.xy))
	{
		// Missed scene, now just get the sky value...
	}
	else
	{
		// Get world coordinate of landscape...
		vec3 pos = cameraPos + distance * rd;
		// Get normal from sampling the high definition height map
		// Use the distance to sample larger gaps to help stop aliasing...
		float p = min(.3, .0005 + .00005 * distance*distance);
		vec3 nor = vec3(0.0, Terrain2(pos.xz), 0.0);
		vec3 v2 = nor - vec3(p, Terrain2(pos.xz + vec2(p, 0.0)), 0.0);
		vec3 v3 = nor - vec3(0.0, Terrain2(pos.xz + vec2(0.0, -p)), -p);
		nor = cross(v2, v3);
		nor = normalize(nor);

		// Get the colour using all available data...
		col = TerrainColour(pos, nor, distance);
	}

	col = PostEffects(col, uv);

	vec4 fragColor = vec4(col, 1.0);
	imageStore(image, ivec2(gl_GlobalInvocationID.xy), fragColor);
}

