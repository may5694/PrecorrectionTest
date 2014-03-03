#ifndef _CAMCALIB_H
#define _CAMCALIB_H

#include <string>
#include <sstream>
#include <map>
#include <queue>
#include <SFML/Graphics.hpp>
#include "image.h"
#include "common.h"

static const std::string calibfolder = "calib/";
static const std::string captfolder = "captprec/";

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

// Image space conversion structure
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

// PSF creation parameters and error measurement
struct PSFParm {
	PSFParm(double pdpt, double pap, double psf, double perr) :
		dpt(pdpt), ap(pap), sf(psf), err(perr) {
		// Create string representation
		ss << std::setprecision(2) << std::fixed
			<< dpt << "_" << ap << "_" << sf;
		paramStr = ss.str(); ss.str("");
		i_dpt = i_ap = i_sf = 0;
	}
	double dpt;		// Diopter power of PSF
	double ap;		// Pupil diameter
	double sf;		// Scale factor
	double err;		// Error value

	int i_dpt;		// Diopter index in range
	int i_ap;		// Aperture index in range
	int i_sf;		// Scale index in range
	std::string paramStr;	// String representation of parameters (for filenames)
};
// Functor to compare the error of two PSFParm objects (used for priority queue)
class PSFCmp {
public:
	bool operator() (const PSFParm& lhs, const PSFParm& rhs) const {
		return lhs.err > rhs.err;	// Reverse order so smallest error at front
	}
};
// Holds an array of PSF parameters along with the maximum index of each param
class PSFRange : public std::vector<PSFParm> {
public:
	PSFRange(double dpt_min, double dpt_max, double dpt_inc,
			 double ap_min, double ap_max, double ap_inc,
			 double sf_min, double sf_max, double sf_inc);

	int i_dpt_max;
	int i_ap_max;
	int i_sf_max;
};
typedef std::priority_queue<PSFParm, std::vector<PSFParm>, PSFCmp> PSFpqueue;

// Test a range of PSFs and return a priority queue of the lowest error values
PSFpqueue searchPSFs(const Image& in_disp, const PSFRange& psfs, bool force = false, bool outErrMaps = true);

#endif