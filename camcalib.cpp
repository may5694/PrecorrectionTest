#include <iomanip>
#include "camcalib.h"
#include "canoninterface.h"

CamAlign camAlign;
ImgConvert imgConvert;

void CamAlign::create(int sw, int sh) {
	// Alignment image filenames
	std::stringstream ss;
	ss << topfolder << calibfolder << "align_16.png";
	std::string alignImgName = ss.str(); ss.str("");
	ss << topfolder << calibfolder << "align_16_capt.png";
	std::string alignImgCaptName = ss.str(); ss.str("");

	// Create the alignment image
	Image X(1, sh, 1.0);
	X = X.resize((sw / 4) - 1, sh, 0.0)
		.resize((sw / 4), sh, 1.0)
		.resize((3 * sw / 4) - 1, sh, 0.0)
		.resize((3 * sw / 4), sh, 1.0)
		.resize(sw - 1, sh, 0.0)
		.resize(sw, sh, 1.0);
	Image Y(sw, 1, 1.0);
	Y = Y.resize(sw, (sh / 4) - 1, 0.0)
		.resize(sw, (sh / 4), 1.0)
		.resize(sw, (3 * sh / 4) - 1, 0.0)
		.resize(sw, (3 * sh / 4), 1.0)
		.resize(sw, sh - 1, 0.0)
		.resize(sw, sh, 1.0);
	Image F = (X + Y).clip();
	writeImage(F, alignImgName);

	// Display the alignment image and capture it
	connectToFirstCamera();
	showImage(F, true);
	sf::sleep(sf::seconds(0.1f));
	takeAPicture(alignImgCaptName.c_str());
	disconFromFirstCamera();
}
bool CamAlign::read(const std::string& filename) {
	// Open the alignment file
	FILE* fp = fopen(filename.c_str(), "rb");
	if (!fp) return false;
	// Read in the screen dimensions followed by the transform matrix
	double xformMtx [9];
	fread(&sw, sizeof(int), 1, fp);
	fread(&sh, sizeof(int), 1, fp);
	fread(xformMtx, sizeof(double), 9, fp);
	fclose(fp); fp = NULL;
	
	// Create the transformation matrix object
	t = sf::Transform(xformMtx [0], xformMtx [1], xformMtx [2],
					  xformMtx [3], xformMtx [4], xformMtx [5],
					  xformMtx [6], xformMtx [7], xformMtx [8]);
	return true;
}
bool CamAlign::write(const std::string& filename) {
	// Open the alignment file
	FILE* fp = fopen(filename.c_str(), "wb");
	if (!fp) return false;
	// Write the screen dimensions followed by the transform matrix
	const float* xform4x4 = t.getMatrix();
	double xformMtx [9] = { (double) xform4x4 [0], (double) xform4x4 [4], (double) xform4x4 [12],
							(double) xform4x4 [1], (double) xform4x4 [5], (double) xform4x4 [13],
							(double) xform4x4 [3], (double) xform4x4 [7], (double) xform4x4 [15] };
	fwrite(&sw, sizeof(int), 1, fp);
	fwrite(&sh, sizeof(int), 1, fp);
	fwrite(xformMtx, sizeof(double), 9, fp);
	fclose(fp); fp = NULL;

	return true;
}
Image CamAlign::alignImage(const Image& in, int iw, int ih) {
	// Create a second transform for the image size
	sf::Transform t1 = sf::Transform::Identity;
	t1.translate(-(sw / iw) / 2.0, -(sh - ih) / 2.0);
	// Combine the two transforms
	sf::Transform tf = t1 * t;

	// Copy input image to a GPU texture
	std::vector<sf::Uint8> buf; buf.resize(in.getArraySize() * 4);
	in.toUchar(4, 0x7, buf.data(), true);
	sf::Texture tex; tex.create(in.getWidth(), in.getHeight());
	tex.update(buf.data()); buf.clear();
	sf::Sprite s; s.setTexture(tex);

	// Render the transformed input image
	sf::RenderTexture rt; rt.create(iw, ih);
	rt.clear(); rt.draw(s, tf);
	rt.display();

	// Copy rendered image to output image
	sf::Image outImg = rt.getTexture().copyToImage();
	Image out; out.fromUchar(outImg.getSize().x, outImg.getSize().y, 4, 0x7, outImg.getPixelsPtr());
	return out;
}

