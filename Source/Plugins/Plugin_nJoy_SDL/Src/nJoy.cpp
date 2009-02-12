//////////////////////////////////////////////////////////////////////////////////////////
// Project description
// ŻŻŻŻŻŻŻŻŻŻŻŻŻŻŻŻŻŻŻ
// Name: nJoy
// Description: A Dolphin Compatible Input Plugin
//
// Author: Falcon4ever (nJoy@falcon4ever.com)
// Site: www.multigesture.net
// Copyright (C) 2003-2008 Dolphin Project.
//
//////////////////////////////////////////////////////////////////////////////////////////
//
// Licensetype: GNU General Public License (GPL)
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 2.0.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License 2.0 for more details.
//
// A copy of the GPL 2.0 should have been included with the program.
// If not, see http://www.gnu.org/licenses/
//
// Official SVN repository and contact information can be found at
// http://code.google.com/p/dolphin-emu/
//
//////////////////////////////////////////////////////////////////////////////////////////

 

////////////////////////////////////////////////////////////////////////////////////////
// Issues
/* ŻŻŻŻŻŻŻŻŻ

   The StrangeHack in ConfigAdvanced.cpp doesn't work in Linux, it still wont resize the
   window correctly. So currently in Linux you have to have advanced controls enabled when
   you open the window to see them.

////////////////////////*/

 

////////////////////////////////////////////////////////////////////////////////////////
// Variables guide
/* ŻŻŻŻŻŻŻŻŻ

   Joyinfo[1, 2, 3, 4, ..., number of attached devices]: Gamepad info that is populate by Search_Devices()
   PadMapping[1, 2, 3 and 4]: The button mapping
   Joystate[1, 2, 3 and 4]: The current button states

   The arrays PadMapping[] and PadState[] are numbered 0 to 3 for the four different virtual
   controllers. Joysticks[].ID will have the number of the physical input device mapped to that
   controller (this value range between 0 and the total number of connected physical devices). The
   mapping of a certain physical device to PadState[].joy is initially done by Initialize(), but
   for the configuration we can remap that, like in ConfigBox::ChangeJoystick().

   The joyinfo[] array holds the physical gamepad info for a certain physical device. It's therefore
   used as joyinfo[PadMapping[controller].ID] if we want to get the joyinfo for a certain joystick.

////////////////////////*/

 

//////////////////////////////////////////////////////////////////////////////////////////
// Include
// ŻŻŻŻŻŻŻŻŻ
#include "nJoy.h"

// Declare config window so that we can write debugging info to it from functions in this file
#if defined(HAVE_WX) && HAVE_WX
	ConfigBox* m_frame;
#endif
/////////////////////////

 
//////////////////////////////////////////////////////////////////////////////////////////
// Variables
// ŻŻŻŻŻŻŻŻŻ

// Rumble in windows
#define _EXCLUDE_MAIN_ // Avoid certain declarations in nJoy.h
FILE *pFile;
HINSTANCE nJoy_hInst = NULL;
std::vector<InputCommon::CONTROLLER_INFO> joyinfo;
InputCommon::CONTROLLER_STATE PadState[4];
InputCommon::CONTROLLER_MAPPING PadMapping[4];
bool g_EmulatorRunning = false;
int NumPads = 0, NumGoodPads = 0, LastPad = 0;
#ifdef _WIN32
	HWND m_hWnd = NULL, m_hConsole = NULL; // Handle to window
#endif
SPADInitialize *g_PADInitialize = NULL;

// TODO: fix this dirty hack to stop missing symbols
void __Log(int log, const char *format, ...) {}
void __Logv(int log, int v, const char *format, ...) {}

// Rumble
#ifdef _WIN32

#elif defined(__linux__)
	extern int fd;
#endif

 
//////////////////////////////////////////////////////////////////////////////////////////
// wxWidgets
// ŻŻŻŻŻŻŻŻŻ
#if defined(HAVE_WX) && HAVE_WX
	class wxDLLApp : public wxApp
	{
		bool OnInit()
		{
			return true;
		}
	};

	IMPLEMENT_APP_NO_MAIN(wxDLLApp)
	WXDLLIMPEXP_BASE void wxSetInstance(HINSTANCE hInst);
