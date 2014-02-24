#include <list>
#include <queue>
#include <iostream>
#include <iomanip>
#include "precorrection.h"
#include "JNB.h"
#include "imagewindow.h"
#include "canoninterface.h"

std::list<ImageWindow> windowList;
std::vector<Image> imageVec;
std::vector<std::string> titleVec;
int rows = 1, cols = 1, imw = 120, imh = 0;

// Image spaces
enum ImgSpace {
	Camera,
	Physical,
	Display
};
const int sm_cam = 0x4;
const int sm_phys = 0x2;
const int sm_disp = 0x1;

void saveImage(const Image& image, const std::string& filename) {
	// Convert Image to RGBA8
	sf::Uint8* buf = new sf::Uint8 [image.getArraySize() * 4];
	image.toUchar(4, 0x7, buf, true);

	sf::Image i;
	i.create(image.getWidth(), image.getHeight(), buf);
	i.saveToFile(filename);
	delete [] buf; buf = NULL;
}
void saveColorImage(const Image& r, const Image& g, const Image& b, const std::string& filename) {
	// Convert images to RGBA8
	sf::Uint8* buf = new sf::Uint8 [r.getArraySize() * 4];
	r.toUchar(4, 0x1, buf, true);
	g.toUchar(4, 0x2, buf);
	b.toUchar(4, 0x4, buf);

	sf::Image i;
	i.create(r.getWidth(), r.getHeight(), buf);
	i.saveToFile(filename);
	delete [] buf; buf = NULL;
}
ImageWindow* showImage(const Image& image, int dx = 0, int dy = 0, const std::string& title = "") {
	ImageWindow w(image, true);
	w.setTitle(title);
	w.move(dx, dy);
	w.update();
	windowList.push_back(std::move(w));
	return &windowList.back();
}
ImageWindow* showColorImage(const Image& r, const Image& g, const Image& b, int dx = 0, int dy = 0, const std::string& title = "") {
	// Convert images to RGBA8
	sf::Uint8* buf = new sf::Uint8 [r.getArraySize() * 4];
	r.toUchar(4, 0x1, buf, true);
	g.toUchar(4, 0x2, buf);
	b.toUchar(4, 0x4, buf);

	ImageWindow w(r.getWidth(), r.getHeight(), buf);
	w.setTitle(title);
	w.move(dx, dy);
	w.update();
	windowList.push_back(std::move(w));
	return &windowList.back();

	delete [] buf; buf = NULL;
}
ImageWindow* showFullscreenImage(const Image& image, bool showBCs = true, BC_ENUM bc = BC_PERIODIC) {
	ImageWindow w(image, false);
	w.position(1680, 0);
	w.resize(1280, 1024);
	w.setColor(0, 0, 0);
	w.setBCs(bc);
	w.showBCs(showBCs);
	w.update();
	windowList.push_back(std::move(w));
	return &windowList.back();
}
Image alignCaptured(const Image& captured, int w, int h, std::string xformFile) {
	// Read in transform matrix
	FILE* fp = fopen(xformFile.c_str(), "rb");
	if (!fp) throw -1;
	int sw, sh;
	double xformMtx [9];
	fread(&sw, sizeof(int), 1, fp);
	fread(&sh, sizeof(int), 1, fp);
	int numRead = fread(xformMtx, sizeof(double), 9, fp);
	if (numRead < 9) { fclose(fp); throw -1; }
	fclose(fp);

	// Copy input image to a GPU texture
	sf::Uint8* buf = new sf::Uint8 [captured.getArraySize() * 4];
	captured.toUchar(4, 0x7, buf, true);
	sf::Texture capTex; capTex.create(captured.getWidth(), captured.getHeight());
	capTex.update(buf); delete [] buf; buf = NULL;
	// Create a sprite for the captured image
	sf::Sprite s; s.setTexture(capTex);

	// Create transformation from camera to monitor
	sf::Transform t1(xformMtx[0], xformMtx[1], xformMtx[2],
					 xformMtx[3], xformMtx[4], xformMtx[5],
					 xformMtx[6], xformMtx[7], xformMtx[8]);
	// Create transformation from monitor to image
	sf::Transform t2 = sf::Transform::Identity;
	t2.translate(-(sw - w) / 2.0, -(sh - h) / 2.0);
	// Combine the two transforms
	sf::Transform t = t2 * t1;

	// Create a new render texture for the transformed captured image
	sf::RenderTexture rt; rt.create(w, h);
	rt.clear();
	rt.draw(s, t);
	rt.display();

	// Copy transformed image to an output image and return it
	sf::Image capXform = rt.getTexture().copyToImage();
	Image out; out.fromUchar(capXform.getSize().x, capXform.getSize().y, 4, 0x7, capXform.getPixelsPtr());
	return out;
}
Image convertImgSpace(const Image& input, ImgSpace from, ImgSpace to, std::string convertPath) {
	// Read in conversion file
	FILE* fp = fopen(convertPath.c_str(), "rb");
	if (!fp) throw -1;
	int size;
	fread(&size, sizeof(int), 1, fp);
	std::map<double, double> camToPhys, physToCam;
	for (int i = 0; i < size; i++) {
		double k, v;
		fread(&k, sizeof(double), 1, fp);
		fread(&v, sizeof(double), 1, fp);
		camToPhys[k] = v;
		physToCam[v] = k;
	}
	fread(&size, sizeof(int), 1, fp);
	std::map<double, double> physToDisp, dispToPhys;
	for (int i = 0; i < size; i++) {
		double k, v;
		fread(&k, sizeof(double), 1, fp);
		fread(&v, sizeof(double), 1, fp);
		physToDisp[k] = v;
		dispToPhys[v] = k;
	}
	fclose(fp); fp = NULL;

	std::map<double, double>* usemap = NULL;
	switch (from) {
	case Camera:
		switch (to) {
		case Camera:	// Camera to camera (do nothing)
			return input; break;
		case Physical:	// Camera to physical
			usemap = &camToPhys; break;
		case Display:	// Camera to display (requires two conversions)
			return convertImgSpace(convertImgSpace(input, Camera, Physical, convertPath),
				Physical, Display, convertPath); break;
		} break;
	case Physical:
		switch (to) {
		case Camera:	// Physical to camera
			usemap = &physToCam; break;
		case Physical:	// Physical to physical (do nothing)
			return input; break;
		case Display:	// Physical to display
			usemap = &physToDisp; break;
		} break;
	case Display:
		switch (to) {
		case Camera:	// Display to camera (requires two conversions)
			return convertImgSpace(convertImgSpace(input, Display, Physical, convertPath),
				Physical, Camera, convertPath); break;
		case Physical:	// Display to physical
			usemap = &dispToPhys; break;
		case Display:	// Display to display (do nothing)
			return input; break;
		} break;
	}

	// Convert the input image
	Image output(input.getWidth(), input.getHeight());
	for (int y = 0; y < output.getHeight(); y++) {
		for (int x = 0; x < output.getWidth(); x++) {
			double val = input.getPixel(x, y);
			double newval;
			auto iu = usemap->upper_bound(val);
			auto il = iu;
			if (il != usemap->begin()) il--; else il = usemap->end();
			if (il == usemap->end()) {
				// Value is < lowest stored key, give lowest value
				newval = iu->second;
			} else if (iu == usemap->end()) {
				// Value is >= highest stored key, give highest value
				newval = il->second;
			} else {
				// Interpolate between higher and lower stored values
				double iui = iu->first;
				double iuo = iu->second;
				double ili = il->first;
				double ilo = il->second;
				newval = (val - ili) / (iui - ili)
					* (iuo - ilo) + ilo;
			}
			output.setPixel(x, y, newval);
		}
	}
	return output;
}
void alignCam(std::string calibpath, int w, int h) {
	std::stringstream ss;
	ss << calibpath << "align_16.png";
	std::string alignImgName = ss.str(); ss.str("");
	ss << calibpath << "align_16_capt.png";
	std::string alignImgCaptName = ss.str(); ss.str("");

	// Create an alignment image
	Image X(1, h, 1.0);
	X = X.resize((w / 4) - 1, h, 0.0)
		.resize((w / 4), h, 1.0)
		.resize((3 * w / 4) - 1, h, 0.0)
		.resize((3 * w / 4), h, 1.0)
		.resize(w - 1, h, 0.0)
		.resize(w, h, 1.0);
	Image Y(w, 1, 1.0);
	Y = Y.resize(w, (h / 4) - 1, 0.0)
		.resize(w, (h / 4), 1.0)
		.resize(w, (3 * h / 4) - 1, 0.0)
		.resize(w, (3 * h / 4), 1.0)
		.resize(w, h - 1, 0.0)
		.resize(w, h, 1.0);
	Image F = (X + Y).clip();
	saveImage(F, alignImgName);

	// Display the alignment image and capture it
	connectToFirstCamera();
	showFullscreenImage(F);
	sf::sleep(sf::seconds(0.1f));
	takeAPicture(alignImgCaptName.c_str());
	disconFromFirstCamera();

	// Now generate transform information with Matlab!
}
void calibCam(std::string calibpath, std::string alignName, std::string convertName, int w, int h) {
	std::stringstream ss;
	connectToFirstCamera();

	// Camera calibration
	// Get a list of supported shutter speeds within a given range
	int oldtv = getTv();
	std::vector<int> tvlist; getTvList(tvlist);
	int tvmin = CAMTV_1S, tvmax = CAMTV_40;
	auto il = std::lower_bound(tvlist.begin(), tvlist.end(), tvmin);
	auto iu = std::upper_bound(tvlist.begin(), tvlist.end(), tvmax);

	// Display a full intensity image
	Image full(w, h, 1.0);
	showFullscreenImage(full);
	sf::sleep(sf::seconds(0.3f));

	// Loop through shutter times
	std::map<int, std::pair<double, double>> timeToAvgMax;
	for (auto ii = il; ii != iu; ii++) {
		double time = tvStrValMap.at(*ii).second;
		ss << calibpath << "calib_cam_"
			<< std::setprecision(3) << std::fixed << time << "s.png";
		std::string camfname = ss.str(); ss.str("");

		// Take a photo at each speed
		setTv(*ii);
		takeAPicture(camfname.c_str());
		sf::Image cam_i; cam_i.loadFromFile(camfname);
		Image cam; cam.fromUchar(cam_i.getSize().x, cam_i.getSize().y, 4, 0x7, cam_i.getPixelsPtr());

		// Crop the image and overwrite captured image
		cam = alignCaptured(cam, w / 2, h / 2, alignName);
		saveImage(cam, camfname);

		// Save the average intensity value in the map
		double avg = cam.avg();
		double max = cam.max();
		timeToAvgMax[*ii] = std::pair<double, double>(avg, max);
	}
	// Remove times for which there is overexposure
	auto iend = timeToAvgMax.begin();
	auto ibegin = iend; ibegin++;
	for (auto ii = ibegin; ii != timeToAvgMax.end(); ii++) {
		if ((1.0 - ii->second.second) > DBL_EPSILON) {
			iend = ii; iend--;
			break;
		}
	}
	int timeMaxI = iend->first;
	double timeMax = tvStrValMap.at(timeMaxI).second;
	std::cout << "Maximum shutter time: " << timeMax << std::endl;
	if (iend != timeToAvgMax.begin()) timeToAvgMax.erase(timeToAvgMax.begin(), iend);

	// Build a bidirectional mapping of physical radiance vs camera intensity
	std::map<double, double> physToCam, camToPhys;
	physToCam[0.0] = 0.0; camToPhys[0.0] = 0.0;
	for (auto ii = timeToAvgMax.begin(); ii != timeToAvgMax.end(); ii++) {
		double time = tvStrValMap.at(ii->first).second;
		physToCam[time / timeMax] = ii->second.first;
		camToPhys[ii->second.first] = time / timeMax;
	}

	// Set camera shutter time to maximum used
	setTv(timeMaxI);

	// Build a bidirectional mapping of physical radiance vs display intensity
	std::map<double, double> physToDisp, dispToPhys;
	int skipInt = 8;
	for (int ii = 0; ii < 256; ii += skipInt) {
		double i = ii / 255.0;
		ss << calibpath << "calib_disp_"
			<< std::setprecision(3) << std::fixed << i << ".png";
		std::string camfname = ss.str(); ss.str("");

		// Create an intensity image
		Image disp(w, h, i);

		// Take a picture of the displayed image
		showFullscreenImage(disp);
		windowList.pop_front();
		sf::sleep(sf::seconds(0.1f));
		takeAPicture(camfname.c_str());
		sf::Image cam_i; cam_i.loadFromFile(camfname);
		Image cam; cam.fromUchar(cam_i.getSize().x, cam_i.getSize().y, 4, 0x7, cam_i.getPixelsPtr());

		// Crop the image and save over captured image
		cam = alignCaptured(cam, w / 2, h / 2, alignName);
		saveImage(cam, camfname);

		// Transform the average camera intensity to physical radiance
		double avg = cam.avg();
		double newavg;
		auto iu = camToPhys.upper_bound(avg);
		auto il = iu;
		if (il != camToPhys.begin()) il--; else il = camToPhys.end();
		if (il == camToPhys.end()) {
			// Value is < lowest stored key, give lowest value
			newavg = iu->second;
		} else if (iu == camToPhys.end()) {
			// Value is >= highest stored key, give highest value
			newavg = il->second;
		} else {
			// Interpolate between higher and lower stored values
			double iui = iu->first;
			double iuo = iu->second;
			double ili = il->first;
			double ilo = il->second;
			newavg = (avg - ili) / (iui - ili)
				* (iuo - ilo) + ilo;
		}

		// Save the display intensity and radiance into the maps
		physToDisp[newavg] = i;
		dispToPhys[i] = newavg;

		// Make sure 255 is also captured
		if (ii < 255 && ii + skipInt >= 256)
			ii = 255 - skipInt;
	}

	disconFromFirstCamera();

	// Write camera and display calibration to a file
	FILE* fp = fopen(convertName.c_str(), "wb");
	if (!fp) exit(-1);
	int size = camToPhys.size();
	fwrite(&size, sizeof(int), 1, fp);
	for (auto ii = camToPhys.begin(); ii != camToPhys.end(); ii++) {
		double k = ii->first, v = ii->second;
		fwrite(&k, sizeof(double), 1, fp);
		fwrite(&v, sizeof(double), 1, fp);
	}
	size = physToDisp.size();
	fwrite(&size, sizeof(int), 1, fp);
	for (auto ii = physToDisp.begin(); ii != physToDisp.end(); ii++) {
		double k = ii->first, v = ii->second;
		fwrite(&k, sizeof(double), 1, fp);
		fwrite(&v, sizeof(double), 1, fp);
	}
	fclose(fp);
}
void addImageToGrid(const Image& image, const std::string& title = "") {
	if (image.getWidth() > imw) imw = image.getWidth();
	if (image.getHeight() > imh) imh = image.getHeight();
	imageVec.push_back(image);
	titleVec.push_back(title);
}
void showGrid() {
	std::reverse(imageVec.begin(), imageVec.end());
	std::reverse(titleVec.begin(), titleVec.end());

	// Show images
	for (int r = 0; r < rows; r++) {
		for (int c = 0; c < cols; c++) {
			if (imageVec.empty()) break;
			int x = (imw + 28) * (c - cols / 2) + ((cols + 1) % 2) * (imw / 2 + 14);
			int y = (imh + 48) * (r - rows / 2) + ((rows + 1) % 2) * (imh / 2 + 24) - 30;
			showImage(imageVec.back(), x, y, titleVec.back());
			imageVec.pop_back(); titleVec.pop_back();
		}
	}
}
void loop() {
	while (!windowList.empty()) {
		for (auto it = windowList.begin(); it != windowList.end();) {
			it->update();

			if (!it->isOpen()) {
				//windowList.erase(it++);
				windowList.clear();
				break;
			} else it++;
		}
	}
}

