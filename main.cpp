#include <iostream>
#include <iomanip>
#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include "cvprecorrection.h"
#include "precorrection.h"
#include "common.h"
#include "camcalib.h"

using namespace cv;
using namespace prec;

int main() {
	std::string psffolder = "psf/";
	std::string path = "img/";
	std::string imgstr = "apples";
	ss << path << imgstr << ".png";
	std::string imgFname = ss.str(); ss.str("");


	PSFParm ppm(2.0, 6.0, 0.5, 0.0);
	ss << psffolder << "psf_" << ppm.paramStr << ".dbl";
	std::string psfName = ss.str(); ss.str("");

	Mat cvPsf; readDbl(cvPsf, psfName);
	Image imPsf; imPsf.fromBinary(psfName);

	// Load image with OpenCV and convert to double
	Mat cvImage = imread(imgFname, CV_LOAD_IMAGE_COLOR);
	if (!cvImage.data) {
		std::cout << "Could not read " << ss.str() << "!" << std::endl;
		return -1;
	}
	cvImage.convertTo(cvImage, CV_64FC3, 1.0 / 255.0);

	// Extract the green channel
	Mat cvGreen(cvImage.rows, cvImage.cols, CV_64FC1);
	int fromTo [] = { 1, 0 };
	mixChannels(&cvImage, 1, &cvGreen, 1, fromTo, 1);
	cvImage.release();

	convolve(cvGreen, cvPsf, cvGreen, Reflexive);

	// Read green channel into Image
	Image imGreen;
	{ Image imRed, imBlue;
	readColorImage(imRed, imGreen, imBlue, imgFname); }

	// Convolve image
	imGreen = convolve(imGreen, imPsf, BC_PERIODIC);

	// Load Image result into a Mat
	Mat cvImGreen(imGreen.getWidth(), imGreen.getHeight(), CV_64FC1, imGreen.data());


	Mat delta = abs(cvGreen - cvImGreen);
	double max;
	minMaxIdx(delta, NULL, &max);
	std::cout << "maximum difference: " << max << std::endl;
	delta /= max;

	namedWindow("wnd1", CV_WINDOW_AUTOSIZE);
	imshow("wnd1", cvGreen);
	namedWindow("wnd2", CV_WINDOW_AUTOSIZE);
	imshow("wnd2", cvImGreen);
	namedWindow("wnd3", CV_WINDOW_AUTOSIZE);
	imshow("wnd3", delta);
	waitKey(0);

	return 0;
}