#endif

 
//////////////////////////////////////////////////////////////////////////////////////////
// DllMain
// ŻŻŻŻŻŻŻ
#ifdef _WIN32
BOOL APIENTRY DllMain(	HINSTANCE hinstDLL,	// DLL module handle
						DWORD dwReason,		// reason called
						LPVOID lpvReserved)	// reserved
{
	switch (dwReason)
	{
		case DLL_PROCESS_ATTACH:
		{       
			//use wxInitialize() if you don't want GUI instead of the following 12 lines
			wxSetInstance((HINSTANCE)hinstDLL);
			int argc = 0;
			char **argv = NULL;
			wxEntryStart(argc, argv);

			if (!wxTheApp || !wxTheApp->CallOnInit() )
				return FALSE;
		}
		break;

		case DLL_PROCESS_DETACH:		
			wxEntryCleanup(); //use wxUninitialize() if you don't want GUI
		break;

		default:
			break;
	}

	nJoy_hInst = hinstDLL;
	return TRUE;
}
#endif


//////////////////////////////////////////////////////////////////////////////////////////
// Input Plugin Functions (from spec's)
// ŻŻŻŻŻŻŻŻŻŻŻŻŻŻŻŻŻŻŻŻŻŻŻŻŻŻŻŻŻŻŻŻŻŻŻŻ

// Get properties of plugin
// ŻŻŻŻŻŻŻŻŻŻŻŻŻŻŻŻŻŻŻŻŻŻŻŻ
void GetDllInfo(PLUGIN_INFO* _PluginInfo)
{
	_PluginInfo->Version = 0x0100;
	_PluginInfo->Type = PLUGIN_TYPE_PAD;

#ifdef DEBUGFAST
	sprintf(_PluginInfo->Name, "nJoy v"INPUT_VERSION" (DebugFast) by Falcon4ever");
#else
#ifndef _DEBUG
	sprintf(_PluginInfo->Name, "nJoy v"INPUT_VERSION " by Falcon4ever");
#else
	sprintf(_PluginInfo->Name, "nJoy v"INPUT_VERSION" (Debug) by Falcon4ever");
#endif
#endif
}

void SetDllGlobals(PLUGIN_GLOBALS* _pPluginGlobals) {}

// Call config dialog
// ŻŻŻŻŻŻŻŻŻŻŻŻŻŻŻŻŻŻ
void DllConfig(HWND _hParent)
{
	// Debugging
	#ifdef SHOW_PAD_STATUS
		Console::Open(100);
		m_hConsole = Console::GetHwnd();
	#endif

	#ifdef _WIN32
		// Start the pads so we can use them in the configuration and advanced controls
		if(!g_EmulatorRunning)
		{
			Search_Devices(joyinfo, NumPads, NumGoodPads); // Populate joyinfo for all attached devices

			// Check if a DirectInput error occured
			if(ReloadDLL())
			{
				PostMessage(_hParent, WM_USER, NJOY_RELOAD, 0);
				return;
			}
		}

		m_frame = new ConfigBox(NULL);
		m_frame->Show();

	#else
		if (SDL_Init(SDL_INIT_JOYSTICK ) < 0)
		{
			printf("Could not initialize SDL! (%s)\n", SDL_GetError());
			return;
		}

		g_Config.Load();	// load settings

		#if defined(HAVE_WX) && HAVE_WX
			ConfigBox frame(NULL);
			frame.ShowModal();
		#endif
	#endif
}

void DllDebugger(HWND _hParent, bool Show) {}

 
// Init PAD (start emulation)
// ŻŻŻŻŻŻŻŻŻŻŻŻŻŻŻŻŻŻŻŻŻŻŻŻŻŻ
/* Information: This function can not be run twice without a Shutdown in between. If
   it's run twice the SDL_Init() will cause a crash. One solution to this is to keep a
   global function that remembers the SDL_Init() and SDL_Quit() (g_EmulatorRunning does
   not do that since we can open and close this without any game running). But I would
   suggest that avoiding to run this twice from the Core is better. */
