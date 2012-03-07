/*-----------------------------------------------------------------
LOG
    GEM - Graphics Environment for Multimedia

    Adaptive threshold object

    Copyright (c) 1997-1999 Mark Danks. mark@danks.org
    Copyright (c) Günther Geiger. geiger@epy.co.at
    Copyright (c) 2001-2002 IOhannes m zmoelnig. forum::für::umläute. IEM. zmoelnig@iem.kug.ac.at
    Copyright (c) 2002 James Tittle & Chris Clepper
    For information on usage and redistribution, and for a DISCLAIMER OF ALL
    WARRANTIES, see the file, "GEM.LICENSE.TERMS" in this distribution.

-----------------------------------------------------------------*/

#ifndef INCLUDE_pix_openni_H_
#define INCLUDE_pix_openni_H_



#include <stdio.h>
#include <stdlib.h>
#include <string.h>
//#include <assert.h>
//#include <iostream>
//#include <cmath>
#include <vector>
#include <stdint.h>

#include "XnCodecIDs.h"
#include "XnOpenNI.h"
#include "XnCppWrapper.h"

#include "Base/GemBase.h"
#include "Gem/Properties.h"
#include "Gem/Image.h"
#include "Base/GemPixObj.h"

#include <pthread.h>


/*-----------------------------------------------------------------
-------------------------------------------------------------------
CLASS
    pix_openni
    

KEYWORDS
    pix
    
DESCRIPTION
   
-----------------------------------------------------------------*/
#ifdef _WIN32
class GEM_EXPORT pix_openni : public GemBase
#else
class GEM_EXTERN pix_openni : public GemBase
#endif
{
    CPPEXTERN_HEADER(pix_openni, GemBase);

    public:

	    //////////
	    // Constructor
    	pix_openni(int argc, t_atom *argv);
    	
    	t_outlet 	*m_dataout;
			
			void 				outputJoint (XnUserID player, XnSkeletonJoint eJoint);
			
			bool m_osc_output;
			bool m_real_world_coords;
			bool m_output_euler;
			bool m_auto_calibration;
			
    protected:
    	
    	//////////
    	// Destructor
    	virtual ~pix_openni();

			virtual void	startRendering();
    	//////////
    	// Rendering 	
			virtual void 	render(GemState *state);
			
			virtual void 	postrender(GemState *state);
		
		  // Stop Transfer
			virtual void	stopRendering();
    	
	//////////
    	// Settings/Info
    	//void 				outputJoint (XnUserID player, XnSkeletonJoint eJoint);
    	void				VideoModeMess(int argc, t_atom*argv);
			void				DepthModeMess(int argc, t_atom*argv);
    	void	    	bangMess();
    	
			void				renderDepth(int argc, t_atom*argv);

			static void* openni_thread_func(void*);
			
			
  // Settings
  		int x_dim;

			int openni_ready;
			
			bool m_player; //playback started?
      bool rgb_started;
      bool depth_started;
      bool audio_started;
			bool usergen_started;
      bool skeleton_started;
      bool hand_started;
      
      bool rgb_wanted;
      bool depth_wanted;
      bool audio_wanted;
      bool skeleton_wanted;
			bool usergen_wanted;
      bool hand_wanted;
			
			bool m_registration_wanted;
			bool m_registration;
			bool m_usercoloring;

			float m_skeleton_smoothing;
			
      bool destroy_thread; // shutdown...
      
			int	depth_output;
			int	req_depth_output;
			
			std::string m_filename;
  
      uint16_t t_gamma[10000];
        
			int 		m_width;
			int			m_height;
			
			XnCallbackHandle hUserCallbacks, hHandsCallbacks, hGestureCallbacks; // Hands
			XnCallbackHandle hCalibrationStart, hCalibrationComplete, hPoseDetected, hCalibrationInProgress, hPoseInProgress, hUserGeneratorNewData; // Skeleton
			
			XnChar strRequiredCalibrationPose[XN_MAX_NAME_LENGTH];
	
			bool      m_rendering; // "true" when rendering is on, false otherwise

  
    	//////////
    	// The pixBlock with the current image
    	pixBlock    	m_image;
    	pixBlock    	m_depth;
    	
    	GemState					*depth_state;
    	
			//////////
			// The current image
			imageStruct     m_imageStruct;
			
			
			
    private:
    	//////////
    	// Static member functions
    	static void			VideoModeMessCallback(void *data, t_symbol*s, int argc, t_atom*argv);
			static void			DepthModeMessCallback(void *data, t_symbol*s, int argc, t_atom*argv);
    	static void    	bangMessCallback(void *data);
    	
			static void    	openMessCallback(void *data, std::string filename);
			static void    	floatPlayMessCallback(void *data, float value);
			static void    	floatPlaybackSpeedMessCallback(void *data, float value);
			static void    	floatJumpToImageFrameMessCallback(void *data, float value);
			static void    	floatJumpToDepthFrameMessCallback(void *data, float value);
			static void    	floatRecordMessCallback(void *data, float value);
			static void    	floatRealWorldCoordsMessCallback(void *data, float value);
			static void    	floatRegistrationMessCallback(void *data, float value);
			static void    	floatRgbRegistrationMessCallback(void *data, float value);
			static void    	floatOscOutputMessCallback(void *data, float osc_output);
			static void    	floatSkeletonSmoothingMessCallback(void *data, float value);
			static void    	floatEulerOutputMessCallback(void *data, float value);
			static void    	StartUserMessCallback(void *data, t_symbol*s, int argc, t_atom*argv);
			static void    	StopUserMessCallback(void *data, t_symbol*s, int argc, t_atom*argv);
			static void    	floatAutoCalibrationMessCallback(void *data, float value);
			static void    	floatUserColoringMessCallback(void *data, float value);
			static void    	UserInfoMessCallback(void *data);
    	static void    	floatRgbMessCallback(void *data, float rgb);
    	static void    	floatDepthMessCallback(void *data, float depth);
    	static void    	floatUsergenMessCallback(void *data, float value);
    	static void    	floatSkeletonMessCallback(void *data, float skeleton);
    	static void    	floatHandMessCallback(void *data, float hand);
    	static void    	floatDepthOutputMessCallback(void *data, float depth_output);
    	static void    	renderDepthCallback(void *data, t_symbol*s, int argc, t_atom*argv);
    	
			t_outlet        *m_depthoutlet; 
			t_inlet         *m_depthinlet; 
			
			pthread_t openni_thread;

};

#endif	// for header file
