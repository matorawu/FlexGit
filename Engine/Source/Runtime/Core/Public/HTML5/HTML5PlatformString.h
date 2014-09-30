// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.


/*=============================================================================================
	HTML5String.h: HTML5 platform string classes, mostly implemented with ANSI C++
==============================================================================================*/

#pragma once

#include "StandardPlatformString.h"

struct FHTML5PlatformString : public FStandardPlatformString
{
	static const ANSICHAR* GetEncodingName()
	{
		return "UTF-32LE"; // or should this be UTF-8?
	}
};

// default implementation
typedef FHTML5PlatformString FPlatformString;