void Initialize(void *init)
{
	// Debugging
	#ifdef SHOW_PAD_STATUS
		Console::Open(100);
		m_hConsole = Console::GetHwnd();
	#endif
	Console::Print("Initialize: %i\n", SDL_WasInit(0));
    g_PADInitialize = (SPADInitialize*)init;
	g_EmulatorRunning = true;

	#ifdef _DEBUG
		DEBUG_INIT();
	#endif

	#ifdef _WIN32
		m_hWnd = (HWND)g_PADInitialize->hWnd;
	#endif

	// Populate joyinfo for all attached devices if the configuration window is not already open
	if(!m_frame)
	{
		Search_Devices(joyinfo, NumPads, NumGoodPads);
		// Check if a DirectInput error occured
		if(ReloadDLL()) g_PADInitialize->padNumber = -1;
	}
}

bool Search_Devices(std::vector<InputCommon::CONTROLLER_INFO> &_joyinfo, int &_NumPads, int &_NumGoodPads)
{
	bool Success = InputCommon::SearchDevices(_joyinfo, _NumPads, _NumGoodPads);

	// Warn the user if no gamepads are detected
	if (_NumGoodPads == 0 && g_EmulatorRunning)
	{
		PanicAlert("nJoy: No Gamepad Detected");
		return false;
	}

	// Load PadMapping[] etc
	g_Config.Load();

	// Update the PadState[].joy handle
	for (int i = 0; i < 4; i++)
	{
		if (PadMapping[i].enabled && joyinfo.size() > PadMapping[i].ID)
			if(joyinfo.at(PadMapping[i].ID).Good)
				PadState[i].joy = SDL_JoystickOpen(PadMapping[i].ID);
	}

	return Success;
}

// Shutdown PAD (stop emulation)
// ŻŻŻŻŻŻŻŻŻŻŻŻŻŻŻŻŻŻŻŻŻŻŻŻŻŻŻŻŻ
/* Information: This function can not be run twice without an Initialize in between. If
   it's run twice the SDL_...() functions below will cause a crash.
   Called from: The Dolphin Core, ConfigBox::OnClose() */
void Shutdown()
{
	Console::Print("Shutdown: %i\n", SDL_WasInit(0));

	// Always change this variable
	g_EmulatorRunning = false;

	// Don't shutdown if the configuration window is still showing
	if (m_frame) return;

	/* Close all devices carefully. We must check that we are not accessing any undefined
	   vector elements or any bad devices */
	for (int i = 0; i < 4; i++)
	{
		if (PadMapping[i].enabled && joyinfo.size() > PadMapping[i].ID)
			if (joyinfo.at(PadMapping[i].ID).Good)
				if(SDL_JoystickOpened(PadMapping[i].ID))
				{
					SDL_JoystickClose(PadState[i].joy);
					PadState[i].joy = NULL;
				}
	}

	// Clear the physical device info
	joyinfo.clear();
	NumPads = 0;
	NumGoodPads = 0;

	// Finally close SDL
	if (SDL_WasInit(0)) SDL_Quit();

	// Remove the pointer to the initialize data
	g_PADInitialize = NULL;

	#ifdef _DEBUG
		DEBUG_QUIT();
	#endif

	#ifdef _WIN32
		#ifdef USE_RUMBLE_DINPUT_HACK
			FreeDirectInput();
		#endif
	#elif defined(__linux__)
		close(fd);
	#endif
}


// Set buttons status from keyboard input. Currently this is done from wxWidgets in the main application.
// ŻŻŻŻŻŻŻŻŻŻŻŻŻŻ
void PAD_Input(u16 _Key, u8 _UpDown)
{
	// Check that Dolphin is in focus, otherwise don't update the pad status
	if (!IsFocus()) return;

	// Check if the keys are interesting, and then update it
	for(int i = 0; i < 4; i++)
	{
		for(int j = InputCommon::CTL_L_SHOULDER; j <= InputCommon::CTL_START; j++)
		{
			if (PadMapping[i].buttons[j] == _Key)
				{ PadState[i].buttons[j] = _UpDown; break; }
		}

		for(int j = InputCommon::CTL_D_PAD_UP; j <= InputCommon::CTL_D_PAD_RIGHT; j++)
		{
			if (PadMapping[i].dpad2[j] == _Key)
				{ PadState[i].dpad2[j] = _UpDown; break; }
		}
	}

	// Debugging
	//Console::Print("%i", _Key);
}


