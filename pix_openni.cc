////////////////////////////////////////////////////////
//
// GEM - Graphics Environment for Multimedia
//
// zmoelnig@iem.kug.ac.at
//
// Implementation file
//
//    Copyright (c) 1997-2000 Mark Danks.
//    Copyright (c) Günther Geiger.
//    Copyright (c) 2001-2002 IOhannes m zmoelnig. forum::für::umläute. IEM
//    Copyright (c) 2002 James Tittle & Chris Clepper
//    For information on usage and redistribution, and for a DISCLAIMER OF ALL
//    WARRANTIES, see the file, "GEM.LICENSE.TERMS" in this distribution.
//
/////////////////////////////////////////////////////////


#include "pix_openni.h"
#include "Gem/State.h"
#include "Gem/Exception.h"

using namespace xn;

#ifdef __APPLE__
	static int index_offset=1;
#else
	static int index_offset=0;
#endif


CPPEXTERN_NEW_WITH_GIMME(pix_openni);
//---------------------------------------------------------------------------
// Defines
//---------------------------------------------------------------------------
//#define SAMPLE_XML_PATH "SamplesConfig.xml"  // should be replaced with in-code initialisation

#define DISPLAY_MODE_OVERLAY	1
#define DISPLAY_MODE_DEPTH		2
#define DISPLAY_MODE_IMAGE		3
#define DEFAULT_DISPLAY_MODE	DISPLAY_MODE_DEPTH

#define MAX_DEPTH 10000

#define GESTURE_TO_USE "Wave"

//---------------------------------------------------------------------------
// Globals
//---------------------------------------------------------------------------
float g_pDepthHist[MAX_DEPTH];
XnRGB24Pixel* g_pTexMap = NULL;
unsigned int g_nTexMapX = 0;
unsigned int g_nTexMapY = 0;

unsigned int g_nViewState = DEFAULT_DISPLAY_MODE;

Context g_context;
ScriptNode g_scriptNode;
DepthGenerator g_depth;
ImageGenerator g_image;
DepthMetaData g_depthMD;
ImageMetaData g_imageMD;

Recorder g_recorder;

UserGenerator g_UserGenerator;
Player g_player;

HandsGenerator g_HandsGenerator;
GestureGenerator gestureGenerator;

XnBool g_bNeedPose = FALSE;
XnChar g_strPose[20] = "";
XnBool g_bDrawBackground = TRUE;
XnBool g_bDrawPixels = TRUE;
XnBool g_bDrawSkeleton = TRUE;
XnBool g_bPrintID = TRUE;
XnBool g_bPrintState = TRUE;

float posConfidence;
float orientConfidence;

//SCELETON SCALE
float mult_x = 1;
float mult_y = 1;
float mult_z = 1;

float off_x = 0;
float off_y = 0;
float off_z = 0;


//gesture callbacks
void XN_CALLBACK_TYPE Gesture_Recognized(xn::GestureGenerator& generator, const XnChar* strGesture, const XnPoint3D* pIDPosition, const XnPoint3D* pEndPosition, void* pCookie) {
		pix_openni *me = (pix_openni*)pCookie;
    me->post("Gesture recognized: %s\n", strGesture);
    gestureGenerator.RemoveGesture(strGesture);
    g_HandsGenerator.StartTracking(*pEndPosition);
}

void XN_CALLBACK_TYPE Gesture_Process(xn::GestureGenerator& generator, const XnChar* strGesture, const XnPoint3D* pPosition, XnFloat fProgress, void* pCookie) {
}

//hand callbacks new_hand, update_hand, lost_hand

void XN_CALLBACK_TYPE new_hand(xn::HandsGenerator &generator, XnUserID nId, const XnPoint3D *pPosition, XnFloat fTime, void *pCookie) {
	pix_openni *me = (pix_openni*)pCookie;
	//me->post("New Hand %d\n", nId);
	
	t_atom ap[1];
	SETFLOAT (ap, (int)nId);
	if (me->m_osc_output)
	{
		outlet_anything(me->m_dataout, gensym("/hand/new_hand"), 1, ap);
	} else {
		outlet_anything(me->m_dataout, gensym("new_hand"), 1, ap);
	}


}
void XN_CALLBACK_TYPE lost_hand(xn::HandsGenerator &generator, XnUserID nId, XnFloat fTime, void *pCookie) {
  gestureGenerator.AddGesture(GESTURE_TO_USE, NULL);

	pix_openni *me = (pix_openni*)pCookie;
	//me->post("Lost Hand %d\n", nId);
	
	t_atom ap[1];
	SETFLOAT (ap, (int)nId);
	
	if (me->m_osc_output)
	{
		outlet_anything(me->m_dataout, gensym("/hand/lost_hand"), 1, ap);
	} else {
		outlet_anything(me->m_dataout, gensym("lost_hand"), 1, ap);
	}
}
void XN_CALLBACK_TYPE update_hand(xn::HandsGenerator &generator, XnUserID nID, const XnPoint3D *pPosition, XnFloat fTime, void *pCookie) {
	pix_openni *me = (pix_openni*)pCookie;
	
	float jointCoords[3];
	
	if (me->m_real_world_coords)
	{
		jointCoords[0] = pPosition->X; //Normalize coords to 0..1 interval
		jointCoords[1] = pPosition->Y; //Normalize coords to 0..1 interval
		jointCoords[2] = pPosition->Z; //Normalize coords to 0..7.8125 interval
	}
	if (!me->m_real_world_coords)
	{
		jointCoords[0] = off_x + (mult_x * (480 - pPosition->X) / 960); //Normalize coords to 0..1 interval
		jointCoords[1] = off_y + (mult_y * (320 - pPosition->Y) / 640); //Normalize coords to 0..1 interval
		jointCoords[2] = off_z + (mult_z * pPosition->Z * 7.8125 / 10000); //Normalize coords to 0..7.8125 interval
	}

	t_atom ap[4];
		
	SETFLOAT (ap+0, (int)nID);
	SETFLOAT (ap+1, jointCoords[0]);
	SETFLOAT (ap+2, jointCoords[1]);
	SETFLOAT (ap+3, jointCoords[2]);
	if (me->m_osc_output)
	{
		outlet_anything(me->m_dataout, gensym("/hand/coords"), 4, ap);
	} else {
		outlet_anything(me->m_dataout, gensym("hand"), 4, ap);
	}
}

