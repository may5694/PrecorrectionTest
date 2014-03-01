#ifndef _CAMCALIB_H
#define _CAMCALIB_H

#include <string>
#include <sstream>
#include <map>
#include <SFML/Graphics.hpp>
#include "image.h"
#include "common.h"

static const std::string calibfolder = "calib/";
static const std::string psffolder = "psf/";

// Enumerate the various image spaces
enum ImgSpace {
	Camera,		// Intensity as seen by the camera
	Physical,	// Physical illumination produced by display
	Display,	// Intensity sent to the display
};

// Camera alignment structure
struct CamAlign {
	sf::Transform t;	// Image transform matrix
	int sw, sh;			// Display resolution

	void create(int sw, int sh);				// Create the data needed to calculate alignment
	bool read(const std::string& filename);		// Load a saved alignment
	bool write(const std::string& filename);	// Save an existing alignment

	Image alignImage(const Image& in, int iw, int ih);		// Align an image taken by the camera
};
extern CamAlign camAlign;

// Image space conversion structuer
struct ImgConvert {
	std::map<double, double> camToPhys;		// Map camera intensity to physical radiance
	std::map<double, double> physToCam;		// Map physical radiance to camera intensity
	std::map<double, double> dispToPhys;	// Map display intensity to physical radiance
	std::map<double, double> physToDisp;	// Map physical radiance to display intensity

	void create();								// Create the conversion via calibration
	bool read(const std::string& filename);		// Load saved conversions
	bool write(const std::string& filename);	// Save existing conversions

	Image convertImage(const Image& in, ImgSpace from, ImgSpace to);				// Convert an image from one space to another
	Image convertImage(const Image& in, const std::map<double, double>& usemap);	// Convert an image using a specified map
	double convertValue(double in, ImgSpace from, ImgSpace to);						// Convert a value from one space to another
	double convertValue(double in, const std::map<double, double>& usemap);			// Convert a value using a specified map
};
extern ImgConvert imgConvert;

#endif