// Save state
// ŻŻŻŻŻŻŻŻŻŻŻŻŻŻ
void DoState(unsigned char **ptr, int mode) {}

 
// Set PAD attached pads
// ŻŻŻŻŻŻŻŻŻŻŻŻŻŻŻŻŻŻŻŻŻ
unsigned int PAD_GetAttachedPads()
{
	unsigned int connected = 0;

	g_Config.Load();

	if (PadMapping[0].enabled) connected |= 1;		
	if (PadMapping[1].enabled) connected |= 2;
	if (PadMapping[2].enabled) connected |= 4;
	if (PadMapping[3].enabled) connected |= 8;

	//Console::Print("PAD_GetAttachedPads: %i %i %i %i\n", PadMapping[0].enabled, PadMapping[1].enabled, PadMapping[2].enabled, PadMapping[3].enabled);

	return connected;
}


// Set PAD status
// ŻŻŻŻŻŻŻŻŻŻŻŻŻŻ
// Called from: SI_DeviceGCController.cpp
// Function: Gives the current pad status to the Core
void PAD_GetStatus(u8 _numPAD, SPADStatus* _pPADStatus)
{
	//Console::Print("PAD_GetStatus(): %i %i %i\n", _numPAD, PadMapping[_numPAD].enabled, PadState[_numPAD].joy);

	/* Check if the pad is enabled and avaliable, currently we don't disable pads just because they are
	   disconnected */
	if (!PadMapping[_numPAD].enabled || !PadState[_numPAD].joy) return;

	// Clear pad status
	memset(_pPADStatus, 0, sizeof(SPADStatus));

	// Check that Dolphin is in focus, otherwise don't update the pad status
	if (g_Config.bCheckFocus || IsFocus())
		GetJoyState(PadState[_numPAD], PadMapping[_numPAD], _numPAD, joyinfo[PadMapping[_numPAD].ID].NumButtons);

	// Get type
	int TriggerType = PadMapping[_numPAD].triggertype;
 
	///////////////////////////////////////////////////
	// The analog controls
	// -----------

	// Read axis values
	int i_main_stick_x = PadState[_numPAD].axis[InputCommon::CTL_MAIN_X];
	int i_main_stick_y = -PadState[_numPAD].axis[InputCommon::CTL_MAIN_Y];
    int i_sub_stick_x = PadState[_numPAD].axis[InputCommon::CTL_SUB_X];
	int i_sub_stick_y = -PadState[_numPAD].axis[InputCommon::CTL_SUB_Y];
	int TriggerLeft = PadState[_numPAD].axis[InputCommon::CTL_L_SHOULDER];
	int TriggerRight = PadState[_numPAD].axis[InputCommon::CTL_R_SHOULDER];

	// Check if we should make adjustments
	if(PadMapping[_numPAD].bSquareToCircle)
	{
		std::vector<int> main_xy = InputCommon::Square2Circle(i_main_stick_x, i_main_stick_y, _numPAD, PadMapping[_numPAD].SDiagonal);
		i_main_stick_x = main_xy.at(0);
		i_main_stick_y = main_xy.at(1);
	}

	// Convert axis values
	u8 main_stick_x = InputCommon::Pad_Convert(i_main_stick_x);
	u8 main_stick_y = InputCommon::Pad_Convert(i_main_stick_y);
    u8 sub_stick_x = InputCommon::Pad_Convert(i_sub_stick_x);
	u8 sub_stick_y = InputCommon::Pad_Convert(i_sub_stick_y);

	// Convert the triggers values, if we are using analog triggers at all
	if(PadMapping[_numPAD].triggertype == InputCommon::CTL_TRIGGER_SDL)
	{
		if(PadMapping[_numPAD].buttons[InputCommon::CTL_L_SHOULDER] >= 1000) TriggerLeft = InputCommon::Pad_Convert(TriggerLeft);
		if(PadMapping[_numPAD].buttons[InputCommon::CTL_R_SHOULDER] >= 1000) TriggerRight = InputCommon::Pad_Convert(TriggerRight);
	}

	// Set Deadzones (perhaps out of function?)
	int deadzone = (int)(((float)(128.00/100.00)) * (float)(PadMapping[_numPAD].deadzone + 1));
	int deadzone2 = (int)(((float)(-128.00/100.00)) * (float)(PadMapping[_numPAD].deadzone + 1));

	// Send values to Dolpin if they are outside the deadzone
	if ((main_stick_x < deadzone2)	|| (main_stick_x > deadzone))	_pPADStatus->stickX = main_stick_x;
	if ((main_stick_y < deadzone2)	|| (main_stick_y > deadzone))	_pPADStatus->stickY = main_stick_y;
	if ((sub_stick_x < deadzone2)	|| (sub_stick_x > deadzone))	_pPADStatus->substickX = sub_stick_x;
	if ((sub_stick_y < deadzone2)	|| (sub_stick_y > deadzone))	_pPADStatus->substickY = sub_stick_y;

 
	///////////////////////////////////////////////////
	// The L and R triggers
	// -----------	
	int TriggerValue = 255;
	if (PadState[_numPAD].halfpress) TriggerValue = 100;

	_pPADStatus->button |= PAD_USE_ORIGIN; // Neutral value, no button pressed	

	// Check if the digital L button is pressed
	if (PadState[_numPAD].buttons[InputCommon::CTL_L_SHOULDER])
	{
		_pPADStatus->button |= PAD_TRIGGER_L;
		_pPADStatus->triggerLeft = TriggerValue;
	}
	else if(TriggerLeft > 0)
		_pPADStatus->triggerLeft = TriggerLeft;

	// Check if the digital R button is pressed
	if (PadState[_numPAD].buttons[InputCommon::CTL_R_SHOULDER])
	{
		_pPADStatus->button |= PAD_TRIGGER_R;
		_pPADStatus->triggerRight = TriggerValue;
	}
	else if(TriggerRight > 0)
		_pPADStatus->triggerRight = TriggerRight;

	// Update the buttons in analog mode to
	if(TriggerLeft == 0xff) _pPADStatus->button |= PAD_TRIGGER_L;
	if(TriggerRight == 0xff) _pPADStatus->button |= PAD_TRIGGER_R;


	///////////////////////////////////////////////////
	// The digital buttons
	// -----------	
	if (PadState[_numPAD].buttons[InputCommon::CTL_A_BUTTON])
	{
		_pPADStatus->button |= PAD_BUTTON_A;
		_pPADStatus->analogA = 255;			// Perhaps support pressure?
	}
	if (PadState[_numPAD].buttons[InputCommon::CTL_B_BUTTON])
	{
		_pPADStatus->button |= PAD_BUTTON_B;
		_pPADStatus->analogB = 255;			// Perhaps support pressure?
	}
	if (PadState[_numPAD].buttons[InputCommon::CTL_X_BUTTON])	_pPADStatus->button|=PAD_BUTTON_X;
	if (PadState[_numPAD].buttons[InputCommon::CTL_Y_BUTTON])	_pPADStatus->button|=PAD_BUTTON_Y;
	if (PadState[_numPAD].buttons[InputCommon::CTL_Z_TRIGGER])	_pPADStatus->button|=PAD_TRIGGER_Z;
	if (PadState[_numPAD].buttons[InputCommon::CTL_START])		_pPADStatus->button|=PAD_BUTTON_START;


	///////////////////////////////////////////////////
	// The D-pad
	// -----------		
	if (PadMapping[_numPAD].controllertype == InputCommon::CTL_DPAD_HAT)
	{
		if (PadState[_numPAD].dpad == SDL_HAT_LEFTUP	|| PadState[_numPAD].dpad == SDL_HAT_UP		|| PadState[_numPAD].dpad == SDL_HAT_RIGHTUP )		_pPADStatus->button|=PAD_BUTTON_UP;
		if (PadState[_numPAD].dpad == SDL_HAT_LEFTUP	|| PadState[_numPAD].dpad == SDL_HAT_LEFT	|| PadState[_numPAD].dpad == SDL_HAT_LEFTDOWN )		_pPADStatus->button|=PAD_BUTTON_LEFT;
		if (PadState[_numPAD].dpad == SDL_HAT_LEFTDOWN	|| PadState[_numPAD].dpad == SDL_HAT_DOWN	|| PadState[_numPAD].dpad == SDL_HAT_RIGHTDOWN )	_pPADStatus->button|=PAD_BUTTON_DOWN;
		if (PadState[_numPAD].dpad == SDL_HAT_RIGHTUP	|| PadState[_numPAD].dpad == SDL_HAT_RIGHT	|| PadState[_numPAD].dpad == SDL_HAT_RIGHTDOWN )	_pPADStatus->button|=PAD_BUTTON_RIGHT;
	}
	else
	{
		if (PadState[_numPAD].dpad2[InputCommon::CTL_D_PAD_UP])
			_pPADStatus->button |= PAD_BUTTON_UP;
		if (PadState[_numPAD].dpad2[InputCommon::CTL_D_PAD_DOWN])
			_pPADStatus->button |= PAD_BUTTON_DOWN;
		if (PadState[_numPAD].dpad2[InputCommon::CTL_D_PAD_LEFT])
			_pPADStatus->button |= PAD_BUTTON_LEFT;
		if (PadState[_numPAD].dpad2[InputCommon::CTL_D_PAD_RIGHT])
			_pPADStatus->button |= PAD_BUTTON_RIGHT;
	}

	// Update error code
	_pPADStatus->err = PAD_ERR_NONE;

	// Use rumble
	Pad_Use_Rumble(_numPAD, _pPADStatus);

	// Debugging 
	/*
	// Show the status of all connected pads
	if ((LastPad == 0 && _numPAD == 0) || _numPAD < LastPad) Console::ClearScreen();	
	LastPad = _numPAD;
	Console::Print(
		"Pad        | Number:%i Enabled:%i Handle:%i\n"
		"Trigger    | StatusLeft:%04x StatusRight:%04x  TriggerLeft:%04x TriggerRight:%04x  TriggerValue:%i\n"
		"Buttons    | Overall:%i  X:%i\n"
		"======================================================\n",

		_numPAD, PadMapping[_numPAD].enabled, PadState[_numPAD].joy,

		PadState[_numPAD].buttons[InputCommon::CTL_L_SHOULDER], PadState[_numPAD].buttons[InputCommon::CTL_R_SHOULDER], PadState[_numPAD].halfpress,

		(PadMapping[_numPAD].triggertype ? "CTL_TRIGGER_XINPUT" : "CTL_TRIGGER_SDL"),
			_pPADStatus->triggerLeft, _pPADStatus->triggerRight,  TriggerLeft, TriggerRight,  TriggerValue,

		_pPADStatus->button, PadState[_numPAD].buttons[InputCommon::CTL_X_BUTTON]
		);
	*/
}


