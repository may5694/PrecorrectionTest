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

// Structure to hold PSF creation parameters along with test result
struct PSFParm {
	PSFParm(double pdpt, double pap, double psf, double pmse) :
		dpt(pdpt), ap(pap), sf(psf), mse(pmse) {}
	double dpt;		// Diopter power of PSF
	double ap;		// Pupil diameter
	double sf;		// Scale factor
	double mse;		// MSE of convolved test image against captured test image
};
// Functor to compare the MSE of two PSFParm objects
class PSFCmp {
public:
	bool operator() (const PSFParm& lhs, const PSFParm& rhs) const {
		return lhs.mse > rhs.mse;	// Reverse order so smallest MSE at front
	}
};

int w = 1280, h = 1024;
double dpt_cam = 2.00;
std::stringstream ss;
std::string topfolder;
std::string calibfolder = "calib/";
std::string psffolder = "psf/";
std::string imgprefix = "letters";
std::string alignfname;
std::string gammafname;

void alignCam() {
	ss << topfolder << calibfolder << "align_16.png";
	std::string alignName = ss.str(); ss.str("");
	ss << topfolder << calibfolder << "align_16_cam.png";
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
void calibCam() {
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
		ss << topfolder << calibfolder << "calib_"
			<< std::setprecision(3) << std::fixed << i << ".png";
		std::string camfname = ss.str(); ss.str("");
		takeAPicture(camfname.c_str());
		sf::Image cam_i; cam_i.loadFromFile(camfname);
		Image cam; cam.fromUchar(cam_i.getSize().x, cam_i.getSize().y, 4, 0x7, cam_i.getPixelsPtr());

		// Crop the image and save over captured image
		cam = alignCaptured(cam, w / 2, h / 2, alignfname);
		saveImage(cam, camfname);

		// Save the average intensity value in the map
		camToDisp[cam.avg()] = i;

		// Make sure 255 is also captured
		if (ii < 255 && ii + 8 >= 256)
			ii = 255 - 8;
	}

	disconFromFirstCamera();

	// Write camera gamma curve to file
	FILE* fp = fopen(gammafname.c_str(), "wb");
	if (!fp) exit(-1);
	int size = camToDisp.size();
	fwrite(&size, sizeof(int), 1, fp);
	for (auto ii = camToDisp.begin(); ii != camToDisp.end(); ii++) {
		double k = ii->first, v = ii->second;
		fwrite(&k, sizeof(double), 1, fp);
		fwrite(&v, sizeof(double), 1, fp);
	}
	fclose(fp);
}

