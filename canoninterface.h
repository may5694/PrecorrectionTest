/************************************************************************/
/*                                                                      */
/*			Interface for new Canon camera Rebel Xsi					*/
/*																		*/
/************************************************************************/


#include <EDSDK.h>
#include <EDSDKTypes.h>
#include <vector>
#include "cameraproperties.h"

boolean connectToFirstCamera();
boolean takeAPicture(const char *filename, void(*callback)(void*)=NULL);
int     isTakeAPictureFinished();
int     isPictureReset();
void    resetPictureTaking();
boolean disconFromFirstCamera();

// Shutter speed accessors / modifiers
int getTv();
void getTvList(std::vector<int>& list);
bool setTv(int tv);