void test1(const Image& F, const Image& PSF) {
	int fw = F.getWidth();
	int fh = F.getHeight();

	Options opts;
	opts.relchg = 1e-3;
	double theta = 9.3e5;
	sf::Clock c;

	std::cout << "TVL1 Reflexive:" << std::endl;
	opts.tv = TVL1;
	opts.bc = BC_REFLEXIVE;
	c.restart();
	//Image KtF_L1R = precorrect(F, PSF, theta, opts);
	Image KtF_L1R; KtF_L1R.fromBinary("lenna_KtF_L1R.dbl");
	std::cout << "Elapsed time: " << c.getElapsedTime().asSeconds() << " seconds" << std::endl << std::endl;
	KtF_L1R.toBinary("lenna_KtF_L1R.dbl");

	std::cout << "TVL2 Reflexive:" << std::endl;
	opts.tv = TVL2;
	c.restart();
	//Image KtF_L2R = precorrect(F, PSF, theta, opts);
	Image KtF_L2R; KtF_L2R.fromBinary("lenna_KtF_L2R.dbl");
	std::cout << "Elapsed time: " << c.getElapsedTime().asSeconds() << " seconds" << std::endl << std::endl;
	KtF_L2R.toBinary("lenna_KtF_L2R.dbl");

	Image KKtF_L1R = convolve(KtF_L1R, PSF, BC_REFLEXIVE);
	Image KKtF_L2R = convolve(KtF_L2R, PSF, BC_REFLEXIVE);

	std::cout << "TVL1 PSNR: " << KKtF_L1R.psnr(F) << "db" << std::endl;
	std::cout << "TVL1 Sharpness: " << KKtF_L1R.sharpness(BC_REFLEXIVE) << std::endl;
	std::cout << "TVL1 Contrast: " << KKtF_L1R.contrast() << std::endl;
	std::cout << "TVL1 Sharpness / Contrast: " << KKtF_L1R.sharpness(BC_REFLEXIVE) / KKtF_L1R.contrast() << std::endl;
	std::cout << "TVL1 JNB Sharpness: " << sharpnessJNB(KKtF_L1R) << std::endl;
	std::cout << std::endl;

	std::cout << "TVL2 PSNR: " << KKtF_L2R.psnr(F) << "db" << std::endl;
	std::cout << "TVL2 Sharpness: " << KKtF_L2R.sharpness(BC_REFLEXIVE) << std::endl;
	std::cout << "TVL2 Contrast: " << KKtF_L2R.contrast() << std::endl;
	std::cout << "TVL2 Sharpness / Contrast: " << KKtF_L2R.sharpness(BC_REFLEXIVE) / KKtF_L2R.contrast() << std::endl;
	std::cout << "TVL2 JNB Sharpness: " << sharpnessJNB(KKtF_L2R) << std::endl;

	rows = 2; cols = 2;
	addImageToGrid(KtF_L1R, "K\'F TVL1 R");
	addImageToGrid(KKtF_L1R, "KK\'F TVL1 R");
	addImageToGrid(KtF_L2R, "K\'F TVL2 R");
	addImageToGrid(KKtF_L2R, "KK\'F TVL2 R");

	showGrid();
}
void test2(const Image& F, const Image& PSF) {
	// Load several PSFs
	Image PSF_0125; PSF_0125.fromBinary("PSF_0.125.dbl");
	Image PSF_0250; PSF_0250.fromBinary("PSF_0.250.dbl");
	Image PSF_0375; PSF_0375.fromBinary("PSF_0.375.dbl");
	Image PSF_0500; PSF_0500.fromBinary("PSF_0.500.dbl");
	Image PSF_0625; PSF_0625.fromBinary("PSF_0.625.dbl");
	Image PSF_0750; PSF_0750.fromBinary("PSF_0.750.dbl");
	Image PSF_0875; PSF_0875.fromBinary("PSF_0.875.dbl");
	Image PSF_1000; PSF_1000.fromBinary("PSF_1.000.dbl");
	Image PSF_1125; PSF_1125.fromBinary("PSF_1.125.dbl");

	// Create several blurry images
	Image KF_0125 = convolve(F, PSF_0125, BC_PERIODIC);
	Image KF_0250 = convolve(F, PSF_0250, BC_PERIODIC);
	Image KF_0375 = convolve(F, PSF_0375, BC_PERIODIC);
	Image KF_0500 = convolve(F, PSF_0500, BC_PERIODIC);
	Image KF_0625 = convolve(F, PSF_0625, BC_PERIODIC);
	Image KF_0750 = convolve(F, PSF_0750, BC_PERIODIC);
	Image KF_0875 = convolve(F, PSF_0875, BC_PERIODIC);
	Image KF_1000 = convolve(F, PSF_1000, BC_PERIODIC);
	Image KF_1125 = convolve(F, PSF_1125, BC_PERIODIC);

	// Measure each's sharpness
	std::cout << "0.000 diopters: " << sharpnessJNB(F) << std::endl;
	std::cout << "0.125 diopters: " << sharpnessJNB(KF_0125) << std::endl;
	std::cout << "0.250 diopters: " << sharpnessJNB(KF_0250) << std::endl;
	std::cout << "0.375 diopters: " << sharpnessJNB(KF_0375) << std::endl;
	std::cout << "0.500 diopters: " << sharpnessJNB(KF_0500) << std::endl;
	std::cout << "0.625 diopters: " << sharpnessJNB(KF_0625) << std::endl;
	std::cout << "0.750 diopters: " << sharpnessJNB(KF_0750) << std::endl;
	std::cout << "0.875 diopters: " << sharpnessJNB(KF_0875) << std::endl;
	std::cout << "1.000 diopters: " << sharpnessJNB(KF_1000) << std::endl;
	std::cout << "1.125 diopters: " << sharpnessJNB(KF_1125) << std::endl;

	// Display each image
	rows = 2; cols = 5;
	addImageToGrid(F, "F");
	addImageToGrid(KF_0125, "KF 0.125 dpt");
	addImageToGrid(KF_0250, "KF 0.250 dpt");
	addImageToGrid(KF_0375, "KF 0.375 dpt");
	addImageToGrid(KF_0500, "KF 0.500 dpt");
	addImageToGrid(KF_0625, "KF 0.625 dpt");
	addImageToGrid(KF_0750, "KF 0.750 dpt");
	addImageToGrid(KF_0875, "KF 0.875 dpt");
	addImageToGrid(KF_1000, "KF 1.000 dpt");
	addImageToGrid(KF_1125, "KF 1.125 dpt");

	showGrid();
}

