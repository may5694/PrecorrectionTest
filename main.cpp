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
//using namespace prec;

int main() {
	std::string psffolder = "psf/";
	std::string path = "img/";
	std::string imgstr = "apples";
	ss << path << imgstr << ".png";
	std::string imgFname = ss.str(); ss.str("");


	PSFParm ppm(2.0, 6.0, 1.0, 0.0);
	ss << psffolder << "psf_" << ppm.paramStr << ".dbl";
	std::string psfName = ss.str(); ss.str("");

	Mat cvPsf; prec::readDbl(cvPsf, psfName);
	Image imPsf; imPsf.fromBinary(psfName);

	// Load image with OpenCV and convert to double
	Mat cvImage = imread(imgFname, CV_LOAD_IMAGE_COLOR);
	if (!cvImage.data) {
		std::cout << "Could not read " << ss.str() << "!" << std::endl;
		return -1;
	}
	cvImage.convertTo(cvImage, CV_64FC3, 1.0 / 255.0);

	vector<Mat> channels;
	split(cvImage, channels);

	// Read green channel into Image
	Image imGreen; { Image imRed, imBlue;
	readColorImage(imRed, imGreen, imBlue, imgFname); }

	prec::Options opts; opts.relchg = DBL_EPSILON; opts.maxiter = 20; opts.bc = prec::Reflexive; opts.tv = prec::TVL2;
	opts.saveHist = true;
	Mat cvPrec;
	cvPrec = prec::precorrect(cvImage, cvPsf, 9.3e4, opts);

	for (auto ch = opts.history.begin(); ch != opts.history.end(); ++ch) {
		for (auto it = ch->begin(); it != ch->end(); ++it) {
			Mat iter = it->X;
			ss << "cvtest/iter/" << ch - opts.history.begin() << "_" << it->iter << ".png";
			Mat itersave; iter.convertTo(itersave, CV_8U, 255.0);
			imwrite(ss.str(), itersave); ss.str("");
		}
	}

	/*Options imOpts; imOpts.relchg = DBL_EPSILON; imOpts.maxiter = 20; imOpts.bc = BC_PERIODIC; imOpts.tv = TVL1;
	Image imPrec = precorrect(imGreen, imPsf, 9.3e4, imOpts);

	Mat cvImPrec(imPrec.getHeight(), imPrec.getWidth(), CV_64FC1, imPrec.data());*/

	/*Mat delta = abs(cvPrec - cvImPrec);
	double max; minMaxIdx(delta, NULL, &max);
	std::cout << "maximum difference: " << max << std::endl;
	delta /= max;*/

	Mat cvDisp1 = cvPrec;
	namedWindow("OpenCV 1", CV_WINDOW_AUTOSIZE);
	imshow("OpenCV 1", cvDisp1);
	/*Mat cvDisp2 = cvImPrec;
	namedWindow("OpenCV 2", CV_WINDOW_AUTOSIZE);
	imshow("OpenCV 2", cvDisp2);*/
	/*Mat cvDisp3 = delta;
	namedWindow("OpenCV 3", CV_WINDOW_AUTOSIZE);
	imshow("OpenCV 3", cvDisp3);*/
	waitKey(0);

	//// Extract the green channel
	//Mat cvGreen(cvImage.rows, cvImage.cols, CV_64FC1);
	//int fromTo [] = { 1, 0 };
	//mixChannels(&cvImage, 1, &cvGreen, 1, fromTo, 1);
	//cvImage.release();

	//double cvTV = totalVariation(cvGreen);

	//// Read green channel into Image
	//Image imGreen;
	//{ Image imRed, imBlue;
	//readColorImage(imRed, imGreen, imBlue, imgFname); }

	//double imTV = imGreen.TV();

	//std::cout << "OpenCV TV:  " << cvTV << std::endl;
	//std::cout << "Image  TV:  " << imTV << std::endl;
	//std::cout << "Difference: " << std::abs(cvTV - imTV) << std::endl;

	//// Load Image result into a Mat
	//Mat cvImDtXY(imDtXY.getWidth(), imDtXY.getHeight(), CV_64FC1, imDtXY.data());


	//Mat delta = abs(DtXY - cvImDtXY);
	//double max;
	//minMaxIdx(delta, NULL, &max);
	//std::cout << "maximum difference X: " << max << std::endl;
	//delta /= max;

	//namedWindow("wnd1", CV_WINDOW_AUTOSIZE);
	//imshow("wnd1", DtXY);
	//namedWindow("wnd2", CV_WINDOW_AUTOSIZE);
	//imshow("wnd2", cvImDtXY);
	//namedWindow("wnd3", CV_WINDOW_AUTOSIZE);
	//imshow("wnd3", delta);
	//waitKey(0);

	return 0;
}