int main(void) {
	ss << "camera_2.0m_f22_" << std::setprecision(2) << std::fixed << dpt_cam << "d_0/";
	topfolder = ss.str(); ss.str("");
	ss << topfolder << "camalign";
	alignfname = ss.str(); ss.str("");
	ss << topfolder << "camgamma";
	gammafname = ss.str(); ss.str("");

	bool loadF = true;				// Whether to load the image to test
	bool searchPSF = false;			// Whether to search the PSF space (else use predefined params)
	bool saveAllTests = false;		// Whether to save all PSF tests (careful with this one!)
	bool loadprec = false;			// Whether to load the precorrections
	bool usegrad = true;			// Whether to use the gradient in PSF testing
	bool align = false;				// Align the camera (don't do anything else)
	bool calib = false;				// Calibrate the camera (don't do anything else)

	// Align the camera
	if (align) {
		alignCam();
		loop();
		return 0;
	}
	// Calibrate the camera intensity
	if (calib) {
		calibCam();
		return 0;
	}

	// Load or generate the test image
	Image F;
	ss << topfolder << imgprefix << ".png";
	if (!loadF) {
		int w = 1280, h = 1024;
		Image X((w / 4) - 1, h, 0.0);
		X = X.resize((w / 4), h, 1.0)
			.resize((3 * w / 4) - 1, h, 0.0)
			.resize((3 * w / 4), h, 1.0)
			.resize(w, h, 0.0);
		Image Y(w, (h / 4) - 1, 0.0);
		Y = Y.resize(w, (h / 4), 1.0)
			.resize(w, (3 * h / 4) - 1, 0.0)
			.resize(w, (3 * h / 4), 1.0)
			.resize(w, h, 0.0);
		F = std::move(X);
		F += Y;
		F = F.clip();

		saveImage(F, ss.str()); ss.str("");
	} else {
		sf::Image img; img.loadFromFile(ss.str()); ss.str("");
		F.fromUchar(img.getSize().x, img.getSize().y, 4, 0x7, img.getPixelsPtr());
	}

	// Capture the test image
	connectToFirstCamera();
	showFullscreenImage(F);
	sf::sleep(sf::seconds(1.0f));
	ss << topfolder << imgprefix << "_cam.png";
	takeAPicture(ss.str().c_str());
	disconFromFirstCamera();

	// Load, then transform and crop the captured image
	sf::Image img; img.loadFromFile(ss.str()); ss.str("");
	Image Fcap; Fcap.fromUchar(img.getSize().x, img.getSize().y, 4, 0x7, img.getPixelsPtr());
	Fcap = alignCaptured(Fcap, F.getWidth(), F.getHeight(), alignfname);
	ss << topfolder << imgprefix << "_cam.png";
	saveImage(Fcap, ss.str()); ss.str("");

	// Gamma-correct captured image
	Fcap = correctGamma(Fcap, gammafname);
	ss << topfolder << imgprefix << "_cam_gam.png";
	saveImage(Fcap, ss.str()); ss.str("");


	/////////////////////////////////// SEARCH PSFS ///////////////////////////////////

	double dpt_best = 2.0;
	double ap_best = 5.4;
	double sf_best = 0.7;
	double mse = 0.0;
	if (searchPSF) {
		// Create a priority queue to hold the smallest MSE
		std::priority_queue<PSFParm, std::vector<PSFParm>, PSFCmp> pqueue;

		std::cout << "Diopters\tAperture\tScale\t\tMSE" << std::endl;
		double dpt_min = 1.2;	double ap_min = 5.2;		double sf_min = 0.5;
		double dpt_max = 3.0;	double ap_max = 7.0;		double sf_max = 1.4;
		double dpt_step = 0.2;	double ap_step = 0.2;		double sf_step = 0.1;
		for (int i_dpt = 0; i_dpt < (int)((dpt_max + dpt_step - dpt_min) / dpt_step + 0.5); i_dpt++) {
			double dpt_psf = dpt_min + i_dpt * dpt_step;
			for (int i_ap = 0; i_ap < (int)((ap_max + ap_step - ap_min) / ap_step + 0.5); i_ap++) {
				double ap_psf = ap_min + i_ap * ap_step;
				for (int i_sf = 0; i_sf < (int)((sf_max + sf_step - sf_min) / sf_step + 0.5); i_sf++) {
					double sf_psf = sf_min + i_sf * sf_step;

					ss << std::setprecision(2) << std::fixed << dpt_psf << "d_"
						<< std::setprecision(4) << std::fixed << ap_psf << "ap_"
						<< std::setprecision(2) << std::fixed << sf_psf << "sf";
					std::string params = ss.str(); ss.str("");

					// Load the PSF
					ss << psffolder << "psf_" << params << ".dbl";
					Image PSF; PSF.fromBinary(ss.str().c_str()); ss.str("");

					// Convolve the original image
					Image KF = convolve(F, PSF, BC_PERIODIC);
					if (saveAllTests) {
						ss << topfolder << imgprefix << "_" << params << "_conv.png";
						saveImage(KF, ss.str()); ss.str("");
					}

					// Calculate MSE and output
					double KFMSE;
					if (usegrad) {
						Image grad = ((KF.diffX(BC_PERIODIC) ^ 2) + (KF.diffY(BC_PERIODIC) ^ 2)).sqrt();
						KFMSE = (((KF - Fcap) ^ 2) * grad.scale()).sum() / (KF.getArraySize());
					} else {
						KFMSE = ((KF - Fcap) ^ 2).sum() / (KF.getArraySize());
					}
					std::cout << std::setprecision(2) << std::fixed << dpt_psf << "d\t\t"
						<< std::setprecision(4) << std::fixed << ap_psf << "ap\t"
						<< std::setprecision(2) << std::fixed << sf_psf << "sf\t\t"
						<< std::setprecision(8) << std::fixed << KFMSE << std::endl;

					// Create a PSFParm and add it to the priority queue
					PSFParm psfparm(dpt_psf, ap_psf, sf_psf, KFMSE);
					pqueue.push(psfparm);
				}
			}
		}

		// Get parameters for smallest MSE value
		const PSFParm& psfparm = pqueue.top();
		dpt_best = psfparm.dpt;
		ap_best = psfparm.ap;
		sf_best = psfparm.sf;
		mse = psfparm.mse;
	}

	// Get PSF parameters into a string
	ss << std::setprecision(2) << std::fixed << dpt_best << "d_"
		<< std::setprecision(4) << std::fixed << ap_best << "ap_"
		<< std::setprecision(2) << std::fixed << sf_best << "sf";
	std::string params = ss.str(); ss.str("");

	// Load the PSF
	ss << psffolder << "psf_" << params << ".dbl";
	Image PSF; PSF.fromBinary(ss.str().c_str()); ss.str("");

	// Convolve the original image
	Image KF = convolve(F, PSF, BC_PERIODIC);
	if (!saveAllTests || !searchPSF) {
		ss << topfolder << imgprefix << "_" << params << "_conv.png";
		saveImage(KF, ss.str()); ss.str("");
	}

	// Output MSE
	std::cout << std::endl << "Best:" << std::endl
		<< std::setprecision(2) << std::fixed << dpt_best << "d\t\t"
		<< std::setprecision(4) << std::fixed << ap_best << "ap\t"
		<< std::setprecision(2) << std::fixed << sf_best << "sf\t\t"
		<< std::setprecision(8) << std::fixed << mse << std::endl;



	////////////////////////////// PRECORRECT //////////////////////////////

	Image KtF_L1, KtF_L2;
	ss << topfolder << imgprefix << "_" << params << "_prec_L1.png";
	std::string KtF_L1_str = ss.str(); ss.str("");
	ss << topfolder << imgprefix << "_" << params << "_prec_L1_conv.png";
	std::string KtF_L1_conv_str = ss.str(); ss.str("");
	ss << topfolder << imgprefix << "_" << params << "_prec_L2.png";
	std::string KtF_L2_str = ss.str(); ss.str("");
	ss << topfolder << imgprefix << "_" << params << "_prec_L2_conv.png";
	std::string KtF_L2_conv_str = ss.str(); ss.str("");

	if (!loadprec) {
		// Precorrect the test image
		Options opts; opts.tv = TVL1;
		KtF_L1 = precorrect(F, PSF, 9.3e4, opts);
		saveImage(KtF_L1, KtF_L1_str);

		opts.tv = TVL2;
		KtF_L2 = precorrect(F, PSF, 9.3e4, opts);
		saveImage(KtF_L2, KtF_L2_str);
	} else {
		img.loadFromFile(KtF_L1_str);
		KtF_L1.fromUchar(img.getSize().x, img.getSize().y, 4, 0x7, img.getPixelsPtr());

		img.loadFromFile(KtF_L2_str);
		KtF_L2.fromUchar(img.getSize().x, img.getSize().y, 4, 0x7, img.getPixelsPtr());
	}

	// Convolve precorrections
	Image KKtF_L1 = convolve(KtF_L1, PSF, BC_PERIODIC);
	saveImage(KKtF_L1, KtF_L1_conv_str);

	Image KKtF_L2 = convolve(KtF_L2, PSF, BC_PERIODIC);
	saveImage(KKtF_L2, KtF_L2_conv_str);


	///////////////////////////////////// CAPTURE PRECORRECTIONS /////////////////////

	ss << topfolder << imgprefix << "_" << params << "_prec_L1_cam.png";
	std::string KtF_L1_cam_str = ss.str(); ss.str("");
	ss << topfolder << imgprefix << "_" << params << "_prec_L1_cam_gam.png";
	std::string KtF_L1_cam_gam_str = ss.str(); ss.str("");
	ss << topfolder << imgprefix << "_" << params << "_prec_L2_cam.png";
	std::string KtF_L2_cam_str = ss.str(); ss.str("");
	ss << topfolder << imgprefix << "_" << params << "_prec_L2_cam_gam.png";
	std::string KtF_L2_cam_gam_str = ss.str(); ss.str("");

	// Display precorrected images and capture
	connectToFirstCamera();
	showFullscreenImage(KtF_L1);
	sf::sleep(sf::seconds(1.0f));
	takeAPicture(KtF_L1_cam_str.c_str());

	showFullscreenImage(KtF_L2);
	sf::sleep(sf::seconds(1.0f));
	takeAPicture(KtF_L2_cam_str.c_str());
	disconFromFirstCamera();

	// Load, align, and gamma-correct captured precorrections
	img.loadFromFile(KtF_L1_cam_str);
	Image KtF_L1_cam; KtF_L1_cam.fromUchar(img.getSize().x, img.getSize().y, 4, 0x7, img.getPixelsPtr());
	KtF_L1_cam = alignCaptured(KtF_L1_cam, F.getWidth(), F.getHeight(), alignfname);
	saveImage(KtF_L1_cam, KtF_L1_cam_str);
	KtF_L1_cam = correctGamma(KtF_L1_cam, gammafname);
	saveImage(KtF_L1_cam, KtF_L1_cam_gam_str);

	img.loadFromFile(KtF_L2_cam_str);
	Image KtF_L2_cam; KtF_L2_cam.fromUchar(img.getSize().x, img.getSize().y, 4, 0x7, img.getPixelsPtr());
	KtF_L2_cam = alignCaptured(KtF_L2_cam, F.getWidth(), F.getHeight(), alignfname);
	saveImage(KtF_L2_cam, KtF_L2_cam_str);
	KtF_L2_cam = correctGamma(KtF_L2_cam, gammafname);
	saveImage(KtF_L2_cam, KtF_L2_cam_gam_str);


	std::cout << "Done!" << std::endl;

	loop();

	return 0;
}