// Structure to hold PSF creation parameters along with test result
struct PSFParm {
	PSFParm(double pdpt, double pap, double psf, double perr) :
		dpt(pdpt), ap(pap), sf(psf), err(perr) {}
	double dpt;		// Diopter power of PSF
	double ap;		// Pupil diameter
	double sf;		// Scale factor
	double err;		// Error value
};
// Functor to compare the MSE of two PSFParm objects
class PSFCmp {
public:
	bool operator() (const PSFParm& lhs, const PSFParm& rhs) const {
		return lhs.err > rhs.err;	// Reverse order so smallest error at front
	}
};

int w = 1280, h = 1024;
double dpt_cam = 2.25;
std::stringstream ss;
std::string topfolder;
std::string calibfolder = "calib/";
std::string calibpath;
std::string psffolder = "psf/";
std::string imgfolder = "img/";
std::string captfolder = "captprec/";
std::string imgprefix = "lenna";
std::string alignName;
std::string convertName;

int main(void) {
	ss << "camera_2.0m_f22_" << std::setprecision(2) << std::fixed << dpt_cam << "d_1/";
	topfolder = ss.str(); ss.str("");
	ss << topfolder << calibfolder;
	calibpath = ss.str(); ss.str("");
	ss << topfolder << calibfolder << "align.dat";
	alignName = ss.str(); ss.str("");
	ss << topfolder << calibfolder << "convert.dat";
	convertName = ss.str(); ss.str("");

	bool searchPSF = true;			// Whether to search the PSF space (else use predefined params)
	bool loadPSFTests = false;		// Whether to load test images instead of capture them
	bool usegrad = false;			// Whether to use the gradient in PSF testing
	int saveMask = sm_disp;			// Which spaces to save
	bool align = false;				// Align the camera (don't do anything else)
	bool calib = false;				// Calibrate the camera (don't do anything else)
	int numBest = 5;				// Pick the top numBest images to fully test

	// Align the camera
	if (align) {
		alignCam(calibpath, w, h);
		loop();
		return 0;
	}
	// Calibrate the camera intensity
	if (calib) {
		calibCam(calibpath, alignName, convertName, w, h);
		return 0;
	}

	// Load the test image
	Image F_disp;
	ss << imgfolder << imgprefix << ".png";
	sf::Image img; img.loadFromFile(ss.str()); ss.str("");
	F_disp.fromUchar(img.getSize().x, img.getSize().y, 4, 0x7, img.getPixelsPtr());
	ss << topfolder << imgprefix << "_disp.png";
	if (saveMask & sm_disp) saveImage(F_disp, ss.str()); ss.str("");

	// Convert original image into physical image space
	Image F_phys = convertImgSpace(F_disp, Display, Physical, convertName);
	ss << topfolder << imgprefix << "_phys.png";
	if (saveMask & sm_phys) saveImage(F_phys, ss.str()); ss.str("");
	Image F_cam = convertImgSpace(F_phys, Physical, Camera, convertName);
	ss << topfolder << imgprefix << "_cam.png";
	if (saveMask & sm_cam) saveImage(F_cam, ss.str()); ss.str("");

	// Capture the test image
	connectToFirstCamera();
	showFullscreenImage(F_disp);
	sf::sleep(sf::seconds(1.0f));
	ss << topfolder << imgprefix << "_capt_cam.png";
	takeAPicture(ss.str().c_str());
	disconFromFirstCamera();

	// Load, then transform and crop the captured image
	img.loadFromFile(ss.str()); ss.str("");
	Image F_capt_cam; F_capt_cam.fromUchar(img.getSize().x, img.getSize().y, 4, 0x7, img.getPixelsPtr());
	F_capt_cam = alignCaptured(F_capt_cam, F_disp.getWidth(), F_disp.getHeight(), alignName);
	ss << topfolder << imgprefix << "_capt_cam.png";
	saveImage(F_capt_cam, ss.str()); ss.str("");

	// Convert captured image into physical and display space
	Image F_capt_phys = convertImgSpace(F_capt_cam, Camera, Physical, convertName);
	ss << topfolder << imgprefix << "_capt_phys.png";
	if (saveMask & sm_phys) saveImage(F_capt_phys, ss.str()); ss.str("");
	Image F_capt_disp = convertImgSpace(F_capt_phys, Physical, Display, convertName);
	ss << topfolder << imgprefix << "_capt_disp.png";
	if (saveMask & sm_disp) saveImage(F_capt_disp, ss.str()); ss.str("");


	/////////////////////////////////// SEARCH PSFS ///////////////////////////////////

	double dpt_best = 2.0;
	double ap_best = 5.4;
	double sf_best = 0.6;
	double err_best = 0.0;
	// Create a priority queue to hold the smallest MSE
	std::priority_queue<PSFParm, std::vector<PSFParm>, PSFCmp> pqueue;
	if (searchPSF) {
		if (!loadPSFTests) connectToFirstCamera();

		std::cout << "Diopters\tAperture\tScale\t\tError" << std::endl;
		double dpt_min = 2.25;	double ap_min = 5.9;		double sf_min = 0.4;
		double dpt_max = 2.25;	double ap_max = 6.1;		double sf_max = 0.6;
		double dpt_step = 0.2;	double ap_step = 0.02;		double sf_step = 0.02;
		int i_dpt_max = (int)((dpt_max + dpt_step - dpt_min) / dpt_step + 0.5);
		int i_ap_max = (int)((ap_max + ap_step - ap_min) / ap_step + 0.5);
		int i_sf_max = (int)((sf_max + sf_step - sf_min) / sf_step + 0.5);
		for (int i_dpt = 0; i_dpt < i_dpt_max; i_dpt++) {
			double dpt_psf = dpt_min + i_dpt * dpt_step;

			// Create an image to visualize the MSE
			Image psfErr(i_sf_max, i_ap_max);

			for (int i_ap = 0; i_ap < i_ap_max; i_ap++) {
				double ap_psf = ap_min + i_ap * ap_step;
				for (int i_sf = 0; i_sf < i_sf_max; i_sf++) {
					double sf_psf = sf_min + i_sf * sf_step;

					ss << std::setprecision(2) << std::fixed << dpt_psf << "d_"
						<< std::setprecision(4) << std::fixed << ap_psf << "ap_"
						<< std::setprecision(2) << std::fixed << sf_psf << "sf";
					std::string params = ss.str(); ss.str("");

					Image F_prec_L2_conv_phys, F_prec_L2_capt_phys;
					ss << topfolder << captfolder << imgprefix << "_" << params << "_prec_L2_conv_phys.png";
					std::string convname = ss.str(); ss.str("");
					ss << topfolder << captfolder << imgprefix << "_" << params << "_prec_L2_capt_phys.png";
					std::string captname = ss.str(); ss.str("");
					if (!loadPSFTests) {
						// Load the PSF
						ss << psffolder << "psf_" << params << ".dbl";
						Image PSF; PSF.fromBinary(ss.str().c_str()); ss.str("");

						// Precorrect original image
						Options opts; opts.tv = TVL2; opts.maxiter = 1; opts.print = false;
						Image F_prec_L2_phys = precorrect(F_phys, PSF, 9.3e4, opts);
						Image F_prec_L2_disp = convertImgSpace(F_prec_L2_phys, Physical, Display, convertName);

						// Convolve precorrection
						F_prec_L2_conv_phys = convolve(F_prec_L2_phys, PSF, BC_PERIODIC);
						saveImage(F_prec_L2_conv_phys, convname);

						// Capture precorrection
						showFullscreenImage(F_prec_L2_disp);
						if (windowList.size() > 1) windowList.pop_front();
						sf::sleep(sf::seconds(0.1f));
						takeAPicture(captname.c_str());

						// Align and convert captured image
						img.loadFromFile(captname);
						Image F_prec_L2_capt_cam; F_prec_L2_capt_cam.fromUchar(img.getSize().x, img.getSize().y, 4, 0x7, img.getPixelsPtr());
						F_prec_L2_capt_cam = alignCaptured(F_prec_L2_capt_cam, F_disp.getWidth(), F_disp.getHeight(), alignName);
						F_prec_L2_capt_phys = convertImgSpace(F_prec_L2_capt_cam, Camera, Physical, convertName);
						saveImage(F_prec_L2_capt_phys, captname);
					} else {
						img.loadFromFile(convname);
						F_prec_L2_conv_phys.fromUchar(img.getSize().x, img.getSize().y, 4, 0x7, img.getPixelsPtr());
						img.loadFromFile(captname);
						F_prec_L2_capt_phys.fromUchar(img.getSize().x, img.getSize().y, 4, 0x7, img.getPixelsPtr());
					}

					// Calculate error and output
					Image& conv = F_prec_L2_conv_phys;
					Image& capt = F_prec_L2_capt_phys;
					double err;
					if (usegrad) {
						Image grad = ((conv.diffX(BC_PERIODIC) ^ 2) + (conv.diffY(BC_PERIODIC) ^ 2)).sqrt();
						err = (((conv - capt) ^ 2) * grad.scale()).sum() / (conv.getArraySize());
					} else {
						err = ((conv - capt) ^ 2).sum() / (conv.getArraySize());
					}
					std::cout << std::setprecision(2) << std::fixed << dpt_psf << "d\t\t"
						<< std::setprecision(4) << std::fixed << ap_psf << "ap\t"
						<< std::setprecision(2) << std::fixed << sf_psf << "sf\t\t"
						<< std::setprecision(8) << std::fixed << err << std::endl;

					// Save err value into err image
					psfErr.setPixel(i_sf, i_ap, err);

					// Create a PSFParm and add it to the priority queue
					PSFParm psfparm(dpt_psf, ap_psf, sf_psf, err);
					pqueue.push(psfparm);
				}
			}

			psfErr = psfErr.scale().equalize(1024);
			ss << topfolder << imgprefix << "_"
				<< std::setprecision(2) << std::fixed << dpt_psf << "d_err.png";
			saveImage(psfErr, ss.str()); ss.str("");
		}

		// Get parameters for smallest MSE value
		const PSFParm& psfparm = pqueue.top();
		dpt_best = psfparm.dpt;
		ap_best = psfparm.ap;
		sf_best = psfparm.sf;
		err_best = psfparm.err;

		if (!loadPSFTests) disconFromFirstCamera();
	} else {
		PSFParm ppm(dpt_best, ap_best, sf_best, err_best);
		pqueue.push(ppm);
	}

	// Create a log file
	ss << topfolder << imgprefix << "_log.txt";
	FILE* logfp = fopen(ss.str().c_str(), "w"); ss.str("");
	if (!logfp) std::cout << "Could not open log file!" << std::endl;

	for (int i = 0; !pqueue.empty() && i < numBest; i++) {
		const PSFParm& ppm = pqueue.top(); pqueue.pop();

		// Get PSF parameters into a string
		ss << std::setprecision(2) << std::fixed << ppm.dpt << "d_"
			<< std::setprecision(4) << std::fixed << ppm.ap << "ap_"
			<< std::setprecision(2) << std::fixed << ppm.sf << "sf";
		std::string params = ss.str(); ss.str("");

		// Load the PSF
		ss << psffolder << "psf_" << params << ".dbl";
		Image PSF; PSF.fromBinary(ss.str().c_str()); ss.str("");

		// Convolve the original image
		Image F_conv_phys = convolve(F_phys, PSF, BC_PERIODIC);
		Image F_conv_disp = convertImgSpace(F_conv_phys, Physical, Display, convertName);
		Image F_conv_cam = convertImgSpace(F_conv_phys, Physical, Camera, convertName);
		ss << topfolder << imgprefix << "_" << params << "_conv_phys.png";
		if (saveMask & sm_phys) saveImage(F_conv_phys, ss.str()); ss.str("");
		ss << topfolder << imgprefix << "_" << params << "_conv_disp.png";
		if (saveMask & sm_disp) saveImage(F_conv_disp, ss.str()); ss.str("");
		ss << topfolder << imgprefix << "_" << params << "_conv_cam.png";
		if (saveMask & sm_cam) saveImage(F_conv_cam, ss.str()); ss.str("");

		// Output MSE
		ss << std::setprecision(2) << std::fixed << ppm.dpt << "d\t\t"
			<< std::setprecision(4) << std::fixed << ppm.ap << "ap\t"
			<< std::setprecision(2) << std::fixed << ppm.sf << "sf\t\t"
			<< std::setprecision(8) << std::fixed << ppm.err << std::endl;
		std::string outParam = ss.str(); ss.str("");
		std::cout << std::endl << "# " << i+1 << ":" << std::endl << outParam;
		if (logfp) fprintf(logfp, outParam.c_str());


		////////////////////////////// PRECORRECT //////////////////////////////

		Image F_prec_L1_phys, F_prec_L1_disp, F_prec_L1_cam;
		Image F_prec_L2_phys, F_prec_L2_disp, F_prec_L2_cam;
		ss << topfolder << imgprefix << "_" << params << "_prec_L1_phys.png";
		std::string F_prec_L1_phys_str = ss.str(); ss.str("");
		ss << topfolder << imgprefix << "_" << params << "_prec_L1_conv_phys.png";
		std::string F_prec_L1_conv_phys_str = ss.str(); ss.str("");
		ss << topfolder << imgprefix << "_" << params << "_prec_L1_disp.png";
		std::string F_prec_L1_disp_str = ss.str(); ss.str("");
		ss << topfolder << imgprefix << "_" << params << "_prec_L1_conv_disp.png";
		std::string F_prec_L1_conv_disp_str = ss.str(); ss.str("");
		ss << topfolder << imgprefix << "_" << params << "_prec_L1_cam.png";
		std::string F_prec_L1_cam_str = ss.str(); ss.str("");
		ss << topfolder << imgprefix << "_" << params << "_prec_L1_conv_cam.png";
		std::string F_prec_L1_conv_cam_str = ss.str(); ss.str("");
		ss << topfolder << imgprefix << "_" << params << "_prec_L2_phys.png";
		std::string F_prec_L2_phys_str = ss.str(); ss.str("");
		ss << topfolder << imgprefix << "_" << params << "_prec_L2_conv_phys.png";
		std::string F_prec_L2_conv_phys_str = ss.str(); ss.str("");
		ss << topfolder << imgprefix << "_" << params << "_prec_L2_disp.png";
		std::string F_prec_L2_disp_str = ss.str(); ss.str("");
		ss << topfolder << imgprefix << "_" << params << "_prec_L2_conv_disp.png";
		std::string F_prec_L2_conv_disp_str = ss.str(); ss.str("");
		ss << topfolder << imgprefix << "_" << params << "_prec_L2_cam.png";
		std::string F_prec_L2_cam_str = ss.str(); ss.str("");
		ss << topfolder << imgprefix << "_" << params << "_prec_L2_conv_cam.png";
		std::string F_prec_L2_conv_cam_str = ss.str(); ss.str("");

		// Precorrect the test image
		Options opts; opts.tv = TVL1;
		F_prec_L1_phys = precorrect(F_phys, PSF, 9.3e4, opts);
		if (saveMask & sm_phys) saveImage(F_prec_L1_phys, F_prec_L1_phys_str);
		F_prec_L1_disp = convertImgSpace(F_prec_L1_phys, Physical, Display, convertName);
		if (saveMask & sm_disp) saveImage(F_prec_L1_disp, F_prec_L1_disp_str);
		F_prec_L1_cam = convertImgSpace(F_prec_L1_phys, Physical, Camera, convertName);
		if (saveMask & sm_cam) saveImage(F_prec_L1_cam, F_prec_L1_cam_str);

		opts.tv = TVL2;
		F_prec_L2_phys = precorrect(F_phys, PSF, 9.3e4, opts);
		if (saveMask & sm_phys) saveImage(F_prec_L2_phys, F_prec_L2_phys_str);
		F_prec_L2_disp = convertImgSpace(F_prec_L2_phys, Physical, Display, convertName);
		if (saveMask & sm_disp) saveImage(F_prec_L2_disp, F_prec_L2_disp_str);
		F_prec_L2_cam = convertImgSpace(F_prec_L2_phys, Physical, Camera, convertName);
		if (saveMask & sm_cam) saveImage(F_prec_L2_cam, F_prec_L2_cam_str);

		// Convolve precorrections
		Image F_prec_L1_conv_phys = convolve(F_prec_L1_phys, PSF, BC_PERIODIC);
		if (saveMask & sm_phys) saveImage(F_prec_L1_conv_phys, F_prec_L1_conv_phys_str);
		Image F_prec_L1_conv_disp = convertImgSpace(F_prec_L1_conv_phys, Physical, Display, convertName);
		if (saveMask & sm_disp) saveImage(F_prec_L1_conv_disp, F_prec_L1_conv_disp_str);
		Image F_prec_L1_conv_cam = convertImgSpace(F_prec_L1_conv_phys, Physical, Camera, convertName);
		if (saveMask & sm_cam) saveImage(F_prec_L1_conv_cam, F_prec_L1_conv_cam_str);

		Image F_prec_L2_conv_phys = convolve(F_prec_L2_phys, PSF, BC_PERIODIC);
		if (saveMask & sm_phys) saveImage(F_prec_L2_conv_phys, F_prec_L2_conv_phys_str);
		Image F_prec_L2_conv_disp = convertImgSpace(F_prec_L2_conv_phys, Physical, Display, convertName);
		if (saveMask & sm_disp) saveImage(F_prec_L2_conv_disp, F_prec_L2_conv_disp_str);
		Image F_prec_L2_conv_cam = convertImgSpace(F_prec_L2_conv_phys, Physical, Camera, convertName);
		if (saveMask & sm_cam) saveImage(F_prec_L2_conv_cam, F_prec_L2_conv_cam_str);


		///////////////////////////////////// CAPTURE PRECORRECTIONS /////////////////////

		ss << topfolder << imgprefix << "_" << params << "_prec_L1_capt_cam.png";
		std::string F_prec_L1_capt_cam_str = ss.str(); ss.str("");
		ss << topfolder << imgprefix << "_" << params << "_prec_L1_capt_phys.png";
		std::string F_prec_L1_capt_phys_str = ss.str(); ss.str("");
		ss << topfolder << imgprefix << "_" << params << "_prec_L1_capt_disp.png";
		std::string F_prec_L1_capt_disp_str = ss.str(); ss.str("");
		ss << topfolder << imgprefix << "_" << params << "_prec_L2_capt_cam.png";
		std::string F_prec_L2_capt_cam_str = ss.str(); ss.str("");
		ss << topfolder << imgprefix << "_" << params << "_prec_L2_capt_phys.png";
		std::string F_prec_L2_capt_phys_str = ss.str(); ss.str("");
		ss << topfolder << imgprefix << "_" << params << "_prec_L2_capt_disp.png";
		std::string F_prec_L2_capt_disp_str = ss.str(); ss.str("");

		// Display precorrected images and capture
		connectToFirstCamera();
		showFullscreenImage(F_prec_L1_disp);
		sf::sleep(sf::seconds(1.0f));
		takeAPicture(F_prec_L1_capt_cam_str.c_str());

		showFullscreenImage(F_prec_L2_disp);
		sf::sleep(sf::seconds(1.0f));
		takeAPicture(F_prec_L2_capt_cam_str.c_str());
		disconFromFirstCamera();

		// Load, align, and convert captured precorrections
		img.loadFromFile(F_prec_L1_capt_cam_str);
		Image F_prec_L1_capt_cam; F_prec_L1_capt_cam.fromUchar(img.getSize().x, img.getSize().y, 4, 0x7, img.getPixelsPtr());
		F_prec_L1_capt_cam = alignCaptured(F_prec_L1_capt_cam, F_disp.getWidth(), F_disp.getHeight(), alignName);
		saveImage(F_prec_L1_capt_cam, F_prec_L1_capt_cam_str);
		Image F_prec_L1_capt_phys = convertImgSpace(F_prec_L1_capt_cam, Camera, Physical, convertName);
		if (saveMask & sm_phys) saveImage(F_prec_L1_capt_phys, F_prec_L1_capt_phys_str);
		Image F_prec_L1_capt_disp = convertImgSpace(F_prec_L1_capt_phys, Physical, Display, convertName);
		if (saveMask & sm_disp) saveImage(F_prec_L1_capt_disp, F_prec_L1_capt_disp_str);

		img.loadFromFile(F_prec_L2_capt_cam_str);
		Image F_prec_L2_capt_cam; F_prec_L2_capt_cam.fromUchar(img.getSize().x, img.getSize().y, 4, 0x7, img.getPixelsPtr());
		F_prec_L2_capt_cam = alignCaptured(F_prec_L2_capt_cam, F_disp.getWidth(), F_disp.getHeight(), alignName);
		saveImage(F_prec_L2_capt_cam, F_prec_L2_capt_cam_str);
		Image F_prec_L2_capt_phys = convertImgSpace(F_prec_L2_capt_cam, Camera, Physical, convertName);
		if (saveMask & sm_phys) saveImage(F_prec_L2_capt_phys, F_prec_L2_capt_phys_str);
		Image F_prec_L2_capt_disp = convertImgSpace(F_prec_L2_capt_phys, Physical, Display, convertName);
		if (saveMask & sm_disp) saveImage(F_prec_L2_capt_disp, F_prec_L2_capt_disp_str);
	}

	if (logfp) fclose(logfp);
	std::cout << "Done!" << std::endl;

	loop();

	return 0;
}