///////////////////////////////////////////////// Spec functions


//******************************************************************************
// Supporting functions
//******************************************************************************

//////////////////////////////////////////////////////////////////////////////////////////
/* Check if any of the pads failed to open. In Windows there is a strange "IDirectInputDevice2::
   SetDataFormat() DirectX error -2147024809" after exactly four SDL_Init() and SDL_Quit() */
// ŻŻŻŻŻŻŻŻŻŻŻŻŻŻŻŻ
bool ReloadDLL()
{
	if (   (PadMapping[0].enabled && PadState[0].joy == NULL)
		|| (PadMapping[1].enabled && PadState[1].joy == NULL)
		|| (PadMapping[2].enabled && PadState[2].joy == NULL)
		|| (PadMapping[3].enabled && PadState[3].joy == NULL))
	{
		// Check if it was an error and not just no pads connected
		std::string StrError = SDL_GetError();
		if (StrError.find("IDirectInputDevice2") != std::string::npos)
		{
			// Clear the physical device info
			joyinfo.clear();
			NumPads = 0;
			NumGoodPads = 0;
			// Close SDL
			if (SDL_WasInit(0)) SDL_Quit();
			// Log message
			Console::Print("Error: %s\n", StrError.c_str());	
			return true;
		}
	}

	return false;
}

//////////////////////////////////////////////////////////////////////////////////////////
// Check if Dolphin is in focus
// ŻŻŻŻŻŻŻŻŻŻŻŻŻŻŻŻ
bool IsFocus()
{
return true;

#ifdef _WIN32
	HWND RenderingWindow = NULL; if (g_PADInitialize) RenderingWindow = g_PADInitialize->hWnd;
	HWND Parent = GetParent(RenderingWindow);
	HWND TopLevel = GetParent(Parent);
	HWND Config = NULL; if (m_frame) Config = (HWND)m_frame->GetHWND();
	// Support both rendering to main window and not, and the config and eventual console window
	if (GetForegroundWindow() == TopLevel || GetForegroundWindow() == RenderingWindow || GetForegroundWindow() == Config || GetForegroundWindow() == m_hConsole)
		return true;
	else
		return false;
#else
	return true;
#endif
}




 