// Sceleton Callbacks
// Callback: New user was detected
void XN_CALLBACK_TYPE User_NewUser(xn::UserGenerator& generator, XnUserID nId, void* pCookie)
{
	pix_openni *me = (pix_openni*)pCookie;

	//me->post("New User %d\n", nId);
	
	t_atom ap[1];
	SETFLOAT (ap, (int)nId);
	
	if (me->m_osc_output)
	{
		outlet_anything(me->m_dataout, gensym("/skeleton/new_user"), 1, ap);
	} else {
		outlet_anything(me->m_dataout, gensym("new_user"), 1, ap);
	}
	
	// New user found
	if (g_bNeedPose)
	{
		g_UserGenerator.GetPoseDetectionCap().StartPoseDetection(g_strPose, nId);
	}
	else
	{
		g_UserGenerator.GetSkeletonCap().RequestCalibration(nId, TRUE);
	}
}
// Callback: An existing user was lost
void XN_CALLBACK_TYPE User_LostUser(xn::UserGenerator& generator, XnUserID nId, void* pCookie)
{
	pix_openni *me = (pix_openni*)pCookie;
	
	t_atom ap[1];
	SETFLOAT (ap, (int)nId);
	if (me->m_osc_output)
	{
		outlet_anything(me->m_dataout, gensym("/skeleton/lost_user"), 1, ap);
	} else {
		outlet_anything(me->m_dataout, gensym("lost_user"), 1, ap);
	}
}
// Callback: Detected a pose
void XN_CALLBACK_TYPE UserPose_PoseDetected(xn::PoseDetectionCapability& capability, const XnChar* strPose, XnUserID nId, void* pCookie)
{
	pix_openni *me = (pix_openni*)pCookie;
	
	//me->post("Pose %s detected for user %d\n", strPose, nId);
	
	t_atom ap[1];
	SETFLOAT (ap, (int)nId);
	if (me->m_osc_output)
	{
		outlet_anything(me->m_dataout, gensym("/skeleton/pose_detected"), 1, ap);
	} else {
		outlet_anything(me->m_dataout, gensym("pose_detected"), 1, ap);
	}

	g_UserGenerator.GetPoseDetectionCap().StopPoseDetection(nId);
	g_UserGenerator.GetSkeletonCap().RequestCalibration(nId, TRUE);
}
// Callback: Started calibration
void XN_CALLBACK_TYPE UserCalibration_CalibrationStart(xn::SkeletonCapability& capability, XnUserID nId, void* pCookie)
{
	pix_openni *me = (pix_openni*)pCookie;
	//me->post("Calibration started for user %d\n", nId);
	
	t_atom ap[1];
	SETFLOAT (ap, (int)nId);
	if (me->m_osc_output)
	{
		outlet_anything(me->m_dataout, gensym("/skeleton/calib_started"), 1, ap);
	} else {
		outlet_anything(me->m_dataout, gensym("calib_started"), 1, ap);
	}
}
// Callback: Finished calibration
void XN_CALLBACK_TYPE UserCalibration_CalibrationEnd(xn::SkeletonCapability& capability, XnUserID nId, XnBool bSuccess, void* pCookie)
{
	pix_openni *me = (pix_openni*)pCookie;
	
  t_atom ap[1];
  SETFLOAT (ap, (int)nId);
  
	if (bSuccess)
	{
		// Calibration succeeded
		//me->post("Calibration complete, start tracking user %d\n", nId);

		if (me->m_osc_output)
		{
			outlet_anything(me->m_dataout, gensym("/skeleton/new_skel"), 1, ap);
		} else {
			outlet_anything(me->m_dataout, gensym("new_skel"), 1, ap);
		}

	
		g_UserGenerator.GetSkeletonCap().StartTracking(nId);
	}
	else
	{
		// Calibration failed
		//post("Calibration failed for user %d\n", nId);
		
		if (me->m_osc_output)
		{
			outlet_anything(me->m_dataout, gensym("/skeleton/new_skel_failed"), 1, ap);
		} else {
			outlet_anything(me->m_dataout, gensym("new_skel_failed"), 1, ap);
		}
		
		if (g_bNeedPose)
		{
			g_UserGenerator.GetPoseDetectionCap().StartPoseDetection(g_strPose, nId);
		}
		else
		{
			g_UserGenerator.GetSkeletonCap().RequestCalibration(nId, TRUE);
		}
	}
}

void XN_CALLBACK_TYPE UserCalibration_CalibrationComplete(xn::SkeletonCapability& capability, XnUserID nId, XnCalibrationStatus eStatus, void* pCookie)
{
	pix_openni *me = (pix_openni*)pCookie;
	
  t_atom ap[1];
  SETFLOAT (ap, (int)nId);
  
	if (eStatus == XN_CALIBRATION_STATUS_OK)
	{
		// Calibration succeeded
		//me->post("Calibration complete, start tracking user %d\n", nId);

		if (me->m_osc_output)
		{
			outlet_anything(me->m_dataout, gensym("/skeleton/new_skel"), 1, ap);
		} else {
			outlet_anything(me->m_dataout, gensym("new_skel"), 1, ap);
		}
	
		g_UserGenerator.GetSkeletonCap().StartTracking(nId);
		g_UserGenerator.GetSkeletonCap().StartTracking(nId);
	}
	else
	{
		// Calibration failed
		//me->post("Calibration failed for user %d\n", nId);
		
		if (me->m_osc_output)
		{
			outlet_anything(me->m_dataout, gensym("/skeleton/new_skel_failed"), 1, ap);
		} else {
			outlet_anything(me->m_dataout, gensym("new_skel_failed"), 1, ap);
		}
		
		if (g_bNeedPose)
		{
			g_UserGenerator.GetPoseDetectionCap().StartPoseDetection(g_strPose, nId);
		}
		else
		{
			g_UserGenerator.GetSkeletonCap().RequestCalibration(nId, TRUE);
		}
	}
}

/////////////////////////////////////////////////////////
//
// pix_openni
//
/////////////////////////////////////////////////////////
// Constructor
//
/////////////////////////////////////////////////////////

//pix_openni :: pix_openni(t_float kinect_device_nr, t_float rgb_on, t_float depth_on)
pix_openni :: pix_openni(int argc, t_atom *argv)
{
	// second inlet/outlet for depthmap
	m_depthoutlet = outlet_new(this->x_obj, 0);
	m_depthinlet  = inlet_new(this->x_obj, &this->x_obj->ob_pd, gensym("gem_state"), gensym("depth_state"));

	m_dataout = outlet_new(this->x_obj, 0);
	
	post("pix_openni 0.03 - experimental - 2011/2012 by Matthias Kronlachner");

	// init status variables
	m_player = false;
	depth_wanted = false;
	depth_started= false; 
	rgb_wanted = false;
	rgb_started= false; 
	skeleton_wanted = false;
	skeleton_started= false; 
	hand_wanted = false;
	hand_started= false; 
	
	openni_ready = false;
	destroy_thread = false;
	
	m_osc_output = false;
	m_real_world_coords = false;
	
	depth_output = 0;
	req_depth_output = 0;
	
	// CHECK FOR ARGS AND ACTIVATE STREAMS
	if (argc >= 1)
	{
		post("using multiple kinects not available now... chosen ID: %d", atom_getint(&argv[0]));
	}
	if (argc >= 2)
	{
		if (atom_getint(&argv[1]) != 0)
		{
			rgb_wanted = true;
		}
	}
	if (argc >= 3)
	{
		if (atom_getint(&argv[2]) != 0)
		{
			depth_wanted = true;
		}
	}
	if (argc >= 4)
	{
		if (atom_getint(&argv[3]) != 0)
		{
			skeleton_wanted = true;
		}
	}
	if (argc >= 5)
	{
		if (atom_getint(&argv[4]) != 0)
		{
			hand_wanted = true;
		}
	}

	m_width=640;
  m_height=480;

	XnStatus rc;  // ERROR STATUS
	
//// INIT IN CODE:: not working now

		//Context context;
		rc = g_context.Init(); // key difference: Init() not InitFromXml()
		if (rc != XN_STATUS_OK)
		{
			post("OPEN NI init() failed.");
		} else {
			post("OPEN NI initialised successfully.");
			openni_ready = true;
		}
	
	// CREATE THREAD FOR SKELETON AND HAND
	//int res = pthread_create(&openni_thread, NULL, openni_thread_func, this);
	//if (res) {
		//throw(GemException("pthread_create failed\n"));
	//}
	
	//// DEPTH CONVERSION
	//depth map representation for mm output
  for (int i=0; i<10000; i++) {
  	float v = i/7000.0;
  	v = powf(v, 3)* 6;
  	t_gamma[i] = v*6*256;
  }
		
}

