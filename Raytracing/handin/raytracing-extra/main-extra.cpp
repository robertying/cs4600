#include <cstdlib>
#include <cstdio>
#include <cmath>
#include <fstream>
#include <vector>
#include <iostream>
#include <cassert>
#include <random>
#include <algorithm>
#include <Eigen>

using namespace Eigen;

const int MAX_DEPTH = 5;

// image background color
Vector3f bgcolor(1.0f, 1.0f, 1.0f);

// lights in the scene
std::vector<std::vector<Vector3f>> lightPositions = {
	{
		Vector3f(0.0, 60, 60),
		Vector3f(1.0, 60, 60),
		Vector3f(-1.0, 60, 60),
		Vector3f(0.0, 59, 60),
		Vector3f(0.0, 61, 60),
		Vector3f(0.0, 60, 59),
		Vector3f(0.0, 60, 61)
	},
	{
		Vector3f(-60.0, 60, 60),
		Vector3f(-59.0, 60, 60),
		Vector3f(-61.0, 60, 60),
		Vector3f(-60.0, 59, 60),
		Vector3f(-60.0, 61, 60),
		Vector3f(-60.0, 60, 59),
		Vector3f(-60.0, 60, 61)
	},
	{
		Vector3f(60.0, 60, 60),
		Vector3f(59.0, 60, 60),
		Vector3f(61.0, 60, 60),
		Vector3f(60.0, 59, 60),
		Vector3f(60.0, 61, 60),
		Vector3f(60.0, 60, 59),
		Vector3f(60.0, 60, 61)
	}
};

class Sphere
{
public:
	Vector3f center;  // position of the sphere
	float radius;  // sphere radius
	Vector3f surfaceColor; // surface color
	bool specular;

	Sphere(
		const Vector3f &c,
		const float &r,
		const Vector3f &sc,
		bool specular) :
		center(c), radius(r), surfaceColor(sc), specular(specular)
	{
	}

	// line vs. sphere intersection (note: this is slightly different from ray vs. sphere intersection!)
	bool intersect(const Vector3f &rayOrigin, const Vector3f &rayDirection, float &t0, float &t1) const
	{
		Vector3f l = center - rayOrigin;
		float tca = l.dot(rayDirection);
		if (tca < 0) return false;
		float d2 = l.dot(l) - tca * tca;
		if (d2 > (radius * radius)) return false;
		float thc = sqrt(radius * radius - d2);
		t0 = tca - thc;
		t1 = tca + thc;

		return true;
	}
};

// diffuse reflection model
Vector3f diffuse(const Vector3f &L, // direction vector from the point on the surface towards a light source
	const Vector3f &N, // normal at this point on the surface
	const Vector3f &diffuseColor,
	const float kd // diffuse reflection constant
)
{
	Vector3f resColor = Vector3f::Zero();

	resColor = kd * std::max(L.dot(N), 0.f) * diffuseColor;

	return resColor;
}

// Phong reflection model
Vector3f phong(const Vector3f &L, // direction vector from the point on the surface towards a light source
	const Vector3f &N, // normal at this point on the surface
	const Vector3f &V, // direction pointing towards the viewer
	const Vector3f &diffuseColor,
	const Vector3f &specularColor,
	const float kd, // diffuse reflection constant
	const float ks, // specular reflection constant
	const float alpha) // shininess constant
{
	Vector3f resColor = Vector3f::Zero();

	Vector3f R = 2 * N*(N.dot(L)) - L;
	R.normalize();
	resColor = diffuse(L, N, diffuseColor, kd) + ks * std::powf(std::max(R.dot(V), 0.f), alpha) * specularColor;

	return resColor;
}

