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

#ifndef _EiC
//#include "cv.h"
#endif

#include <stdio.h>
#include <stdlib.h>
//#include <string.h>
//#include <assert.h>
//#include <iostream>
//#include <cmath>
#include <vector>

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
class GEM_EXTERN pix_openni : public GemBase
{
    CPPEXTERN_HEADER(pix_openni, GemBase);

    public:

	    //////////
	    // Constructor
    	pix_openni(int argc, t_atom *argv);
    	
    	t_outlet 	*m_dataout;
    	
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
    	void 				outputJoint (XnUserID player, XnSkeletonJoint eJoint);
    	void				VideoModeMess(int argc, t_atom*argv);
			void				DepthModeMess(int argc, t_atom*argv);
    	void	    	bangMess();
    	
			void				renderDepth(int argc, t_atom*argv);

			static void* openni_thread_func(void*);
			
	//std::vector<uint16_t> m_gamma;
			
  // Settings
  int x_dim;

	int openni_ready;
	
      bool rgb_started;
      bool depth_started;
      bool audio_started;
      bool skeleton_started;
      bool hand_started;
      
      bool rgb_wanted;
      bool depth_wanted;
      bool audio_wanted;
      bool skeleton_wanted;
      bool hand_wanted;
      
      bool destroy_thread; // shutdown...
      
			int	depth_output;
			int	req_depth_output;
  
      uint16_t t_gamma[10000];
        
	int 		m_width;
	int			m_height;
	
	XnCallbackHandle hUserCallbacks, hCalibrationStartCallback, hCalibrationCompleteCallback, hPoseCallbacks, hUserExitCallback, hUserReEnterCallback, hHandsCallbacks, hGestureCallbacks;
	
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
    	
    	static void    	floatRgbMessCallback(void *data, float rgb);
    	static void    	floatDepthMessCallback(void *data, float depth);
    	static void    	floatSkeletonMessCallback(void *data, float skeleton);
    	static void    	floatHandMessCallback(void *data, float hand);
    	static void    	floatDepthOutputMessCallback(void *data, float depth_output);
    	static void    	renderDepthCallback(void *data, t_symbol*s, int argc, t_atom*argv);
    	
			t_outlet        *m_depthoutlet; 
			t_inlet         *m_depthinlet; 
			
			pthread_t openni_thread;
			
	/////////
	// IplImage needed

  

};

#endif	// for header file
