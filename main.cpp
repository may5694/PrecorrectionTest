#include <iostream>
#include <iomanip>
#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc/imgproc.hpp>
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
void writeDbl(Mat m, std::string fname) {
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

void circshift(Mat in, Mat& out, int dx, int dy) {
	// Anchor coordinates
	int ax = ((dx % in.cols) + in.cols) % in.cols;
	int ay = ((dy % in.rows) + in.rows) % in.rows;

	// Segment input, copy to output in flipped positions
	Mat output = Mat::zeros(in.size(), in.type());
	in(Rect(0, 0, in.cols - ax, in.rows - ay)).copyTo(output(Rect(ax, ay, in.cols - ax, in.rows - ay)));
	in(Rect(in.cols - ax, 0, ax, in.rows - ay)).copyTo(output(Rect(0, ay, ax, in.rows - ay)));
	in(Rect(0, in.rows - ay, in.cols - ax, ay)).copyTo(output(Rect(ax, 0, in.cols - ax, ay)));
	in(Rect(in.cols - ax, in.rows - ay, ax, ay)).copyTo(output(Rect(0, 0, ax, ay)));
	out = output;
}
void dctshift(Mat in, Mat& out, int cx, int cy) {
	// k = number of pixels between edge and center (c) pixel (e.g. [ooooxoooo] k = 4 pixels, c = pixel id 4)
	int k = std::min(std::min(cx, in.cols - cx - 1), std::min(cy, in.rows - cy - 1));

	// Create a copy of the input PSF, enforcing a square with odd sidelength
	Mat PP; in(Range(cy - k, cy + k + 1), Range(cx - k, cx + k + 1)).copyTo(PP);

	// Create two diagonal matrices used to shift the PSF
	Mat Z1 = Mat::zeros(PP.size(), PP.type()); Z1.diag(k) = Scalar(1.0);
	Mat Z2 = Mat::zeros(PP.size(), PP.type()); Z2.diag(k + 1) = Scalar(1.0);

	// Shift the PSF and add each quadrant together
	PP = Z1 * PP * Z1.t() + Z1 * PP * Z2.t() + Z2 * PP * Z1.t() + Z2 * PP * Z2.t();

	// Store the resulting shifted PSF into the output matrix
	out = Mat::zeros(in.size(), in.type());
	PP.copyTo(out(Range(0, 2 * k + 1), Range(0, 2 * k + 1)));
}

void psf2otf(Mat psf, Mat& otf, int w, int h) {
	// Create the otf image
	otf = Mat::zeros(h, w, psf.type());

	// Copy in the psf and shift the center to the origin
	psf.copyTo(otf(Rect(0, 0, psf.cols, psf.rows)));
	circshift(otf, otf, -((psf.cols - 1) / 2), -((psf.rows - 1) / 2));

	dft(otf, otf);
}
void psf2otf_rflx(Mat psf, Mat& otf, int w, int h) {
	// Copy the psf and dctshift it
	Mat copy = psf.clone();
	dctshift(copy, copy, psf.cols / 2, psf.rows / 2);

	// Create the otf image
	otf = Mat::zeros(h, w, psf.type());
	copy.copyTo(otf(Rect(0, 0, psf.cols, psf.rows)));

	// Create an image to scale dct result
	Mat e = Mat::zeros(h, w, psf.type());
	e.at<double>(0, 0) = 1.0;
	dct(e, e);

	// Perform discrete cosine transform
	dct(otf, otf);
	otf /= e;		// Scale the result
}

void convolve(Mat F, Mat psf, Mat& out) {
	Mat otf; psf2otf(psf, otf, F.cols, F.rows);
	Mat Ff; dft(F, Ff);

	mulSpectrums(Ff, otf, Ff, 0, false);

	dft(Ff, out, DFT_INVERSE | DFT_SCALE);
}
void convolve_rflx(Mat F, Mat psf, Mat& out) {
	Mat otf; psf2otf_rflx(psf, otf, F.cols, F.rows);
	Mat Ff; dct(F, Ff);

	Ff = Ff.mul(otf);

	dct(Ff, out, DCT_INVERSE);
}
void correlate(Mat F, Mat psf, Mat& out) {
	Mat otf; psf2otf(psf, otf, F.cols, F.rows);
	Mat Ff; dft(F, Ff);

	mulSpectrums(Ff, otf, Ff, 0, true);

	dft(Ff, out, DFT_INVERSE | DFT_SCALE);
}
void correlate_rflx(Mat F, Mat psf, Mat& out) {
	Mat otf; psf2otf_rflx(psf, otf, F.cols, F.rows);
	Mat Ff; dct(F, Ff);

	Ff = Ff.mul(otf);

	dct(Ff, out, DCT_INVERSE);
}

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

	correlate_rflx(cvGreen, cvPsf, cvGreen);

	// Read green channel into Image
	Image imGreen;
	{ Image imRed, imBlue;
	readColorImage(imRed, imGreen, imBlue, imgFname); }

	// Convolve image
	imGreen = correlate(imGreen, imPsf, BC_REFLEXIVE);

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