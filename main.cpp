#include <iostream>
#include <iomanip>
#include "precorrection.h"
#include "JNB.h"
#include "common.h"
#include "camcalib.h"
#include "canoninterface.h"

const int sm_cam = 0x4;
const int sm_phys = 0x2;
const int sm_disp = 0x1;

double dpt_cam = 2.25;
std::string imgfolder = "img/";
std::string imgprefix = "lenna";
std::string alignName;
std::string convertName;

int main(void) {
	ss << "camera_2.0m_f22_" << std::setprecision(2) << std::fixed << dpt_cam << "d_1/";
	topfolder = ss.str(); ss.str("");
	ss << topfolder << calibfolder << "align.dat";
	alignName = ss.str(); ss.str("");
	ss << topfolder << calibfolder << "convert.dat";
	convertName = ss.str(); ss.str("");

	int saveMask = sm_disp;			// Which spaces to save
	int numBest = 5;				// Pick the top numBest images to fully test

	// Align the camera
	if (!camAlign.read(alignName)) {
		camAlign.create(sw, sh);
		loop();		// Loop to continue displaying alignment image (useful for focusing camera)

		// Return here because alignment must be created in Matlab!
		return 0;
	}
	// Calibrate the camera intensity
	if (!imgConvert.read(convertName)) {
		imgConvert.create();
		imgConvert.write(convertName);
	}

	// Load the test image
	ss << imgfolder << imgprefix << ".png";
	Image F_disp; readImage(F_disp, ss.str()); ss.str("");
	ss << topfolder << imgprefix << "_disp.png";
	if (saveMask & sm_disp) writeImage(F_disp, ss.str()); ss.str("");

	// Convert original image into physical image space
	Image F_phys = imgConvert.convertImage(F_disp, Display, Physical);
	ss << topfolder << imgprefix << "_phys.png";
	if (saveMask & sm_phys) writeImage(F_phys, ss.str()); ss.str("");
	Image F_cam = imgConvert.convertImage(F_phys, Physical, Camera);
	ss << topfolder << imgprefix << "_cam.png";
	if (saveMask & sm_cam) writeImage(F_cam, ss.str()); ss.str("");

	// Capture the test image
	connectToFirstCamera();
	showImage(F_disp, true);
	sf::sleep(sf::seconds(1.0f));
	ss << topfolder << imgprefix << "_capt_cam.png";
	takeAPicture(ss.str().c_str());
	disconFromFirstCamera();

	// Load, then align the captured image
	Image F_capt_cam; readImage(F_capt_cam, ss.str()); ss.str("");
	F_capt_cam = camAlign.alignImage(F_capt_cam, F_disp.getWidth(), F_disp.getHeight());
	ss << topfolder << imgprefix << "_capt_cam.png";
	writeImage(F_capt_cam, ss.str()); ss.str("");

	// Convert captured image into physical and display space
	Image F_capt_phys = imgConvert.convertImage(F_capt_cam, Camera, Physical);
	ss << topfolder << imgprefix << "_capt_phys.png";
	if (saveMask & sm_phys) writeImage(F_capt_phys, ss.str()); ss.str("");
	Image F_capt_disp = imgConvert.convertImage(F_capt_phys, Physical, Display);
	ss << topfolder << imgprefix << "_capt_disp.png";
	if (saveMask & sm_disp) writeImage(F_capt_disp, ss.str()); ss.str("");

	// Search the PSF parameter space for the optimal PSF
	PSFRange psfRange(dpt_cam, dpt_cam, 0.1, 4.0, 7.0, 0.1, 0.3, 0.8, 0.1);
	PSFpqueue pqueue = searchPSFs(F_disp, psfRange);

	// Create a log file
	ss << topfolder << imgprefix << "_log.txt";
	FILE* logfp = fopen(ss.str().c_str(), "w"); ss.str("");
	if (!logfp) std::cout << "Could not open log file!" << std::endl;

	for (int i = 0; !pqueue.empty() && i < numBest; i++) {
		const PSFParm& ppm = pqueue.top(); pqueue.pop();

		// Load the PSF
		ss << psffolder << "psf_" << ppm.paramStr << ".dbl";
		Image PSF; PSF.fromBinary(ss.str().c_str()); ss.str("");

		// Convolve the original image
		Image F_conv_phys = convolve(F_phys, PSF, BC_PERIODIC);
		Image F_conv_disp = imgConvert.convertImage(F_conv_phys, Physical, Display);
		Image F_conv_cam = imgConvert.convertImage(F_conv_phys, Physical, Camera);
		ss << topfolder << imgprefix << "_" << ppm.paramStr << "_conv_phys.png";
		if (saveMask & sm_phys) writeImage(F_conv_phys, ss.str()); ss.str("");
		ss << topfolder << imgprefix << "_" << ppm.paramStr << "_conv_disp.png";
		if (saveMask & sm_disp) writeImage(F_conv_disp, ss.str()); ss.str("");
		ss << topfolder << imgprefix << "_" << ppm.paramStr << "_conv_cam.png";
		if (saveMask & sm_cam) writeImage(F_conv_cam, ss.str()); ss.str("");

		// Output MSE
		ss << std::setprecision(2) << std::fixed
			<< ppm.dpt << "d\t\t" << ppm.ap << "ap\t" << ppm.sf << "sf\t\t"
			<< std::setprecision(8) << std::fixed << ppm.err << std::endl;
		std::string outParam = ss.str(); ss.str("");
		std::cout << std::endl << "# " << i+1 << ":" << std::endl << outParam;
		if (logfp) fprintf(logfp, outParam.c_str());


		////////////////////////////// PRECORRECT //////////////////////////////

		Image F_prec_L1_phys, F_prec_L1_disp, F_prec_L1_cam;
		Image F_prec_L2_phys, F_prec_L2_disp, F_prec_L2_cam;
		ss << topfolder << imgprefix << "_" << ppm.paramStr << "_prec_L1_phys.png";
		std::string F_prec_L1_phys_str = ss.str(); ss.str("");
		ss << topfolder << imgprefix << "_" << ppm.paramStr << "_prec_L1_conv_phys.png";
		std::string F_prec_L1_conv_phys_str = ss.str(); ss.str("");
		ss << topfolder << imgprefix << "_" << ppm.paramStr << "_prec_L1_disp.png";
		std::string F_prec_L1_disp_str = ss.str(); ss.str("");
		ss << topfolder << imgprefix << "_" << ppm.paramStr << "_prec_L1_conv_disp.png";
		std::string F_prec_L1_conv_disp_str = ss.str(); ss.str("");
		ss << topfolder << imgprefix << "_" << ppm.paramStr << "_prec_L1_cam.png";
		std::string F_prec_L1_cam_str = ss.str(); ss.str("");
		ss << topfolder << imgprefix << "_" << ppm.paramStr << "_prec_L1_conv_cam.png";
		std::string F_prec_L1_conv_cam_str = ss.str(); ss.str("");
		ss << topfolder << imgprefix << "_" << ppm.paramStr << "_prec_L2_phys.png";
		std::string F_prec_L2_phys_str = ss.str(); ss.str("");
		ss << topfolder << imgprefix << "_" << ppm.paramStr << "_prec_L2_conv_phys.png";
		std::string F_prec_L2_conv_phys_str = ss.str(); ss.str("");
		ss << topfolder << imgprefix << "_" << ppm.paramStr << "_prec_L2_disp.png";
		std::string F_prec_L2_disp_str = ss.str(); ss.str("");
		ss << topfolder << imgprefix << "_" << ppm.paramStr << "_prec_L2_conv_disp.png";
		std::string F_prec_L2_conv_disp_str = ss.str(); ss.str("");
		ss << topfolder << imgprefix << "_" << ppm.paramStr << "_prec_L2_cam.png";
		std::string F_prec_L2_cam_str = ss.str(); ss.str("");
		ss << topfolder << imgprefix << "_" << ppm.paramStr << "_prec_L2_conv_cam.png";
		std::string F_prec_L2_conv_cam_str = ss.str(); ss.str("");

		// Precorrect the test image
		Options opts; opts.tv = TVL1;
		F_prec_L1_phys = precorrect(F_phys, PSF, 9.3e4, opts);
		if (saveMask & sm_phys) writeImage(F_prec_L1_phys, F_prec_L1_phys_str);
		F_prec_L1_disp = imgConvert.convertImage(F_prec_L1_phys, Physical, Display);
		if (saveMask & sm_disp) writeImage(F_prec_L1_disp, F_prec_L1_disp_str);
		F_prec_L1_cam = imgConvert.convertImage(F_prec_L1_phys, Physical, Camera);
		if (saveMask & sm_cam) writeImage(F_prec_L1_cam, F_prec_L1_cam_str);

		opts.tv = TVL2;
		F_prec_L2_phys = precorrect(F_phys, PSF, 9.3e4, opts);
		if (saveMask & sm_phys) writeImage(F_prec_L2_phys, F_prec_L2_phys_str);
		F_prec_L2_disp = imgConvert.convertImage(F_prec_L2_phys, Physical, Display);
		if (saveMask & sm_disp) writeImage(F_prec_L2_disp, F_prec_L2_disp_str);
		F_prec_L2_cam = imgConvert.convertImage(F_prec_L2_phys, Physical, Camera);
		if (saveMask & sm_cam) writeImage(F_prec_L2_cam, F_prec_L2_cam_str);

		// Convolve precorrections
		Image F_prec_L1_conv_phys = convolve(F_prec_L1_phys, PSF, BC_PERIODIC);
		if (saveMask & sm_phys) writeImage(F_prec_L1_conv_phys, F_prec_L1_conv_phys_str);
		Image F_prec_L1_conv_disp = imgConvert.convertImage(F_prec_L1_conv_phys, Physical, Display);
		if (saveMask & sm_disp) writeImage(F_prec_L1_conv_disp, F_prec_L1_conv_disp_str);
		Image F_prec_L1_conv_cam = imgConvert.convertImage(F_prec_L1_conv_phys, Physical, Camera);
		if (saveMask & sm_cam) writeImage(F_prec_L1_conv_cam, F_prec_L1_conv_cam_str);

		Image F_prec_L2_conv_phys = convolve(F_prec_L2_phys, PSF, BC_PERIODIC);
		if (saveMask & sm_phys) writeImage(F_prec_L2_conv_phys, F_prec_L2_conv_phys_str);
		Image F_prec_L2_conv_disp = imgConvert.convertImage(F_prec_L2_conv_phys, Physical, Display);
		if (saveMask & sm_disp) writeImage(F_prec_L2_conv_disp, F_prec_L2_conv_disp_str);
		Image F_prec_L2_conv_cam = imgConvert.convertImage(F_prec_L2_conv_phys, Physical, Camera);
		if (saveMask & sm_cam) writeImage(F_prec_L2_conv_cam, F_prec_L2_conv_cam_str);


		///////////////////////////////////// CAPTURE PRECORRECTIONS /////////////////////

		ss << topfolder << imgprefix << "_" << ppm.paramStr << "_prec_L1_capt_cam.png";
		std::string F_prec_L1_capt_cam_str = ss.str(); ss.str("");
		ss << topfolder << imgprefix << "_" << ppm.paramStr << "_prec_L1_capt_phys.png";
		std::string F_prec_L1_capt_phys_str = ss.str(); ss.str("");
		ss << topfolder << imgprefix << "_" << ppm.paramStr << "_prec_L1_capt_disp.png";
		std::string F_prec_L1_capt_disp_str = ss.str(); ss.str("");
		ss << topfolder << imgprefix << "_" << ppm.paramStr << "_prec_L2_capt_cam.png";
		std::string F_prec_L2_capt_cam_str = ss.str(); ss.str("");
		ss << topfolder << imgprefix << "_" << ppm.paramStr << "_prec_L2_capt_phys.png";
		std::string F_prec_L2_capt_phys_str = ss.str(); ss.str("");
		ss << topfolder << imgprefix << "_" << ppm.paramStr << "_prec_L2_capt_disp.png";
		std::string F_prec_L2_capt_disp_str = ss.str(); ss.str("");

		// Display precorrected images and capture
		connectToFirstCamera();
		showImage(F_prec_L1_disp, true);
		sf::sleep(sf::seconds(1.0f));
		takeAPicture(F_prec_L1_capt_cam_str.c_str());

		showImage(F_prec_L2_disp, true);
		sf::sleep(sf::seconds(1.0f));
		takeAPicture(F_prec_L2_capt_cam_str.c_str());
		disconFromFirstCamera();

		// Load, align, and convert captured precorrections
		Image F_prec_L1_capt_cam; readImage(F_prec_L1_capt_cam, F_prec_L1_capt_cam_str);
		F_prec_L1_capt_cam = camAlign.alignImage(F_prec_L1_capt_cam, F_disp.getWidth(), F_disp.getHeight());
		writeImage(F_prec_L1_capt_cam, F_prec_L1_capt_cam_str);
		Image F_prec_L1_capt_phys = imgConvert.convertImage(F_prec_L1_capt_cam, Camera, Physical);
		if (saveMask & sm_phys) writeImage(F_prec_L1_capt_phys, F_prec_L1_capt_phys_str);
		Image F_prec_L1_capt_disp = imgConvert.convertImage(F_prec_L1_capt_phys, Physical, Display);
		if (saveMask & sm_disp) writeImage(F_prec_L1_capt_disp, F_prec_L1_capt_disp_str);

		Image F_prec_L2_capt_cam; readImage(F_prec_L2_capt_cam, F_prec_L2_capt_cam_str);
		F_prec_L2_capt_cam = camAlign.alignImage(F_prec_L2_capt_cam, F_disp.getWidth(), F_disp.getHeight());
		writeImage(F_prec_L2_capt_cam, F_prec_L2_capt_cam_str);
		Image F_prec_L2_capt_phys = imgConvert.convertImage(F_prec_L2_capt_cam, Camera, Physical);
		if (saveMask & sm_phys) writeImage(F_prec_L2_capt_phys, F_prec_L2_capt_phys_str);
		Image F_prec_L2_capt_disp = imgConvert.convertImage(F_prec_L2_capt_phys, Physical, Display);
		if (saveMask & sm_disp) writeImage(F_prec_L2_capt_disp, F_prec_L2_capt_disp_str);
	}

	if (logfp) fclose(logfp);
	std::cout << "Done!" << std::endl;

	loop();

	return 0;
}