/////////////////////////////////////////////////////////
// Destructor
//
/////////////////////////////////////////////////////////
pix_openni :: ~pix_openni()
{ 
	destroy_thread = true;
	//pthread_detach(openni_thread);
	
	g_context.StopGeneratingAll();
	
	if (depth_started)
	{
		g_depth.Release();
	}
		
	if (rgb_started)
	{
		g_image.Release();
	}
	
	if (skeleton_started)
	{
		g_UserGenerator.Release();
	}
	
	g_context.Release();
	
	//g_context.Shutdown();
	
	//g_context.StopGeneratingAll();
	//g_context.ContextRelease(g_context);
}

/////////////////////////////////////////////////////////
// Thread Function
//
/////////////////////////////////////////////////////////
#if 0
void *pix_openni::openni_thread_func(void*target)
{
	pix_openni *me = (pix_openni*) target;
	while (!me->destroy_thread) {
		if (me->openni_ready)
		{
			XnStatus rc = XN_STATUS_OK;
			//rc = g_context.WaitNoneUpdateAll();
			//if (rc != XN_STATUS_OK)
			//{
				//me->post("Read failed: %s\n", xnGetStatusString(rc));
			//} else {
				////post("Read: %s\n", xnGetStatusString(rc));
			//}
			
			if (me->skeleton_wanted && !me->skeleton_started)
			{
				me->post("trying to start skeleton...");

					rc = g_UserGenerator.Create(g_context);
					if (rc != XN_STATUS_OK)
					{
						me->post("OpenNI:: skeleton node couldn't be created! %s", xnGetStatusString(rc));
						me->skeleton_wanted = false;
					} else {
						XnCallbackHandle hUserCallbacks, hCalibrationStart, hCalibrationComplete, hPoseDetected, hCalibrationInProgress, hPoseInProgress;
						if (!g_UserGenerator.IsCapabilitySupported(XN_CAPABILITY_SKELETON))
						{
							me->post("Supplied user generator doesn't support skeleton\n");
						}
						
						rc = g_UserGenerator.RegisterUserCallbacks(User_NewUser, User_LostUser, me, hUserCallbacks);
						me->post("Register to user callbacks", rc);
						rc = g_UserGenerator.GetSkeletonCap().RegisterToCalibrationStart(UserCalibration_CalibrationStart, me, hCalibrationStart);
						me->post("Register to calibration start", rc);
						rc = g_UserGenerator.GetSkeletonCap().RegisterToCalibrationComplete(UserCalibration_CalibrationComplete, me, hCalibrationComplete);
						me->post("Register to calibration complete", rc);

						if (g_UserGenerator.GetSkeletonCap().NeedPoseForCalibration())
						{
							g_bNeedPose = TRUE;
							if (!g_UserGenerator.IsCapabilitySupported(XN_CAPABILITY_POSE_DETECTION))
							{
								me->post("Pose required, but not supported\n");
							}
							rc = g_UserGenerator.GetPoseDetectionCap().RegisterToPoseDetected(UserPose_PoseDetected, me, hPoseDetected);
							me->post("Register to Pose Detected", rc);
							g_UserGenerator.GetSkeletonCap().GetCalibrationPose(g_strPose);
						}

						g_UserGenerator.GetSkeletonCap().SetSkeletonProfile(XN_SKEL_PROFILE_ALL);

						//rc = g_UserGenerator.GetSkeletonCap().RegisterToCalibrationInProgress(MyCalibrationInProgress, NULL, hCalibrationInProgress);
						me->post("Register to calibration in progress", rc);

						//rc = g_UserGenerator.GetPoseDetectionCap().RegisterToPoseInProgress(MyPoseInProgress, NULL, hPoseInProgress);
						me->post("Register to pose in progress", rc);
						
						g_context.StartGeneratingAll();
						
						me->skeleton_started = true;
					}
			}
			
			if (!me->skeleton_wanted && me->skeleton_started)
			{
				g_UserGenerator.Release();
				me->skeleton_started = false;
			}
			
			
			if (me->skeleton_started)
			{
				// SCELETON OUTPUT
				float jointCoords[3];
				
				XnSkeletonJointTransformation jointTrans;
				
				XnUserID aUsers[15];
				XnUInt16 nUsers = 15;
				g_UserGenerator.GetUsers(aUsers, nUsers);
				
				t_atom ap[4];
				
				
				for (int i = 0; i < nUsers; ++i) {
					if (g_UserGenerator.GetSkeletonCap().IsTracking(aUsers[i])) {
						g_UserGenerator.GetSkeletonCap().GetSkeletonJoint(aUsers[i], XN_SKEL_LEFT_HAND, jointTrans);
						
						for(int j = 0; j <= 24; ++j)
						{
							me->outputJoint(aUsers[i], (XnSkeletonJoint) j);
							//me->post("got joint");
						}
					}
				}
			}
		}
	
		// struct timespec test;	/*nanosleep brauch das timespec Structure*/

		// test.tv_sec  = 0;		/*was tv_sec und tv_nsec (Sek. und Nanosek.) enthält*/
		// test.tv_nsec = 5000;
		// nanosleep(&test,NULL);
	}
	me->post("freenect thread ended");
	return 0;
}
#endif

/////////////////////////////////////////////////////////
// startRendering
//
/////////////////////////////////////////////////////////

void pix_openni :: startRendering(){
	
  m_rendering=true;

  //return true;
}

/////////////////////////////////////////////////////////
// render
//
/////////////////////////////////////////////////////////

