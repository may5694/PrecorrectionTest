#include <iostream>
#include <iomanip>
#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>
#include "precorrection.h"
#include "common.h"
#include "camcalib.h"

using namespace cv;

void readDbl(Mat& m, std::string fname) {
	// Open the file to read from
	FILE* fp = fopen(fname.c_str(), "rb");
	if (!fp) throw (-1);

	// Read in dimensions and image content
	int width, height;
	fread(&width, sizeof(width), 1, fp);
	fread(&height, sizeof(height), 1, fp);
	std::vector<double> data;
	data.resize(width * height);
	int numRead = fread(data.data(), sizeof(double), data.size(), fp);

	fclose(fp);

	// Construct a matrix for the read data, then copy it to assume ownership
	Mat read(height, width, CV_64FC1, data.data());
	read.copyTo(m);
}
void writeDbl(const Mat& m, std::string fname) {
	// Open the file to write to
	FILE* fp = fopen(fname.c_str(), "wb");
	if (!fp) throw (-1);

	// Write out dimensions
	fwrite(&m.cols, sizeof(int), 1, fp);
	fwrite(&m.rows, sizeof(int), 1, fp);

	// Write out image content
	if (m.isContinuous()) {
		fwrite(m.data, sizeof(double), m.cols * m.rows, fp);
	} else {
		for (auto it = m.begin<double>(); it != m.end<double>(); it++) {
			double val = *it;
			fwrite(&val, sizeof(double), 1, fp);
		}
	}

	fclose(fp);
}

int main() {
	std::string psffolder = "psf/";
	std::string path = "img/";
	std::string imgstr = "letters";
	ss << path << imgstr << ".png";
	std::string imgFname = ss.str(); ss.str("");
	ss << path << imgstr << ".dbl";
	std::string imgDbl = ss.str(); ss.str("");

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

	// Save the image to DBL format
	writeDbl(cvGreen, imgDbl);

	// Load the saved DBL
	Mat loadDbl; readDbl(loadDbl, imgDbl);

	Mat delta = abs(cvGreen - loadDbl);
	double max;
	minMaxIdx(delta, NULL, &max);
	std::cout << "maximum difference: " << max << std::endl;
	delta /= max;

	namedWindow("wnd1", CV_WINDOW_AUTOSIZE);
	imshow("wnd1", cvGreen);
	namedWindow("wnd2", CV_WINDOW_AUTOSIZE);
	imshow("wnd2", loadDbl);
	namedWindow("wnd3", CV_WINDOW_AUTOSIZE);
	imshow("wnd3", delta);
	waitKey(0);

	return 0;
}