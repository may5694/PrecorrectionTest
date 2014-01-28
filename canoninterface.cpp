/************************************************************************/
/*                                                                      */
/*			Interface for new Canon camera Rebel Xsi					*/
/*																		*/
/************************************************************************/


#include "canoninterface.h"
#include <iostream>

const int CAMERA_RESET = 0;
const int CAMERA_BEGIN = 1;
const int CAMERA_COMPLETE = 2;

static EdsCameraRef camera = NULL;
static int pictureProgress = 0;
static boolean isSDKLoaded = false;
static void(*callback)(void*) = NULL;
static char theFileName[1024];

EdsError downloadImage(EdsDirectoryItemRef directoryItem, char* filename)
{
	EdsError err = EDS_ERR_OK;
	EdsStreamRef stream = NULL;
	// Get directory item information
	EdsDirectoryItemInfo dirItemInfo;
	err = EdsGetDirectoryItemInfo(directoryItem, & dirItemInfo);
	//printf("err EdsGetDirectoryItemInfo: %d\n", err);
	// Create file stream for transfer destination
	if(err == EDS_ERR_OK)
	{
		err = EdsCreateFileStream( theFileName,
			kEdsFileCreateDisposition_CreateAlways,
			kEdsAccess_ReadWrite, &stream);
	   //printf("err EdsCreateFileStream: %X\n", err);
	}
	// Download image
	if(err == EDS_ERR_OK)
	{
		err = EdsDownload( directoryItem, dirItemInfo.size, stream);
	//printf("err EdsDownload: %d\n", err);

	}
	// Issue notification that download is complete
	char* buf = NULL;
	if(err == EDS_ERR_OK)
	{
		err = EdsDownloadComplete(directoryItem);
		//printf("err EdsDownloadComplete: %d\n", err);
		buf = new char [strlen(theFileName)+1];
		strcpy_s(buf, strlen(theFileName)+1, theFileName);
		theFileName[0] = '\0';
	}
	// Release stream
	if( stream != NULL)
	{
		EdsRelease(stream);
		stream = NULL;
	}
	pictureProgress = CAMERA_COMPLETE;
	resetPictureTaking();

	// Notify done downloading
	if (buf) {
		std::cout << "snapped " << buf << std::endl;
		std::flush(std::cout);
		delete [] buf; buf = NULL;
	}
	return err;
}

EdsError EDSCALLBACK handleObjectEvent( EdsObjectEvent event,
									   EdsBaseRef object,
									   EdsVoid * context)
{
	//printf("object event (context=\"%s\").\n", (char *)context);
	//printf("object event (object=\"%s\").\n", (char *)object);
	// do something
	switch(event)
	{
	case kEdsObjectEvent_DirItemRequestTransfer:
	   //printf("download!\n");
		downloadImage(object, (char*)context);
		break;
	default:
		break;
	}
	
	// Object must be released
	if(object){
		EdsRelease(object);
	}
	if ( callback != NULL )
		callback((char*)context);
	return EDS_ERR_OK;
}

boolean connectToFirstCamera(){
	
	EdsError err = EDS_ERR_OK;

	// Initialize SDK
	err = EdsInitializeSDK();
	if ( err == EDS_ERR_OK )
		isSDKLoaded = true;


/*	// Get first camera
	if(err == EDS_ERR_OK)
	{
		err = getFirstCamera();
	}*/

	EdsCameraListRef cameraList = NULL;
	EdsUInt32 count = 0;

	// Get camera list
	err = EdsGetCameraList(&cameraList);

	// Get number of cameras
	if(err == EDS_ERR_OK)
	{
		err = EdsGetChildCount(cameraList, &count);
		if(count == 0)
		{
			err = EDS_ERR_DEVICE_NOT_FOUND;
		}
	}

	// Get first camera retrieved
	if(err == EDS_ERR_OK)
	{
		err = EdsGetChildAtIndex(cameraList , 0 , &camera);
	}

	// Release camera list
	if(cameraList != NULL)
	{
		EdsRelease(cameraList);
		cameraList = NULL;
	}

	//Open Session
	if(err == EDS_ERR_OK)
	{
		err = EdsOpenSession(camera);
	}

	//Set save to host
	if (err == EDS_ERR_OK ) {
		EdsUInt32 wkSaveTo = kEdsSaveTo_Host ;
		do {
			err = EdsSetPropertyData ( camera, kEdsPropID_SaveTo, 
				0, 4, (void*)&wkSaveTo ) ;
			if (err != EDS_ERR_OK) {
				std::cout << "warning: camera busy, waiting..." << std::endl;
				std::flush(std::cout);
				Sleep(100);
			}
		} while (err != EDS_ERR_OK);
	}

	// Set host capacity
	EdsCapacity capacity = {0x7FFFFFFF, 0x1000, 1};
	err = EdsSetCapacity ( camera, capacity );

	return err == EDS_ERR_OK;
}