void pix_openni :: render(GemState *state)
{
	if (openni_ready)
		{
			// UPDATE ALL  --> BETTER IN THREAD?!
			XnStatus rc = XN_STATUS_OK;
			rc = g_context.WaitNoneUpdateAll();
			if (rc != XN_STATUS_OK)
			{
				post("Read failed: %s\n", xnGetStatusString(rc));
				return;
			} else {
				//post("Read: %s\n", xnGetStatusString(rc));
			}
		
		if (rgb_wanted && !rgb_started)
		{
			post("trying to start rgb stream");
			XnStatus rc;
			rc = g_image.Create(g_context);
			if (rc != XN_STATUS_OK)
			{
				post("OpenNI:: image node couldn't be created! %d", rc);
				rgb_wanted=false;
			} else {
				XnMapOutputMode mapMode;
				mapMode.nXRes = 640;
				mapMode.nYRes = 480;
				mapMode.nFPS = 30;
				g_image.SetMapOutputMode(mapMode);
				g_image.SetPixelFormat(XN_PIXEL_FORMAT_RGB24);
				rgb_started = true;
				g_context.StartGeneratingAll();
				
				m_image.image.xsize = mapMode.nXRes;
			  m_image.image.ysize = mapMode.nYRes;
			  m_image.image.setCsizeByFormat(GL_RGBA);
			  m_image.image.reallocate();
			
				post("OpenNI:: Image node created!");
			}
		}
		
		if (!rgb_wanted && rgb_started)
		{
			//g_image.Release();
			rgb_started = false;
		}
		
		// OUTPUT RGB IMAGE
		if (rgb_wanted && rgb_started)
		{
					g_image.GetMetaData(g_imageMD);
					
					//m_image.image.data= (unsigned char*)g_imageMD.RGB24Data();
					if (((int)g_imageMD.XRes() != (int)m_image.image.xsize) || ((int)g_imageMD.YRes() != (int)m_image.image.ysize))
					{
						m_image.image.xsize = g_imageMD.XRes();
						m_image.image.ysize = g_imageMD.YRes();
						m_image.image.reallocate();
					}
					
					const XnUInt8* pImage = g_imageMD.Data();
					
					int size = m_image.image.xsize * m_image.image.ysize * m_image.image.csize;
							
					int i=0;
					
					while (i<=size-1-index_offset) {
						int num=(i%4)+floor(i/4)*3;
						if ((i % 4)==3)
							{
									m_image.image.data[i+index_offset]=255.0;
							}  else  {
									m_image.image.data[i+index_offset]=(unsigned char)pImage[num];
							}
						i++;
					}
					
					m_image.newimage = 1;
					m_image.image.notowned = true;
					m_image.image.upsidedown=true;
					state->set(GemState::_PIX, &m_image);
			}
			
			
			// SKELETON CODE IN RENDER METHOD!! -> PACK INTO THREAD!

			if (skeleton_wanted && !skeleton_started)
			{
				post("trying to start skeleton...");
					rc = g_context.FindExistingNode(XN_NODE_TYPE_USER, g_UserGenerator);
					//if (nRetVal != XN_STATUS_OK)
					post("OpenNI:: skeleton found skeleton? : %s", xnGetStatusString(rc));
					
					rc = g_UserGenerator.Create(g_context);
					if (rc != XN_STATUS_OK)
					{
						post("OpenNI:: skeleton node couldn't be created! %s", xnGetStatusString(rc));
						skeleton_wanted=false;
					} else {
						XnCallbackHandle hUserCallbacks, hCalibrationStart, hCalibrationComplete, hPoseDetected, hCalibrationInProgress, hPoseInProgress;
						if (!g_UserGenerator.IsCapabilitySupported(XN_CAPABILITY_SKELETON))
						{
							post("Supplied user generator doesn't support skeleton\n");
						}
						
						rc = g_UserGenerator.RegisterUserCallbacks(User_NewUser, User_LostUser, this, hUserCallbacks);
						post("Register to user callbacks: %s", xnGetStatusString(rc));
						rc = g_UserGenerator.GetSkeletonCap().RegisterToCalibrationStart(UserCalibration_CalibrationStart, this, hCalibrationStart);
						post("Register to calibration start: %s", xnGetStatusString(rc));
						rc = g_UserGenerator.GetSkeletonCap().RegisterToCalibrationComplete(UserCalibration_CalibrationComplete, this, hCalibrationComplete);
						post("Register to calibration complete: %s", xnGetStatusString(rc));

						if (g_UserGenerator.GetSkeletonCap().NeedPoseForCalibration())
						{
							g_bNeedPose = TRUE;
							if (!g_UserGenerator.IsCapabilitySupported(XN_CAPABILITY_POSE_DETECTION))
							{
								post("Pose required, but not supported\n");
							}
							rc = g_UserGenerator.GetPoseDetectionCap().RegisterToPoseDetected(UserPose_PoseDetected, this, hPoseDetected);
							post("Register to Pose Detected", rc);
							g_UserGenerator.GetSkeletonCap().GetCalibrationPose(g_strPose);
						}

						g_UserGenerator.GetSkeletonCap().SetSkeletonProfile(XN_SKEL_PROFILE_ALL);

						//rc = g_UserGenerator.GetSkeletonCap().RegisterToCalibrationInProgress(MyCalibrationInProgress, NULL, hCalibrationInProgress);
						post("Register to calibration in progress", rc);

						//rc = g_UserGenerator.GetPoseDetectionCap().RegisterToPoseInProgress(MyPoseInProgress, NULL, hPoseInProgress);
						post("Register to pose in progress", rc);
						
						g_context.StartGeneratingAll();
						
						skeleton_started = true;
					}
			}
			
			if (!skeleton_wanted && skeleton_started)
			{
				g_UserGenerator.Release();
				skeleton_started = false;
			}
			
			
			if (skeleton_started)
			{
				// SCELETON OUTPUT
				float jointCoords[3];
				
				XnSkeletonJointTransformation jointTrans;
				
				XnUserID aUsers[15];
				XnUInt16 nUsers = 15;
				g_UserGenerator.GetUsers(aUsers, nUsers);
				
				t_atom ap[4];
				
				
				for (int i = 0; i < nUsers; ++i) {
					if (g_UserGenerator.GetSkeletonCap().IsTracking(aUsers[i])) {
						g_UserGenerator.GetSkeletonCap().GetSkeletonJoint(aUsers[i], XN_SKEL_LEFT_HAND, jointTrans);
						
						for(int j = 0; j <= 24; ++j)
						{
							outputJoint(aUsers[i], (XnSkeletonJoint) j);
						}
					}
				}
			}
			
		
		// HAND GESTURES
			if (hand_wanted && !hand_started)
			{
				post("trying to start hand tracking...");
				XnCallbackHandle hHandsCallbacks, hGestureCallbacks;
				rc = g_HandsGenerator.Create(g_context);
				if (rc != XN_STATUS_OK)
				{
					post("OpenNI:: HandsGenerator node couldn't be created!");
					hand_wanted=false;
					return;
				} else {
					rc = gestureGenerator.Create(g_context);
					if (rc != XN_STATUS_OK)
					{
						post("OpenNI:: GestureGenerator node couldn't be created!");
						hand_wanted=false;
						return;
					} else {
						rc = gestureGenerator.RegisterGestureCallbacks(Gesture_Recognized, Gesture_Process, this, hGestureCallbacks);
						post("RegisterGestureCallbacks: %s\n", xnGetStatusString(rc));
						rc = g_HandsGenerator.RegisterHandCallbacks(new_hand, update_hand, lost_hand, this, hHandsCallbacks);
						post("RegisterHandCallbacks: %s\n", xnGetStatusString(rc));
						g_HandsGenerator.SetSmoothing(0.2);
						g_context.StartGeneratingAll();
						rc = gestureGenerator.AddGesture(GESTURE_TO_USE, NULL);
						if (rc == XN_STATUS_OK)
						{
							post("OpenNI:: HandTracking started!");
							hand_started = true;
						}
					}
				}
		}
		
		if (!hand_wanted && hand_started)
		{
			gestureGenerator.Release();
			g_HandsGenerator.Release();
			
			hand_started = false;
		}
		
		if (m_player)
		{
			const XnChar* strNodeName = NULL;	
			XnUInt32 nFrame_image = 0;
			XnUInt32 totFrame_image = 0;
			strNodeName = g_image.GetName();
			g_player.TellFrame(strNodeName, nFrame_image);
			g_player.GetNumFrames(strNodeName, totFrame_image);
			
			
			XnUInt32 nFrame_depth = 0;
			XnUInt32 totFrame_depth = 0;
			strNodeName = g_depth.GetName();
			g_player.TellFrame(strNodeName,nFrame_depth);
			g_player.GetNumFrames(strNodeName, totFrame_depth);
			
			t_atom ap[4];
			SETFLOAT (ap+0, nFrame_image);
			SETFLOAT (ap+1, totFrame_image);
			SETFLOAT (ap+2, nFrame_depth);
			SETFLOAT (ap+3, totFrame_depth);

			outlet_anything(m_dataout, gensym("playback"), 4, ap);
		}
		
		}
}

