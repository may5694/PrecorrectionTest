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

	bool searchPSF = true;			// Whether to search the PSF space (else use predefined params)
	bool loadPSFTests = false;		// Whether to load test images instead of capture them
	bool usegrad = false;			// Whether to use the gradient in PSF testing
	int saveMask = sm_disp;			// Which spaces to save
	bool align = false;				// Align the camera (don't do anything else)
	bool calib = false;				// Calibrate the camera (don't do anything else)
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
						Image F_prec_L2_disp = imgConvert.convertImage(F_prec_L2_phys, Physical, Display);

						// Convolve precorrection
						F_prec_L2_conv_phys = convolve(F_prec_L2_phys, PSF, BC_PERIODIC);
						writeImage(F_prec_L2_conv_phys, convname);

						// Capture precorrection
						showImage(F_prec_L2_disp, true);
						if (windowList.size() > 1) windowList.pop_front();
						sf::sleep(sf::seconds(0.1f));
						takeAPicture(captname.c_str());

						// Align and convert captured image
						Image F_prec_L2_capt_cam; readImage(F_prec_L2_capt_cam, captname);
						F_prec_L2_capt_cam = camAlign.alignImage(F_prec_L2_capt_cam, F_disp.getWidth(), F_disp.getHeight());
						F_prec_L2_capt_phys = imgConvert.convertImage(F_prec_L2_capt_cam, Camera, Physical);
						writeImage(F_prec_L2_capt_phys, captname);
					} else {
						readImage(F_prec_L2_conv_phys, convname);
						readImage(F_prec_L2_capt_phys, captname);
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
			writeImage(psfErr, ss.str()); ss.str("");
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
		Image F_conv_disp = imgConvert.convertImage(F_conv_phys, Physical, Display);
		Image F_conv_cam = imgConvert.convertImage(F_conv_phys, Physical, Camera);
		ss << topfolder << imgprefix << "_" << params << "_conv_phys.png";
		if (saveMask & sm_phys) writeImage(F_conv_phys, ss.str()); ss.str("");
		ss << topfolder << imgprefix << "_" << params << "_conv_disp.png";
		if (saveMask & sm_disp) writeImage(F_conv_disp, ss.str()); ss.str("");
		ss << topfolder << imgprefix << "_" << params << "_conv_cam.png";
		if (saveMask & sm_cam) writeImage(F_conv_cam, ss.str()); ss.str("");

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