boolean takeAPicture(const char* filename, void(*cb)(void*)){

	//printf("taking a picture\n");
	if (pictureProgress != CAMERA_RESET) {
		std::cout << "error: camera not reset" << std::endl;
		std::flush(std::cout);
		return false;
	}
	EdsError err = EDS_ERR_OK;
	callback = cb;
	
	// Set event handler
	if(err == EDS_ERR_OK)
	{
		//printf("%s\n", filename);
		strcpy_s(theFileName, 1024, filename);
		//printf("%s\n", theFileName);
		if (camera==NULL) {
			std::cout << "error: camera is null" << std::endl;
			std::flush(std::cout);
			return false;
		}
		err = EdsSetObjectEventHandler(camera, kEdsObjectEvent_All,
			handleObjectEvent, theFileName);
	}

	pictureProgress = CAMERA_BEGIN; // picture started
	err = EdsSendCommand( camera, kEdsCameraCommand_TakePicture, 0);
	//handleObjectEvent( kEdsObjectEvent_DirItemRequestTransfer,NULL, theFileName);
	//printf("done!\n");

	// Run a message loop to guarantee the event handler runs
	MSG msg;
	while(pictureProgress == CAMERA_BEGIN) {        
		GetMessage(&msg, NULL, NULL, NULL);
		TranslateMessage(&msg); 
		DispatchMessage(&msg); 
	}

	return err == EDS_ERR_OK;
}


int isTakeAPictureFinished()
{
	return (pictureProgress == CAMERA_COMPLETE) ? 1 : 0;
}

int isPictureReset()
{
	return (pictureProgress == CAMERA_RESET);
}

void resetPictureTaking()
{
	pictureProgress = CAMERA_RESET;
}


boolean disconFromFirstCamera(){

	EdsError err = EDS_ERR_OK;
	// Close session with camera
	err = EdsCloseSession(camera);

	// Release camera
	if(camera != NULL)
	{
		EdsRelease(camera);
	}
	// Terminate SDK
	if(isSDKLoaded)
	{
		EdsTerminateSDK();
	}
	pictureProgress = CAMERA_RESET;
	return err == EDS_ERR_OK;
}

// Shutter speed accessors / modifiers
int getTv() {
	EdsError err = EDS_ERR_OK;
	EdsDataType dataType;
	EdsUInt32 dataSize;
	EdsInt32 Tv;
	err = EdsGetPropertySize(camera, kEdsPropID_Tv, 0, &dataType, &dataSize);
	if (!err) {
		err = EdsGetPropertyData(camera, kEdsPropID_Tv, 0, dataSize, &Tv);
		if (!err) {
			return Tv;
		}
	}

	return 0;
}
void getTvList(std::vector<int>& list) {
	EdsError err = EDS_ERR_OK;
	EdsPropertyDesc TvDesc;
	err = EdsGetPropertyDesc(camera, kEdsPropID_Tv, &TvDesc);
	if (!err) {
		list = std::vector<int>(TvDesc.propDesc, TvDesc.propDesc + TvDesc.numElements);
	}
}
bool setTv(int tv) {
	EdsError err = EdsSetPropertyData(camera, kEdsPropID_Tv, 0, sizeof(EdsInt32), &tv);
	return !err;
}