void pix_openni :: renderDepth(int argc, t_atom*argv)
{
  if (argc==2 && argv->a_type==A_POINTER && (argv+1)->a_type==A_POINTER) // is it gem_state?
  {
		depth_state =  (GemState *) (argv+1)->a_w.w_gpointer;
		
		// start depth stream if wanted
		if (depth_wanted && !depth_started)
		{
			post("trying to start depth stream");
			if (openni_ready)
			{
				XnStatus rc;
				rc = g_depth.Create(g_context);
				if (rc != XN_STATUS_OK)
				{
					post("OpenNI:: Depth node couldn't be created!");
					depth_wanted=false;
				} else {
					XnMapOutputMode mapMode;
					mapMode.nXRes = 640;
					mapMode.nYRes = 480;
					mapMode.nFPS = 30;
					g_depth.SetMapOutputMode(mapMode);
					depth_started = true;
					g_context.StartGeneratingAll();
					post("OpenNI:: Depth node created!");
					
					m_depth.image.xsize = mapMode.nXRes;
				  m_depth.image.ysize = mapMode.nYRes;
				  m_depth.image.setCsizeByFormat(GL_RGBA);
				  m_depth.image.reallocate();
				}
			}
		}
		
		if (!depth_wanted && depth_started)
		{
			//g_depth.Release();
			depth_started = false;
		}
	
		if (depth_wanted && depth_started) //DEPTH OUTPUT
		{

			// check if depth output request changed -> reallocate image_struct
			if (req_depth_output != depth_output)
			{
			  if ((req_depth_output == 0) || (req_depth_output == 2))
				{
					m_depth.image.setCsizeByFormat(GL_RGBA);
				}
				if (req_depth_output == 1)
				{
					m_depth.image.setCsizeByFormat(GL_LUMINANCE);
				}
				if (req_depth_output == 3)
				{
					m_depth.image.setCsizeByFormat(GL_YCBCR_422_GEM);
				}
				m_depth.image.reallocate();
				depth_output=req_depth_output;
			}
			
			g_depth.GetMetaData(g_depthMD);
			//const XnDepthPixel* pDepth = g_depthMD.Data();
			const XnDepthPixel* pDepth = g_depth.GetDepthMap(); 
			
				if (depth_output == 0) // RGBA mapped
				{
		
					uint8_t *pixels = m_depth.image.data;
		
					uint16_t *depth_pixel = (uint16_t*)g_depthMD.Data();
					
					for( unsigned int i = 0 ; i < 640*480 ; i++) {
						int pval = t_gamma[depth_pixel[i]];
						int lb = pval & 0xff;
						int form_mult = 4; // Changed for RGBA (4 instead of originally 3)
						switch (pval>>8) {
						case 0:																					
							pixels[form_mult*i+0+index_offset] = 255;
							pixels[form_mult*i+1+index_offset] = 255-lb;
							pixels[form_mult*i+2+index_offset] = 255-lb;
							break;
						case 1:
							pixels[form_mult*i+0+index_offset] = 255;
							pixels[form_mult*i+1+index_offset] = lb;
							pixels[form_mult*i+2+index_offset] = 0;
							break;
						case 2:
							pixels[form_mult*i+0+index_offset] = 255-lb;
							pixels[form_mult*i+1+index_offset] = 255;
							pixels[form_mult*i+2+index_offset] = 0;
							break;
						case 3:
							pixels[form_mult*i+0+index_offset] = 0;
							pixels[form_mult*i+1+index_offset] = 255;
							pixels[form_mult*i+2+index_offset] = lb;
							break;
						case 4:
							pixels[form_mult*i+0+index_offset] = 0;
							pixels[form_mult*i+1+index_offset] = 255-lb;
							pixels[form_mult*i+2+index_offset] = 255;
							break;
						case 5:
							pixels[form_mult*i+0+index_offset] = 0;
							pixels[form_mult*i+1+index_offset] = 0;
							pixels[form_mult*i+2+index_offset] = 255-lb;
							break;
						default:
							pixels[form_mult*i+0+index_offset] = 0;
							pixels[form_mult*i+1+index_offset] = 0;
							pixels[form_mult*i+2+index_offset] = 0;
							break;
						}
					}
				}
				
				if (depth_output == 1) // GREYSCALE
				{
					uint8_t *pixels = m_depth.image.data;
		
					uint16_t *depth_pixel = (uint16_t*)g_depthMD.Data();
					
					for(int y = 0; y < 640*480; y++) {
						pixels[y+index_offset]=(uint8_t)(depth_pixel[y] >> 5);
						//pixels[4*y+1+index_offset]=(uint8_t)(depth_pixel[y] / 2);
						//pixels[4*y+2+index_offset]=(uint8_t)(depth_pixel[y] / 2);
						//pixels[4*y+2+index_offset]=(uint8_t)(depth_pixel[y] / 2);
					}
				}

				if (depth_output == 2) // RAW RGBA
				{
					uint8_t *pixels = m_depth.image.data;
		
					uint16_t *depth_pixel = (uint16_t*)g_depthMD.Data();
					  
					for(int y = 0; y < 640*480; y++) {
						//pixels[4*y]=255;
						//pixels[4*y+1]=255;
						pixels[4*y+index_offset]=(uint8_t)(depth_pixel[y] >> 8);
						pixels[4*y+1+index_offset]=(uint8_t)(depth_pixel[y] & 0xff);
						pixels[4*y+2+index_offset]=0;
						//pixels[4*y+3+index_offset]=0;
					}
				}
				
				if (depth_output == 3) // RAW YUV
				{
					const XnDepthPixel* pDepth = g_depthMD.Data();
					m_depth.image.data= (unsigned char*)&pDepth[0];
				}
				
				m_depth.newimage = 1;
				m_depth.image.notowned = true;
				m_depth.image.upsidedown=true;
				depth_state->set(GemState::_PIX, &m_depth);
				//got_depth=0;
				
				t_atom ap[2];
				ap->a_type=A_POINTER;
				ap->a_w.w_gpointer=(t_gpointer *)m_cache;  // the cache ?
				(ap+1)->a_type=A_POINTER;
				(ap+1)->a_w.w_gpointer=(t_gpointer *)depth_state;
				outlet_anything(m_depthoutlet, gensym("gem_state"), 2, ap);
		}
	}
}

