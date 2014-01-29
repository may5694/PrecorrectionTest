#include <list>
#include <iostream>
#include "precorrection.h"
#include "JNB.h"
#include "imagewindow.h"
#include "canoninterface.h"

std::list<ImageWindow> windowList;
std::vector<Image> imageVec;
std::vector<std::string> titleVec;
int rows = 1, cols = 1, imw = 0, imh = 0;

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
	windowList.push_back(std::move(w));
	return &windowList.back();

	delete [] buf; buf = NULL;
}
ImageWindow* showFullscreenImage(const Image& image) {
	ImageWindow w(image, false);
	w.position(1680, 0);
	w.resize(1280, 1024);
	w.setColor(0, 0, 0);
	windowList.push_back(std::move(w));
	return &windowList.back();
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

int main(void) {
	// Load the original image
	sf::Image sfImg;
	sfImg.loadFromFile("lenna512x512.png");
	Image F;
	F.fromUchar(sfImg.getSize().x, sfImg.getSize().y, 4, 0x1, sfImg.getPixelsPtr());

	int fw = F.getWidth();
	int fh = F.getHeight();

	// Load the PSF
	Image PSF;
	PSF.fromBinary("psf_1.5_d10_p4.dbl");

	// Precorrect
	Image KtF = precorrect(F, PSF, 9.3e4);

	connectToFirstCamera();

	ImageWindow* w = showFullscreenImage(KtF);
	w->update();

	sf::sleep(sf::seconds(1.0));
	takeAPicture("camera_lenna_prec_blur_0.5_40.jpg");

	disconFromFirstCamera();

	// Convolve image
	Image KKtF = convolve(KtF, PSF, BC_PERIODIC);
	showImage(KKtF, 0, 0, "KKtF");

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