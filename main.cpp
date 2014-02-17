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

int main(void) {
	double dpt_cam = 2.00;
	std::stringstream ss;
	std::string topfolder = "camera_2.0m_f22_7/";
	ss << std::setprecision(2) << std::fixed << dpt_cam << "d/";
	std::string dptfolder = ss.str(); ss.str("");
	std::string psffolder = "psf/";
	std::string imgprefix = "lenna";
	bool loadF = true;
	bool searchPSF = true;
	bool loadprec = false;
	bool equalize = true;
	bool usegrad = true;

	Image F;
	ss << topfolder << dptfolder << imgprefix << ".png";
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
	ss << topfolder << dptfolder << imgprefix << "_cam.jpg";
	takeAPicture(ss.str().c_str());
	disconFromFirstCamera();

	// Load, then transform and crop the captured image
	sf::Image img; img.loadFromFile(ss.str()); ss.str("");
	Image Fcap; Fcap.fromUchar(img.getSize().x, img.getSize().y, 4, 0x7, img.getPixelsPtr());
	ss << topfolder << dptfolder << "camxform";
	Fcap = xformCaptured(Fcap, F.getWidth(), F.getHeight(), ss.str()); ss.str("");
	ss << topfolder << dptfolder << imgprefix << "_cam.jpg";
	saveImage(Fcap, ss.str()); ss.str("");



	/////////////////////////////////// SEARCH PSFS ///////////////////////////////////

	double dpt_best = 1.2;
	double ap_best = 5.4;
	double sf_best = 1.0;
	double mse = 0.000142548;
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

					// Load the PSF
					ss << psffolder << "psf_"
						<< std::setprecision(2) << std::fixed << dpt_psf << "d_"
						<< std::setprecision(4) << std::fixed << ap_psf << "ap_"
						<< std::setprecision(2) << std::fixed << sf_psf << "sf.dbl";
					Image PSF; PSF.fromBinary(ss.str().c_str()); ss.str("");

					// Convolve the original image
					Image KF = convolve(F, PSF, BC_PERIODIC);

					Image Fcap_e = Fcap;
					if (equalize)
						// Match captured image's histogram to convolved's histogram
						Fcap_e = Fcap.match(KF);

					// Calculate MSE and output
					double KFMSE;
					if (usegrad) {
						Image grad = ((KF.diffX(BC_PERIODIC) ^ 2) + (KF.diffY(BC_PERIODIC) ^ 2)).sqrt();
						KFMSE = (((KF - Fcap_e) ^ 2) * grad.scale()).sum() / (KF.getArraySize());
					} else {
						KFMSE = ((KF - Fcap_e) ^ 2).sum() / (KF.getArraySize());
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

	// Load the PSF
	ss << psffolder << "psf_"
		<< std::setprecision(2) << std::fixed << dpt_best << "d_"
		<< std::setprecision(4) << std::fixed << ap_best << "ap_"
		<< std::setprecision(2) << std::fixed << sf_best << "sf.dbl";
	Image PSF; PSF.fromBinary(ss.str().c_str()); ss.str("");

	// Convolve the original image and save it
	Image KF = convolve(F, PSF, BC_PERIODIC);
	ss << topfolder << dptfolder << imgprefix << "_conv_"
		<< std::setprecision(2) << std::fixed << dpt_best << "d_"
		<< std::setprecision(4) << std::fixed << ap_best << "ap_"
		<< std::setprecision(2) << std::fixed << sf_best << "sf.png";
	saveImage(KF, ss.str()); ss.str("");

	Image Fcap_e = Fcap;
	if (equalize) {
		// Equalize captured image's histogram for comparison
		Image Fcap_e = Fcap.match(KF);
		ss << topfolder << dptfolder << imgprefix << "_cam_equ_"
			<< std::setprecision(2) << std::fixed << dpt_best << "d_"
			<< std::setprecision(4) << std::fixed << ap_best << "ap_"
			<< std::setprecision(2) << std::fixed << sf_best << "sf.png";
		saveImage(Fcap_e, ss.str()); ss.str("");
	}

	// Output MSE
	std::cout << std::endl << "Best:" << std::endl
		<< std::setprecision(2) << std::fixed << dpt_best << "d\t\t"
		<< std::setprecision(4) << std::fixed << ap_best << "ap\t"
		<< std::setprecision(2) << std::fixed << sf_best << "sf\t\t"
		<< std::setprecision(8) << std::fixed << mse << std::endl;



	////////////////////////////// PRECORRECT //////////////////////////////

	Image KtF_L1, KtF_L2;
	if (!loadprec) {
		// Precorrect the test image
		Options opts; opts.tv = TVL1;
		KtF_L1 = precorrect(F, PSF, 9.3e4, opts);
		ss << topfolder << dptfolder << imgprefix << "_prec_L1.png";
		saveImage(KtF_L1, ss.str()); ss.str("");

		opts.tv = TVL2;
		KtF_L2 = precorrect(F, PSF, 9.3e4, opts);
		ss << topfolder << dptfolder << imgprefix << "_prec_L2.png";
		saveImage(KtF_L2, ss.str()); ss.str("");
	} else {
		ss << topfolder << dptfolder << imgprefix << "_prec_L1.png";
		img.loadFromFile(ss.str()); ss.str("");
		KtF_L1.fromUchar(img.getSize().x, img.getSize().y, 4, 0x7, img.getPixelsPtr());

		ss << topfolder << dptfolder << imgprefix << "_prec_L2.png";
		img.loadFromFile(ss.str()); ss.str("");
		KtF_L2.fromUchar(img.getSize().x, img.getSize().y, 4, 0x7, img.getPixelsPtr());
	}

	// Convolve precorrections
	Image KKtF_L1 = convolve(KtF_L1, PSF, BC_PERIODIC);
	ss << topfolder << dptfolder << imgprefix << "_prec_L1_conv.png";
	saveImage(KKtF_L1, ss.str()); ss.str("");

	Image KKtF_L2 = convolve(KtF_L2, PSF, BC_PERIODIC);
	ss << topfolder << dptfolder << imgprefix << "_prec_L2_conv.png";
	saveImage(KKtF_L2, ss.str()); ss.str("");


	///////////////////////////////////// CAPTURE PRECORRECTIONS /////////////////////

	// Display precorrected images and capture
	connectToFirstCamera();
	showFullscreenImage(KtF_L1);
	sf::sleep(sf::seconds(1.0f));
	ss << topfolder << dptfolder << imgprefix << "_prec_L1_cam.jpg";
	takeAPicture(ss.str().c_str()); ss.str("");

	showFullscreenImage(KtF_L2);
	sf::sleep(sf::seconds(1.0f));
	ss << topfolder << dptfolder << imgprefix << "_prec_L2_cam.jpg";
	takeAPicture(ss.str().c_str()); ss.str("");
	disconFromFirstCamera();

	// Load, then transform and crop captured precorrections
	ss << topfolder << dptfolder << imgprefix << "_prec_L1_cam.jpg";
	img.loadFromFile(ss.str()); ss.str("");
	Image KtF_L1_cam; KtF_L1_cam.fromUchar(img.getSize().x, img.getSize().y, 4, 0x7, img.getPixelsPtr());
	ss << topfolder << dptfolder << "camxform";
	KtF_L1_cam = xformCaptured(KtF_L1_cam, F.getWidth(), F.getHeight(), ss.str()); ss.str("");
	ss << topfolder << dptfolder << imgprefix << "_prec_L1_cam.jpg";
	saveImage(KtF_L1_cam, ss.str()); ss.str("");

	ss << topfolder << dptfolder << imgprefix << "_prec_L2_cam.jpg";
	img.loadFromFile(ss.str()); ss.str("");
	Image KtF_L2_cam; KtF_L2_cam.fromUchar(img.getSize().x, img.getSize().y, 4, 0x7, img.getPixelsPtr());
	ss << topfolder << dptfolder << "camxform";
	KtF_L2_cam = xformCaptured(KtF_L2_cam, F.getWidth(), F.getHeight(), ss.str()); ss.str("");
	ss << topfolder << dptfolder << imgprefix << "_prec_L2_cam.jpg";
	saveImage(KtF_L2_cam, ss.str()); ss.str("");


	if (equalize) {
		// Equalize captured precorrections
		Image KtF_L1_cam_e = KtF_L1_cam.match(KKtF_L1);
		ss << topfolder << dptfolder << imgprefix << "_prec_L1_cam_equ.png";
		saveImage(KtF_L1_cam_e, ss.str()); ss.str("");
		Image KtF_L2_cam_e = KtF_L2_cam.match(KKtF_L2);
		ss << topfolder << dptfolder << imgprefix << "_prec_L2_cam_equ.png";
		saveImage(KtF_L2_cam_e, ss.str()); ss.str("");
	}

	std::cout << "Done!" << std::endl;

	loop();

	return 0;
}
int main_(void) {
	std::string prefix = "lennaColor512x512";
	std::string diopt = "-2.0";
	std::stringstream ss;

	// Load the original image
	sf::Image sfImg;
	ss << prefix << ".png";
	sfImg.loadFromFile(ss.str()); ss.str("");
	Image Fr, Fg, Fb;
	Fr.fromUchar(sfImg.getSize().x, sfImg.getSize().y, 4, 0x1, sfImg.getPixelsPtr());
	Fg.fromUchar(sfImg.getSize().x, sfImg.getSize().y, 4, 0x2, sfImg.getPixelsPtr());
	Fb.fromUchar(sfImg.getSize().x, sfImg.getSize().y, 4, 0x4, sfImg.getPixelsPtr());

	int fw = Fr.getWidth();
	int fh = Fr.getHeight();

	// Load the PSF
	Image H;
	ss << "psf_" << diopt << ".dbl";
	H.fromBinary(ss.str().c_str()); ss.str("");

	// Deblur image
	//*
	sf::Clock c;
	std::cout << "Red:" << std::endl;
	Image KtFr = precorrect(Fr, H, 1e4);
	std::cout << std::endl << "Green:" << std::endl;
	Image KtFg = precorrect(Fg, H, 1e4);
	std::cout << std::endl << "Blue:" << std::endl;
	Image KtFb = precorrect(Fb, H, 1e4);
	std::cout << std::endl;
	sf::Time elapsed = c.getElapsedTime();
	std::cout << std::endl << "Elapsed time: " << elapsed.asSeconds() << " seconds" << std::endl;
	ss << prefix << "_r_prec_" << diopt << ".dbl";
	KtFr.toBinary(ss.str().c_str()); ss.str("");
	ss << prefix << "_g_prec_" << diopt << ".dbl";
	KtFg.toBinary(ss.str().c_str()); ss.str("");
	ss << prefix << "_b_prec_" << diopt << ".dbl";
	KtFb.toBinary(ss.str().c_str()); ss.str("");/**/
	/*
	Image KtFr, KtFg, KtFb;
	ss << prefix << "_r_prec_" << diopt << ".dbl";
	KtFr.fromBinary(ss.str().c_str()); ss.str("");
	ss << prefix << "_g_prec_" << diopt << ".dbl";
	KtFg.fromBinary(ss.str().c_str()); ss.str("");
	ss << prefix << "_b_prec_" << diopt << ".dbl";
	KtFb.fromBinary(ss.str().c_str()); ss.str("");/**/

	ss << prefix << "_prec_" << diopt << ".png";
	saveColorImage(KtFr, KtFg, KtFb, ss.str()); ss.str("");

	// Convolve precorrected image
	Image KKtFr = convolve(KtFr, H);
	Image KKtFg = convolve(KtFg, H);
	Image KKtFb = convolve(KtFb, H);
	ss << prefix << "_prec_conv_" << diopt << ".png";
	saveColorImage(KKtFr, KKtFg, KKtFb, ss.str()); ss.str("");

	// Convolve original image
	Image KFr = convolve(Fr, H);
	Image KFg = convolve(Fg, H);
	Image KFb = convolve(Fb, H);
	ss << prefix << "_conv_" << diopt << ".png";
	saveColorImage(KFr, KFg, KFb, ss.str()); ss.str("");

	ImageWindow* w;
	showColorImage(Fr, Fg, Fb, -fw / 2 - 14, -fh / 2 - 24 - 30, "F");
	showColorImage(KtFr, KtFg, KtFb, -fw / 2 - 14, fh / 2 + 24 - 30, "K\'F");
	showColorImage(KFr, KFg, KFb, fw / 2 + 14, -fh / 2 - 24 - 30, "KF");
	showColorImage(KKtFr, KKtFg, KKtFb, fw / 2 + 14, fh / 2 + 24 - 30, "KK\'F");

	loop();

	return 0;
}