///////////////////////////////////////
// POSTRENDERING -> Clear
///////////////////////////////////////

void pix_openni :: postrender(GemState *state)
{

}

///////////////////////////////////////
// STOPRENDERING -> Stop Transfer
///////////////////////////////////////

void pix_openni :: stopRendering(){


}


//////////////////////////////////////////
// Output Joint
//////////////////////////////////////////

void pix_openni :: outputJoint (XnUserID player, XnSkeletonJoint eJoint)
{
	t_atom ap[5];
	float jointCoords[3];

	XnSkeletonJointTransformation jointTrans;

	g_UserGenerator.GetSkeletonCap().GetSkeletonJoint(player, eJoint, jointTrans);

	posConfidence = jointTrans.position.fConfidence;

	if (m_real_world_coords)
	{
		jointCoords[0] = jointTrans.position.position.X; //Normalize coords to 0..1 interval
		jointCoords[1] = jointTrans.position.position.Y; //Normalize coords to 0..1 interval
		jointCoords[2] = jointTrans.position.position.Z; //Normalize coords to 0..7.8125 interval
	} else {
		jointCoords[0] = off_x + (mult_x * (1280 - jointTrans.position.position.X) / 2560); //Normalize coords to 0..1 interval
		jointCoords[1] = off_y + (mult_y * (960 - jointTrans.position.position.Y) / 1920); //Normalize coords to 0..1 interval
		jointCoords[2] = off_z + (mult_z * jointTrans.position.position.Z * 7.8125 / 10000); //Normalize coords to 0..7.8125 interval
	}

	if (!m_osc_output)
	{
		switch(eJoint)
		{
			case 1:
			SETSYMBOL (ap+0, gensym("head")); break;
			case 2:
			SETSYMBOL (ap+0, gensym("neck")); break;
			case 3:
			SETSYMBOL (ap+0, gensym("torso")); break;
			case 4:
			SETSYMBOL (ap+0, gensym("waist")); break;
		case 5:
			SETSYMBOL (ap+0, gensym("l_collar")); break;
		case 6:
			SETSYMBOL (ap+0, gensym("l_shoulder")); break;
		case 7:
			SETSYMBOL (ap+0, gensym("l_elbow")); break;
		case 8:
			SETSYMBOL (ap+0, gensym("l_wrist")); break;
		case 9:
			SETSYMBOL (ap+0, gensym("l_hand")); break;
		case 10:
			SETSYMBOL (ap+0, gensym("l_fingertip")); break;
		case 11:
			SETSYMBOL (ap+0, gensym("r_collar")); break;
		case 12:
			SETSYMBOL (ap+0, gensym("r_shoulder")); break;
		case 13:
			SETSYMBOL (ap+0, gensym("r_elbow")); break;
		case 14:
			SETSYMBOL (ap+0, gensym("r_wrist")); break;
		case 15:
			SETSYMBOL (ap+0, gensym("r_hand")); break;
		case 16:
			SETSYMBOL (ap+0, gensym("r_fingertip")); break;
		case 17:
			SETSYMBOL (ap+0, gensym("l_hip")); break;
		case 18:
			SETSYMBOL (ap+0, gensym("l_knee")); break;
		case 19:
			SETSYMBOL (ap+0, gensym("l_ankle")); break;
		case 20:
			SETSYMBOL (ap+0, gensym("l_foot")); break;
		case 21:
			SETSYMBOL (ap+0, gensym("r_hip")); break;
		case 22:
			SETSYMBOL (ap+0, gensym("r_knee")); break;
		case 23:
			SETSYMBOL (ap+0, gensym("r_ankle")); break;
		case 24:
			SETSYMBOL (ap+0, gensym("r_foot"));	break;
	}
	
	SETFLOAT (ap+1, (int)player);
	SETFLOAT (ap+2, jointCoords[0]);
	SETFLOAT (ap+3, jointCoords[1]);
	SETFLOAT (ap+4, jointCoords[2]);

	outlet_anything(m_dataout, gensym("joint"), 5, ap);
}

// OUTPUT IN OSC STYLE -> /joint/l_foot id x y z
if (m_osc_output)
{
	SETFLOAT (ap+0, (int)player);
	SETFLOAT (ap+1, jointCoords[0]);
	SETFLOAT (ap+2, jointCoords[1]);
	SETFLOAT (ap+3, jointCoords[2]);
	
	switch(eJoint)
	{
		case 1:
			outlet_anything(m_dataout, gensym("/skeleton/joint/head"), 4, ap); break;
		case 2:
				outlet_anything(m_dataout, gensym("/skeleton/joint/neck"), 4, ap); break;
		case 3:
					outlet_anything(m_dataout, gensym("/skeleton/joint/torso"), 4, ap); break;
		case 4:
					outlet_anything(m_dataout, gensym("/skeleton/joint/waist"), 4, ap); break;
		case 5:
					outlet_anything(m_dataout, gensym("/skeleton/joint/l_collar"), 4, ap); break;
		case 6:
					outlet_anything(m_dataout, gensym("/skeleton/joint/l_shoulder"), 4, ap); break;
		case 7:
					outlet_anything(m_dataout, gensym("/skeleton/joint/l_elbow"), 4, ap); break;
		case 8:
					outlet_anything(m_dataout, gensym("/skeleton/joint/l_wrist"), 4, ap); break;
		case 9:
					outlet_anything(m_dataout, gensym("/skeleton/joint/l_hand"), 4, ap); break;
		case 10:
					outlet_anything(m_dataout, gensym("/skeleton/joint/l_fingertip"), 4, ap); break;
		case 11:
					outlet_anything(m_dataout, gensym("/skeleton/joint/r_collar"), 4, ap); break;
		case 12:
					outlet_anything(m_dataout, gensym("/skeleton/joint/r_shoulder"), 4, ap); break;
		case 13:
					outlet_anything(m_dataout, gensym("/skeleton/joint/r_elbow"), 4, ap); break;
		case 14:
					outlet_anything(m_dataout, gensym("/skeleton/joint/r_wrist"), 4, ap); break;
		case 15:
					outlet_anything(m_dataout, gensym("/skeleton/joint/r_hand"), 4, ap); break;
		case 16:
					outlet_anything(m_dataout, gensym("/skeleton/joint/r_fingertip"), 4, ap); break;
		case 17:
					outlet_anything(m_dataout, gensym("/skeleton/joint/l_hip"), 4, ap); break;
		case 18:
					outlet_anything(m_dataout, gensym("/skeleton/joint/l_knee"), 4, ap); break;
		case 19:
					outlet_anything(m_dataout, gensym("/skeleton/joint/l_ankle"), 4, ap); break;
		case 20:
					outlet_anything(m_dataout, gensym("/skeleton/joint/l_foot"), 4, ap); break;
		case 21:
					outlet_anything(m_dataout, gensym("/skeleton/joint/r_hip"), 4, ap); break;
		case 22:
					outlet_anything(m_dataout, gensym("/skeleton/joint/r_knee"), 4, ap); break;
		case 23:
					outlet_anything(m_dataout, gensym("/skeleton/joint/r_ankle"), 4, ap); break;
		case 24:
					outlet_anything(m_dataout, gensym("/skeleton/joint/r_foot"), 4, ap); break;
	}
}
}

