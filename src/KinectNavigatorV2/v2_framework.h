#pragma once

// Common includes for the Kinect v2 proxy (Kinect20.dll): the shared nav_core
// framework plus Kinect.h. Kinect.h is header-only for our purposes -- this DLL
// never links Kinect20.lib (it would import a DLL literally named Kinect20.dll,
// which once deployed is us). GetDefaultKinectSensor is resolved via
// GetProcAddress on the renamed backend; everything else is COM vtable dispatch.

#include "framework.h"

#include <Kinect.h>