Vector3f trace(
	const Vector3f &rayOrigin,
	const Vector3f &rayDirection,
	const std::vector<Sphere> &spheres,
	int depth)
{
	Vector3f pixelColor = Vector3f::Zero();
	float error = -0.1f;

	bool hitSphere = false;
	float minDistance = INFINITY;
	Vector3f hitPoint = Vector3f::Zero();
	int sphereIndex = 0;

	for (int i = 0; i < spheres.size(); ++i) {
		float t0, t1;
		bool intersect = spheres[i].intersect(rayOrigin, rayDirection, t0, t1);

		if (t0 > error  && intersect)
		{
			hitSphere = true;

			if (t0 < minDistance) {
				minDistance = t0;
				sphereIndex = i;
				hitPoint = rayOrigin + t0 * rayDirection;
			}
		}
	}

	if (!hitSphere) {
		return bgcolor;
	}

	for (int j = 0; j < 3; ++j) {
		Vector3f rayOrigin2 = hitPoint;
		for (int m = 0; m < lightPositions[j].size(); m++) {
			Vector3f rayDirection2 = lightPositions[j][m] - hitPoint;
			rayDirection2.normalize();

			bool blocked = false;
			for (int k = 0; k < spheres.size(); ++k) {
				float t00, t11;
				bool intersect2 = spheres[k].intersect(rayOrigin2, rayDirection2, t00, t11);
				if (t00 > error && intersect2) {
					blocked = true;
					break;
				}
			}

			if (!blocked) {
				Vector3f N = hitPoint - spheres[sphereIndex].center;
				N.normalize();
				Vector3f L = lightPositions[j][m] - hitPoint;
				L.normalize();
				Vector3f V = -rayDirection;
				pixelColor += phong(L, N, V, spheres[sphereIndex].surfaceColor, Vector3f::Ones(), 1.f, 3.f, 100.f) / (3 * lightPositions[j].size());
			}
		}
	}

	if (++depth <= MAX_DEPTH) {
		if (spheres[sphereIndex].specular) {
			Vector3f N = hitPoint - spheres[sphereIndex].center;
			N.normalize();
			Vector3f L = -rayDirection;
			L.normalize();
			Vector3f R = 2 * N*(N.dot(L)) - L;
			R.normalize();
			pixelColor = 0.95* pixelColor + 0.05 * trace(hitPoint, R, spheres, depth);
		}
	}

	return pixelColor;
}

void render(const std::vector<Sphere> &spheres)
{
	unsigned width = 640;
	unsigned height = 480;
	Vector3f *image = new Vector3f[width * height];
	Vector3f *pixel = image;
	float invWidth = 1 / float(width);
	float invHeight = 1 / float(height);
	float fov = 30;
	float aspectratio = width / float(height);
	float angle = tan(M_PI * 0.5f * fov / 180.f);

	// Trace rays
	for (unsigned y = 0; y < height; ++y)
	{
		for (unsigned x = 0; x < width; ++x)
		{
			float rayX = (2 * ((x + 0.5f) * invWidth) - 1) * angle * aspectratio;
			float rayY = (1 - 2 * ((y + 0.5f) * invHeight)) * angle;
			Vector3f rayDirection(rayX, rayY, -1);
			rayDirection.normalize();
			*(pixel++) = trace(Vector3f::Zero(), rayDirection, spheres, 0);
		}
	}

	// Save result to a PPM image
	std::ofstream ofs("./render.ppm", std::ios::out | std::ios::binary);
	ofs << "P6\n" << width << " " << height << "\n255\n";
	for (unsigned i = 0; i < width * height; ++i)
	{
		const float x = image[i](0);
		const float y = image[i](1);
		const float z = image[i](2);

		ofs << (unsigned char)(std::min(float(1), x) * 255)
			<< (unsigned char)(std::min(float(1), y) * 255)
			<< (unsigned char)(std::min(float(1), z) * 255);
	}

	ofs.close();
	delete[] image;
}

int main(int argc, char **argv)
{
	std::vector<Sphere> spheres;
	// position, radius, surface color
	spheres.push_back(Sphere(Vector3f(0.0, -10004, -20), 10000, Vector3f(0.50, 0.50, 0.50), true));
	spheres.push_back(Sphere(Vector3f(0.0, 0, -20), 4, Vector3f(1.00, 0.32, 0.36), true));
	spheres.push_back(Sphere(Vector3f(5.0, -1, -15), 2, Vector3f(0.90, 0.76, 0.46), true));
	spheres.push_back(Sphere(Vector3f(5.0, 0, -25), 3, Vector3f(0.65, 0.77, 0.97), true));
	spheres.push_back(Sphere(Vector3f(-5.5, 0, -13), 3, Vector3f(0.90, 0.90, 0.90), true));
	spheres.push_back(Sphere(Vector3f(3.5, 3, -13), 1, Vector3f(1.00, 1.00, 0.00), true));
	spheres.push_back(Sphere(Vector3f(-1.5, -1.5, -10), 0.5, Vector3f(0.00, 0.50, 1.00), false));

	render(spheres);

	return 0;
}