//////////////////////////////////////////
// Messages - Settings
//////////////////////////////////////////

void pix_openni :: VideoModeMess (int argc, t_atom*argv)
{
	if (argc == 3 && argv->a_type==A_FLOAT && (argv+1)->a_type==A_FLOAT && (argv+2)->a_type==A_FLOAT)
	{
		XnStatus rc;	
		XnMapOutputMode mapMode;
		mapMode.nXRes = atom_getint(&argv[0]);
		mapMode.nYRes = atom_getint(&argv[1]);
		mapMode.nFPS = atom_getint(&argv[2]);
		rc = g_image.SetMapOutputMode(mapMode);
		post("OpenNI:: trying to set image mode to %ix%i @ %i Hz", atom_getint(&argv[0]), atom_getint(&argv[1]), atom_getint(&argv[2]));
		post("OpenNI:: %s", xnGetStatusString(rc));
	}
}

void pix_openni :: DepthModeMess (int argc, t_atom*argv)
{
	if (argc == 3 && argv->a_type==A_FLOAT && (argv+1)->a_type==A_FLOAT && (argv+2)->a_type==A_FLOAT)
	{
		XnStatus rc;	
		XnMapOutputMode mapMode;
		mapMode.nXRes = atom_getint(&argv[0]);
		mapMode.nYRes = atom_getint(&argv[1]);
		mapMode.nFPS = atom_getint(&argv[2]);
		rc = g_depth.SetMapOutputMode(mapMode);
		post("OpenNI:: trying to set depth mode to %ix%i @ %i Hz", atom_getint(&argv[0]), atom_getint(&argv[1]), atom_getint(&argv[2]));
		post("OpenNI:: %s", xnGetStatusString(rc));
	}
}

void pix_openni :: bangMess ()
{
	// OUTPUT OPENNI DEVICES OPENED
	int nRetVal;
	
	NodeInfoList devicesList;
	  nRetVal = g_context.EnumerateExistingNodes(devicesList, XN_NODE_TYPE_DEVICE);
	  if (nRetVal != XN_STATUS_OK)
	  {
	    post("Failed to enumerate device nodes: %s\n", xnGetStatusString(nRetVal));
	    return;
	  }
		int i=0;
		for (NodeInfoList::Iterator it = devicesList.Begin(); it != devicesList.End(); ++it, ++i)
		  {
		    // Create the device node
		    NodeInfo deviceInfo = *it;

		    post ("Opened Device: %s", deviceInfo.GetInstanceName());

		}

	// OUTPUT AVAILABLE MODES
	if (depth_started)
	{
		post("\nOpenNI:: Current Depth Output Mode: %ix%i @ %d Hz", g_depthMD.XRes(), g_depthMD.YRes(), g_depthMD.FPS());
		XnUInt32 xNum = g_depth.GetSupportedMapOutputModesCount();
		post("OpenNI:: Supported depth modes:");
		XnMapOutputMode* aMode = new XnMapOutputMode[xNum];
		g_depth.GetSupportedMapOutputModes( aMode, xNum );for( unsigned int i = 0; i < xNum; ++ i )
		{
					post("Mode %i : %ix%i @ %d Hz", i, aMode[i].nXRes, aMode[i].nYRes, aMode[i].nFPS);

		}	
		delete[] aMode;
	}
	
	if (rgb_started)
	{
		post("OpenNI:: Current Image (rgb) Output Mode: %ix%i @ %d Hz", g_imageMD.XRes(), g_imageMD.YRes(), g_imageMD.FPS());
					
		XnUInt32 xNum = g_image.GetSupportedMapOutputModesCount();
		post("OpenNI:: Supported image (rgb) modes:");
		XnMapOutputMode* aMode = new XnMapOutputMode[xNum];
		g_image.GetSupportedMapOutputModes( aMode, xNum );for( unsigned int i = 0; i < xNum; ++ i )
		{
					post("Mode %i : %ix%i @ %d Hz", i, aMode[i].nXRes, aMode[i].nYRes, aMode[i].nFPS);

		}	
		delete[] aMode;
	}
}

/////////////////////////////////////////////////////////
// static member function
//
/////////////////////////////////////////////////////////
void pix_openni :: obj_setupCallback(t_class *classPtr)
{
  class_addmethod(classPtr, (t_method)&pix_openni::VideoModeMessCallback,
  		  gensym("video_mode"), A_GIMME, A_NULL);
  class_addmethod(classPtr, (t_method)&pix_openni::DepthModeMessCallback,
  		  gensym("depth_mode"), A_GIMME, A_NULL);
  class_addmethod(classPtr, (t_method)&pix_openni::bangMessCallback,
  		  gensym("bang"), A_NULL);
  class_addmethod(classPtr, (t_method)&pix_openni::floatRgbMessCallback,
  		  gensym("rgb"), A_FLOAT, A_NULL);
  class_addmethod(classPtr, (t_method)&pix_openni::floatDepthMessCallback,
  		  gensym("depth"), A_FLOAT, A_NULL);
  class_addmethod(classPtr, (t_method)&pix_openni::floatSkeletonMessCallback,
  		  gensym("skeleton"), A_FLOAT, A_NULL);
  class_addmethod(classPtr, (t_method)&pix_openni::floatHandMessCallback,
  		  gensym("hand"), A_FLOAT, A_NULL);
  class_addmethod(classPtr, (t_method)&pix_openni::floatDepthOutputMessCallback,
  		  gensym("depth_output"), A_FLOAT, A_NULL);
	class_addmethod(classPtr, (t_method)&pix_openni::floatRegistrationMessCallback,
	  	  gensym("registration"), A_FLOAT, A_NULL);
	class_addmethod(classPtr, (t_method)&pix_openni::floatRealWorldCoordsMessCallback,
	  	  gensym("real_world_coords"), A_FLOAT, A_NULL);
	class_addmethod(classPtr, (t_method)&pix_openni::floatOscOutputMessCallback,
	  	  gensym("osc_style_output"), A_FLOAT, A_NULL);

	class_addmethod(classPtr, (t_method)&pix_openni::openMessCallback,
	  	  gensym("open"), A_SYMBOL, A_NULL);
	class_addmethod(classPtr, (t_method)&pix_openni::floatRecordMessCallback,
			  gensym("record"), A_FLOAT, A_NULL);
	class_addmethod(classPtr, (t_method)&pix_openni::floatPlayMessCallback,
				gensym("play"), A_FLOAT, A_NULL);
	class_addmethod(classPtr, (t_method)&pix_openni::floatPlaybackSpeedMessCallback,
				gensym("playback_speed"), A_FLOAT, A_NULL);
	class_addmethod(classPtr, (t_method)&pix_openni::floatJumpToImageFrameMessCallback,
				gensym("jump_image_frame"), A_FLOAT, A_NULL);
	class_addmethod(classPtr, (t_method)&pix_openni::floatJumpToDepthFrameMessCallback,
				gensym("jump_depth_frame"), A_FLOAT, A_NULL);

	class_addmethod(classPtr, (t_method)(&pix_openni::renderDepthCallback),
        gensym("depth_state"), A_GIMME, A_NULL);
}