void ImgConvert::create() {
	connectToFirstCamera();

	// Get a list of supported shutter speeds within a given range
	int oldtv = getTv();
	std::vector<int> tvlist; getTvList(tvlist);
	int tvmin = CAMTV_1S, tvmax = CAMTV_40;
	auto il = std::lower_bound(tvlist.begin(), tvlist.end(), tvmin);
	auto iu = std::upper_bound(tvlist.begin(), tvlist.end(), tvmax);

	// Display a full intensity image
	Image full(camAlign.sw, camAlign.sh, 1.0);
	showImage(full, true);
	sf::sleep(sf::seconds(0.3f));

	// Loop through shutter times
	std::map<int, std::pair<double, double>> timeToAvgMax;
	for (auto ii = il; ii != iu; ii++) {
		double time = tvStrValMap.at(*ii).second;
		ss << topfolder << calibfolder << "calib_cam_"
			<< std::setprecision(3) << std::fixed << time << "s.png";
		std::string camfname = ss.str(); ss.str("");

		// Take a photo at each speed
		setTv(*ii);
		takeAPicture(camfname.c_str());
		Image cam = readImage(camfname);

		// Align the image and overwrite captured image
		cam = camAlign.alignImage(cam, camAlign.sw / 2, camAlign.sh / 2);
		writeImage(cam, camfname);

		// Save the average intensity value in the map
		double avg = cam.avg();
		double max = cam.max();
		timeToAvgMax [*ii] = std::pair<double, double>(avg, max);
	}

	// Find the first image with overexposure
	auto iend = timeToAvgMax.begin();
	auto ibegin = iend; ibegin++;
	for (auto ii = ibegin; ii != timeToAvgMax.end(); ii++) {
		if ((1.0 - ii->second.second) > DBL_EPSILON) {
			iend = ii; iend--;
			break;
	}}
	// Remove images with overexposure (after the first one)
	int timeMaxI = iend->first;
	double timeMax = tvStrValMap.at(timeMaxI).second;
	if (iend != timeToAvgMax.begin()) timeToAvgMax.erase(timeToAvgMax.begin(), iend);

	// Build a bidirectional mapping of physical radiance vs camera intensity
	physToCam [0.0] = 0.0; camToPhys [0.0] = 0.0;
	for (auto ii = timeToAvgMax.begin(); ii != timeToAvgMax.end(); ii++) {
		double time = tvStrValMap.at(ii->first).second;
		physToCam [time / timeMax] = ii->second.first;
		camToPhys [ii->second.first] = time / timeMax;
	}

	// Set camera shutter time to the maximum used above
	setTv(timeMaxI);

	// Calibrate display using the camera calibration created above
	int skipInt = 8;
	for (int ii = 0; ii < 256; ii += skipInt) {
		double i = ii / 255.0;
		ss << topfolder << calibfolder << "calib_disp_"
			<< std::setprecision(3) << std::fixed << i << ".png";
		std::string camfname = ss.str(); ss.str("");

		// Create an intensity image
		Image disp(camAlign.sw, camAlign.sh, i);

		// Take a picture of the displayed image
		showImage(disp, true);
		windowList.pop_front();
		sf::sleep(sf::seconds(0.1f));
		takeAPicture(camfname.c_str());
		Image cam = readImage(camfname);

		// Align the image and save over captured image
		cam = camAlign.alignImage(cam, camAlign.sw / 2, camAlign.sh / 2);
		writeImage(cam, camfname);

		// Transform the average camera intensity to physical radiance
		double avg = cam.avg();
		double newavg = convertValue(avg, Camera, Physical);

		// Save the display intensity and radiance into the maps
		physToDisp [newavg] = i;
		dispToPhys [i] = newavg;

		// Make sure 255 is also captured
		if (ii < 255 && ii + skipInt >= 256)
			ii = 255 - skipInt;
	}

	disconFromFirstCamera();
}
bool ImgConvert::read(const std::string& filename) {
	// Open conversion file
	FILE* fp = fopen(filename.c_str(), "rb");
	if (!fp) return false;
	
	// Read in camera to/from physical mapping
	int size;
	fread(&size, sizeof(int), 1, fp);
	for (int i = 0; i < size; i++) {
		double k, v;
		fread(&k, sizeof(double), 1, fp);
		fread(&v, sizeof(double), 1, fp);
		camToPhys [k] = v;
		physToCam [v] = k;
	}
	// Read in display to/from physical mapping
	fread(&size, sizeof(int), 1, fp);
	for (int i = 0; i < size; i++) {
		double k, v;
		fread(&k, sizeof(double), 1, fp);
		fread(&v, sizeof(double), 1, fp);
		physToDisp [k] = v;
		dispToPhys [v] = k;
	}
	fclose(fp); fp = NULL;

	return true;
}
bool ImgConvert::write(const std::string& filename) {
	// Open conversion file
	FILE* fp = fopen(filename.c_str(), "wb");
	if (!fp) return false;

	// Write out camera to/from physical mapping
	int size = camToPhys.size();
	fwrite(&size, sizeof(int), 1, fp);
	for (auto it = camToPhys.begin(); it != camToPhys.end(); it++) {
		double k = it->first, v = it->second;
		fwrite(&k, sizeof(double), 1, fp);
		fwrite(&v, sizeof(double), 1, fp);
	}
	// Write out display to/from physical mapping
	size = physToDisp.size();
	fwrite(&size, sizeof(int), 1, fp);
	for (auto it = physToDisp.begin(); it != physToDisp.end(); it++) {
		double k = it->first, v = it->second;
		fwrite(&k, sizeof(double), 1, fp);
		fwrite(&v, sizeof(double), 1, fp);
	}
	fclose(fp); fp = NULL;

	return true;
}
Image ImgConvert::convertImage(const Image& in, ImgSpace from, ImgSpace to) {
	// Figure out which conversion to perform
	switch (from) {
	case Camera:
		switch (to) {
		case Camera:	// Camera to camera (do nothing)
			return in;
		case Physical:	// Camera to physical
			return convertImage(in, camToPhys);
		case Display:	// Camera to display
			return convertImage(convertImage(in, camToPhys), physToDisp);
		} break;
	case Physical:
		switch (to) {
		case Camera:	// Physical to camera
			return convertImage(in, physToCam);
		case Physical:	// Physical to physical (do nothing)
			return in;
		case Display:	// Physical to display
			return convertImage(in, physToDisp);
		} break;
	case Display:
		switch (to) {
		case Camera:	// Display to camera
			return convertImage(convertImage(in, dispToPhys), physToCam);
		case Physical:	// Display to physical
			return convertImage(in, dispToPhys);
		case Display:	// Display to display (do nothing)
			return in;
		} break;
	}
	return in;
}
Image ImgConvert::convertImage(const Image& in, const std::map<double, double>& usemap) {
	// Loop through each pixel
	Image out(in.getWidth(), in.getHeight());
	for (int y = 0; y < out.getHeight(); y++) {
		for (int x = 0; x < out.getWidth(); x++) {
			// Convert the pixel value with convertValue()
			double val = in.getPixel(x, y);
			double newval = convertValue(val, usemap);
			out.setPixel(x, y, newval);
		}
	}
	return out;
}
double ImgConvert::convertValue(double in, ImgSpace from, ImgSpace to) {
	// Figure out which conversion to perform
	switch (from) {
	case Camera:
		switch (to) {
		case Camera:	// Camera to camera (do nothing)
			return in;
		case Physical:	// Camera to physical
			return convertValue(in, camToPhys);
		case Display:	// Camera to display
			return convertValue(convertValue(in, camToPhys), physToDisp);
		} break;
	case Physical:
		switch (to) {
		case Camera:	// Physical to camera
			return convertValue(in, physToCam);
		case Physical:	// Physical to physical (do nothing)
			return in;
		case Display:	// Physical to display
			return convertValue(in, physToDisp);
		} break;
	case Display:
		switch (to) {
		case Camera:	// Display to camera
			return convertValue(convertValue(in, dispToPhys), physToCam);
		case Physical:	// Display to physical
			return convertValue(in, dispToPhys);
		case Display:	// Display to display (do nothing)
			return in;
		} break;
	}
	return in;
}
double ImgConvert::convertValue(double in, const std::map<double, double>& usemap) {
	// Find entries in map that bound input value
	double out;
	auto iu = usemap.upper_bound(in);	// First entry with key > input value
	auto il = iu;						// Last entry with key <= input value
	if (il != usemap.begin()) il--;		// Decrement il if possible (to establish bounds)
	else il = usemap.end();				// Otherwise signal no keys <= input value

	// Determine output value
	if (il == usemap.end()) {
		// Input value is < lowest stored key, set output to lowest stored value
		out = iu->second;
	} else if (iu == usemap.end()) {
		// Input value is >= highest stored key, set output to highest stored value
		out = il->second;
	} else {
		// Linearly interpolate between stored values
		double iuk = iu->first;		// Upper bound key
		double iuv = iu->second;	// Upper bound value
		double ilk = il->first;		// Lower bound key
		double ilv = il->second;	// Lower bound value
		out = (in - ilk) / (iuk - ilk) * (iuv - ilv) + ilv;
	}
	return out;
}