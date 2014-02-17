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
ImageWindow* showFullscreenImage(const Image& image) {
	ImageWindow w(image, false);
	w.position(1680, 0);
	w.resize(1280, 1024);
	w.setColor(0, 0, 0);
	w.update();
	windowList.push_back(std::move(w));
	return &windowList.back();
}
Image xformCaptured(const Image& captured, int w, int h, std::string xformFile) {
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
Image correctGamma(const Image& captured, std::string gammaPath) {
	// Read in gamma file
	FILE* fp = fopen(gammaPath.c_str(), "rb");
	if (!fp) throw -1;
	int size;
	fread(&size, sizeof(int), 1, fp);
	std::map<double, double> camToDisp;
	for (int i = 0; i < size; i++) {
		double k, v;
		fread(&k, sizeof(double), 1, fp);
		fread(&v, sizeof(double), 1, fp);
		camToDisp[k] = v;
	}
	fclose(fp); fp = NULL;

	Image capGamma(captured.getWidth(), captured.getHeight());
	for (int y = 0; y < capGamma.getHeight(); y++) {
		for (int x = 0; x < capGamma.getWidth(); x++) {
			double val = captured.getPixel(x, y);
			auto iu = camToDisp.upper_bound(val);
			auto il = iu;
			if (il != camToDisp.begin()) il--; else il = camToDisp.end();
			double ilc, ild, iuc, iud;
			if (il == camToDisp.end()) {
				ilc = 0.0; ild = 0.0;
				iuc = iu->first;
				iud = iu->second;
			} else if (iu == camToDisp.end()) {
				iuc = 1.0; iud = 1.0;
				ilc = il->first;
				ild = il->second;
			} else {
				iuc = iu->first;
				iud = iu->second;
				ilc = il->first;
				ild = il->second;
			}
			double newval = (val - ilc) / (iuc - ilc)
				* (iud - ild) + ild;
			capGamma.setPixel(x, y, newval);
		}
	}
	return capGamma;
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

int w = 1280, h = 1024;
std::stringstream ss;
std::string topfolder = "camcalib_0/";
std::string imgprefix = "calib";
std::string alignfname = "camxform";
std::string gammafname = "camgamma";

void alignCam() {
	ss << topfolder << "align_16.png";
	std::string alignName = ss.str(); ss.str("");
	ss << topfolder << "align_16_cam.png";
	std::string alignCamName = ss.str(); ss.str("");

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
	saveImage(F, alignName);

	// Display the alignment image and capture it
	connectToFirstCamera();
	showFullscreenImage(F);
	sf::sleep(sf::seconds(0.1f));
	takeAPicture(alignCamName.c_str());
	disconFromFirstCamera();

	// Now generate transform information with Matlab!
}
std::map<double, double> calibCam() {
	connectToFirstCamera();

	// Create a mapping from display intensity to camera intensity
	std::map<double, double> camToDisp;
	for (int ii = 0; ii < 256; ii += 8) {
		double i = ii / 255.0;

		// Create an intensity image
		Image disp(w, h, i);

		// Take a picture of the displayed image
		showFullscreenImage(disp);
		sf::sleep(sf::seconds(0.1f));
		ss << topfolder << imgprefix << "_"
			<< std::setprecision(3) << std::fixed << i << ".png";
		std::string camfname = ss.str(); ss.str("");
		takeAPicture(camfname.c_str());
		sf::Image cam_i; cam_i.loadFromFile(camfname);
		Image cam; cam.fromUchar(cam_i.getSize().x, cam_i.getSize().y, 4, 0x7, cam_i.getPixelsPtr());

		// Crop the image and save over captured image
		ss << topfolder << alignfname;
		cam = xformCaptured(cam, w / 2, h / 2, ss.str()); ss.str("");
		saveImage(cam, camfname);

		// Save the average intensity value in the map
		camToDisp[cam.avg()] = i;

		// Make sure 255 is also captured
		if (ii < 255 && ii + 8 >= 256)
			ii = 255 - 8;
	}

	disconFromFirstCamera();

	// Write camera gamma curve to file
	ss << topfolder << gammafname;
	FILE* fp = fopen(ss.str().c_str(), "wb"); ss.str("");
	if (!fp) exit(-1);
	int size = camToDisp.size();
	fwrite(&size, sizeof(int), 1, fp);
	for (auto ii = camToDisp.begin(); ii != camToDisp.end(); ii++) {
		double k = ii->first, v = ii->second;
		fwrite(&k, sizeof(double), 1, fp);
		fwrite(&v, sizeof(double), 1, fp);
	}
	fclose(fp);

	return camToDisp;
}

int main(void) {
	ss << topfolder << alignfname;
	std::string camxformName = ss.str(); ss.str("");
	ss << topfolder << gammafname;
	std::string camgammaName = ss.str(); ss.str("");

	// Load lenna
	ss << topfolder << "lenna.png";
	sf::Image Fi; Fi.loadFromFile(ss.str()); ss.str("");
	Image F; F.fromUchar(Fi.getSize().x, Fi.getSize().y, 4, 0x7, Fi.getPixelsPtr());

	// Display and capture lenna
	connectToFirstCamera();
	showFullscreenImage(F);
	sf::sleep(sf::seconds(0.1f));
	ss << topfolder << "lenna_cam.png";
	takeAPicture(ss.str().c_str());
	disconFromFirstCamera();

	// Load captured image and crop
	Fi.loadFromFile(ss.str());
	Image Fcap; Fcap.fromUchar(Fi.getSize().x, Fi.getSize().y, 4, 0x7, Fi.getPixelsPtr());
	Fcap = xformCaptured(Fcap, F.getWidth(), F.getHeight(), camxformName);
	saveImage(Fcap, ss.str()); ss.str("");

	// Gamma-correct image to display space
	Fcap = correctGamma(Fcap, camgammaName);
	ss << topfolder << "lenna_cam_gam.png";
	saveImage(Fcap, ss.str()); ss.str("");

	return 0;
}