void pix_openni :: VideoModeMessCallback(void *data, t_symbol*s, int argc, t_atom*argv)
{
  GetMyClass(data)->VideoModeMess(argc, argv);
}

void pix_openni :: DepthModeMessCallback(void *data, t_symbol*s, int argc, t_atom*argv)
{
  GetMyClass(data)->DepthModeMess(argc, argv);
}

void pix_openni :: bangMessCallback(void *data)
{
  GetMyClass(data)->bangMess();
}

void pix_openni :: floatDepthOutputMessCallback(void *data, t_floatarg depth_output)
{
  pix_openni *me = (pix_openni*)GetMyClass(data);
  if ((depth_output >= 0) && (depth_output) <= 3)
		me->req_depth_output=(int)depth_output;
}

void pix_openni :: floatRealWorldCoordsMessCallback(void *data, t_floatarg value)
{
  pix_openni *me = (pix_openni*)GetMyClass(data);
  if ((int)value == 0)
		me->m_real_world_coords=false;
  if ((int)value == 1)
		me->m_real_world_coords=true;
}

// TO BE DONE....!
void pix_openni :: floatRecordMessCallback(void *data, t_floatarg value)
{
  pix_openni *me = (pix_openni*)GetMyClass(data);
	int nRetVal = 0;
	//me->post("filename: %s", me->m_filename.data());
	if (((int)value == 1) && (me->m_filename.data() != ""))
	{
		nRetVal = g_context.FindExistingNode(XN_NODE_TYPE_PLAYER, g_recorder);
		me->post("found recorder? %s", xnGetStatusString(nRetVal));
		nRetVal=g_recorder.Create(g_context);
		me->post("created recorder. %s", xnGetStatusString(nRetVal));
		
		nRetVal=g_recorder.SetDestination(XN_RECORD_MEDIUM_FILE, me->m_filename.data()); // set filename
		me->post("set file name. %s", xnGetStatusString(nRetVal));
		
		nRetVal = g_recorder.AddNodeToRecording(g_image, XN_CODEC_JPEG);
		me->post("added image node. %s", xnGetStatusString(nRetVal));
		nRetVal = g_recorder.AddNodeToRecording(g_depth, XN_CODEC_16Z_EMB_TABLES);
		me->post("added depth node. %s", xnGetStatusString(nRetVal));
		
	}
	
	if ((int)value == 0)
	{
		g_recorder.Release();
		me->post("recording stopped.");
	}
}

// TO BE DONE....!
void pix_openni :: floatPlayMessCallback(void *data, t_floatarg value)
{
  pix_openni *me = (pix_openni*)GetMyClass(data);
	int nRetVal = 0;
	//me->post("filename: %s", me->m_filename.data());
	if (((int)value == 1) && (me->m_filename.data() != ""))
	{
		nRetVal = g_context.OpenFileRecording(me->m_filename.data(), g_player) ;
		me->post("opened file recording? %s", xnGetStatusString(nRetVal));
		if (nRetVal == XN_STATUS_OK) me->m_player = true;
	}
	
	if ((int)value == 0)
	{
		g_context.Release();
	}
}

void pix_openni :: floatPlaybackSpeedMessCallback(void *data, t_floatarg value)
{
  pix_openni *me = (pix_openni*)GetMyClass(data);
	int nRetVal = 0;
	//me->post("filename: %s", me->m_filename.data());
	g_player.SetPlaybackSpeed((double)value);
}

void pix_openni :: floatJumpToImageFrameMessCallback(void *data, t_floatarg value)
{
  pix_openni *me = (pix_openni*)GetMyClass(data);
	int nRetVal = 0;

	const XnChar* strNodeName = NULL;
	strNodeName = g_image.GetName();

	g_player.SeekToFrame(strNodeName,(XnUInt32)value,XN_PLAYER_SEEK_SET);
}

void pix_openni :: floatJumpToDepthFrameMessCallback(void *data, t_floatarg value)
{
  pix_openni *me = (pix_openni*)GetMyClass(data);
	int nRetVal = 0;

	const XnChar* strNodeName = NULL;
	strNodeName = g_depth.GetName();

	g_player.SeekToFrame(strNodeName,(XnUInt32)value,XN_PLAYER_SEEK_SET);
}

void pix_openni :: openMessCallback(void *data, std::string filename)
{
	pix_openni *me = (pix_openni*)GetMyClass(data);
	// filename.empty() not working correctly!?	
	if (filename.data() != "")
	{
		me->post("filename set to %s", filename.data());
		me->m_filename = filename;
	}
}

void pix_openni :: floatRegistrationMessCallback(void *data, t_floatarg value)
{
  pix_openni *me = (pix_openni*)GetMyClass(data);
	int nRetVal = 0;
	if ((int)value == 1)
	{
		if (g_depth.IsCapabilitySupported(XN_CAPABILITY_ALTERNATIVE_VIEW_POINT))
		{
			nRetVal = g_depth.GetAlternativeViewPointCap().SetViewPoint(g_image);
		}
		me->post("changed to registered depth mode. %s", xnGetStatusString(nRetVal));
	}
	
	if ((int)value == 0)
	{
		if (g_depth.IsCapabilitySupported(XN_CAPABILITY_ALTERNATIVE_VIEW_POINT))
		{
			nRetVal = g_depth.GetAlternativeViewPointCap().ResetViewPoint();
		}
		me->post("changed to unregistered depth mode. %s", xnGetStatusString(nRetVal));
	}
}

void pix_openni :: floatOscOutputMessCallback(void *data, t_floatarg osc_output)
{
  pix_openni *me = (pix_openni*)GetMyClass(data);
  if ((int)osc_output == 0)
		me->m_osc_output=false;
  if ((int)osc_output == 1)
		me->m_osc_output=true;
}

void pix_openni :: floatRgbMessCallback(void *data, t_floatarg rgb)
{
  pix_openni *me = (pix_openni*)GetMyClass(data);
  if ((int)rgb == 0)
		me->rgb_wanted=false;
  if ((int)rgb == 1)
		me->rgb_wanted=true;
}

void pix_openni :: floatDepthMessCallback(void *data, t_floatarg depth)
{
  pix_openni *me = (pix_openni*)GetMyClass(data);
  if ((int)depth == 0)
		me->depth_wanted=false;
  if ((int)depth == 1)
		me->depth_wanted=true;
}

void pix_openni :: floatSkeletonMessCallback(void *data, t_floatarg skeleton)
{
  pix_openni *me = (pix_openni*)GetMyClass(data);
  if ((int)skeleton == 0)
		me->skeleton_wanted=false;
  if ((int)skeleton == 1)
		me->skeleton_wanted=true;
}

void pix_openni :: floatHandMessCallback(void *data, t_floatarg hand)
{
  pix_openni *me = (pix_openni*)GetMyClass(data);
  if ((int)hand == 0)
		me->hand_wanted=false;
  if ((int)hand == 1)
		me->hand_wanted=true;
}

void pix_openni :: renderDepthCallback(void *data, t_symbol*s, int argc, t_atom*argv)
{
	GetMyClass(data)->renderDepth(argc, argv);
}
