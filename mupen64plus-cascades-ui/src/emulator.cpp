#include <string>
#include <iostream>

#include "emulator.h"
#include <dlfcn.h>

#ifdef __cplusplus
extern "C" {
#endif
#include "cheat.h"
#include "main.h"
#include "plugin.h"
#include "version.h"
#include "core_interface.h"
#include "compare_core.h"
#include "osal_preproc.h"
#include "bbutil.h"
#ifdef __cplusplus
}
#endif

//#include "ui-sdl.h"
//#include "SDL.h"
#include <screen/screen.h>

using namespace std;

enum VIDEO_PLUGIN
{
	VIDEO_PLUGIN_RICE,
	VIDEO_PLUGIN_GLES2N64
};

const char *button_names[] = {
    "DPad R",       // R_DPAD
    "DPad L",       // L_DPAD
    "DPad D",       // D_DPAD
    "DPad U",       // U_DPAD
    "Start",        // START_BUTTON
    "Z Trig",       // Z_TRIG
    "B Button",     // B_BUTTON
    "A Button",     // A_BUTTON
    "C Button R",   // R_CBUTTON
    "C Button L",   // L_CBUTTON
    "C Button D",   // D_CBUTTON
    "C Button U",   // U_CBUTTON
    "R Trig",       // R_TRIG
    "L Trig",       // L_TRIG
    "Mempak switch",
    "Rumblepak switch",
    "X Axis",       // X_AXIS
    "Y Axis"        // Y_AXIS
};

/** global variables **/
int    g_Verbose = 0;

/** static (local) variables **/
static m64p_handle l_ConfigCore = NULL;
static m64p_handle l_ConfigVideo = NULL;
static m64p_handle l_ConfigUI = NULL;

static const char *l_CoreLibPath = NULL;
static const char *l_ConfigDirPath = NULL;
static const char *l_ROMFilepath = NULL;       // filepath of ROM to load & run at startup

#if defined(SHAREDIR)
  static const char *l_DataDirPath = SHAREDIR;
#else
  static const char *l_DataDirPath = NULL;
#endif

static int   l_SaveOptions = 0;          // save command-line options in configuration file
static int   l_CoreCompareMode = 0;      // 0 = disable, 1 = send, 2 = receive

static eCheatMode l_CheatMode = CHEAT_LIST;

screen_context_t screen_cxt;

static char *g_groupId;
static char *g_windowId;
//void zeldaSubscreenHack();
static m64p_error OpenConfigurationHandles(void);
static m64p_error SaveConfigurationOptions(void);
void DebugCallback(void *Context, int level, const char *message);

Emulator::Emulator(char * groupId, char * windowId){
	g_groupId = strdup(groupId);
	g_windowId = strdup(windowId);

    l_CoreLibPath = "app/native/lib/libmupen64plus.so.2";
    l_ConfigDirPath = "shared/misc/n64/data/";
    l_DataDirPath = "app/native/";
    /*
	g_GfxPlugin = "libmupen64plus-video-rice";
	g_AudioPlugin = "mupen64plus-audio-sdl";
	g_InputPlugin = "libmupen64plus-input-sdl";
	g_RspPlugin = "libmupen64plus-rsp-hle";
	*/

    g_GfxPlugin = NULL;
	g_AudioPlugin = NULL;
	g_InputPlugin = NULL;
	g_RspPlugin = NULL;

    /* load the Mupen64Plus core library */
    if (AttachCoreLib(l_CoreLibPath) != M64ERR_SUCCESS){
        return;
    }

    /* start the Mupen64Plus core library, load the configuration file */
    m64p_error rval = (*CoreStartup)(CONSOLE_API_VERSION, l_ConfigDirPath, l_DataDirPath, (void*)"Core", DebugCallback, NULL, NULL);
    if (rval != M64ERR_SUCCESS)
    {
        printf("UI-console: error starting Mupen64Plus core library.\n");
        DetachCoreLib();
        return;
    }

    /* Open configuration sections */
    rval = OpenConfigurationHandles();
    if (rval != M64ERR_SUCCESS)
    {
        (*CoreShutdown)();
        DetachCoreLib();
        return;
    }

    init_controller_config();

    for(int i=0; i<=3; ++i){
    	char tmp[20];
    	sprintf(tmp, "Input-SDL-Control%d", i+1);
    	load_controller_config(tmp, i);
    }

    //print_controller_config();
}

Emulator::~Emulator(){
	/*
	if(g_groupId)
		free(g_groupId);
	if(l_ROMFilepath)
		free((void*)l_ROMFilepath);
		*/
}

void Emulator::SetRom(const char * rom ){
	printf("Setting Rom: %s\n", rom);fflush(stdout);
	l_ROMFilepath = strdup(rom);
}

int Emulator::LoadRom(){
	static bool romOpened;
/* load ROM image */
	printf("Loading Rom...\n");fflush(stdout);

	if(romOpened){
		(*CoreDoCommand)(M64CMD_ROM_CLOSE, 0, NULL);
	}

	FILE *fPtr = fopen(l_ROMFilepath, "rb");
	if (fPtr == NULL)
	{
		fprintf(stderr, "Error: couldn't open ROM file '%s' for reading.\n", l_ROMFilepath);
		(*CoreShutdown)();
		DetachCoreLib();
		return 7;
	}

	/* get the length of the ROM, allocate memory buffer, load it from disk */
	long romlength = 0;
	fseek(fPtr, 0L, SEEK_END);
	romlength = ftell(fPtr);
	fseek(fPtr, 0L, SEEK_SET);
	unsigned char *ROM_buffer = (unsigned char *) malloc(romlength);
	if (ROM_buffer == NULL)
	{
		fprintf(stderr, "Error: couldn't allocate %li-byte buffer for ROM image file '%s'.\n", romlength, l_ROMFilepath);
		fclose(fPtr);
		(*CoreShutdown)();
		DetachCoreLib();
		return 8;
	}
	else if (fread(ROM_buffer, 1, romlength, fPtr) != romlength)
	{
		fprintf(stderr, "Error: couldn't read %li bytes from ROM image file '%s'.\n", romlength, l_ROMFilepath);
		free(ROM_buffer);
		fclose(fPtr);
		(*CoreShutdown)();
		DetachCoreLib();
		return 9;
	}
	fclose(fPtr);

	/* Try to load the ROM image into the core */
	if ((*CoreDoCommand)(M64CMD_ROM_OPEN, (int) romlength, ROM_buffer) != M64ERR_SUCCESS)
	{
		fprintf(stderr, "Error: core failed to open ROM image file '%s'.\n", l_ROMFilepath);
		free(ROM_buffer);
		(*CoreShutdown)();
		DetachCoreLib();
		return 10;
	}
	free(ROM_buffer); /* the core copies the ROM image, so we can release this buffer immediately */
	fflush(stderr);
	romOpened = 1;

	return 0;
}

int Emulator::Start(){
	int rc = -1;


	if(bbutil_init_egl(screen_cxt, g_groupId, g_windowId) != 0){
		printf("Error initializing EGL...\n");fflush(stdout);
		return -1;
	}

	printf("Finished EGl init...\n");fflush(stdout);

	if( BPS_SUCCESS != dialog_request_events(0) ){
		printf("Error Initializing Dialog events...\n");
		return -1;
	}

	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT);
	PB_eglSwapBuffers();

	g_PluginDir = "app/native/lib";

	int emumode = 2;

	(*ConfigSetParameter)(l_ConfigCore, "R4300Emulator", M64TYPE_INT, &emumode);
	l_SaveOptions = 1;
	//l_ROMFilepath = argv[i];

	/* Handle the core comparison feature */
	if (l_CoreCompareMode != 0 && !(g_CoreCapabilities & M64CAPS_CORE_COMPARE))
	{
		printf("UI-console: can't use --core-compare feature with this Mupen64Plus core library.\n");
		DetachCoreLib();
		return 6;
	}
	compare_core_init(l_CoreCompareMode);

	/* save the given command-line options in configuration file if requested */
	if (l_SaveOptions)
		SaveConfigurationOptions();

	//this->LoadRom();

	/* handle the cheat codes */
	//zeldaSubscreenHack();
	CheatStart(l_CheatMode, l_CheatNumList);CheatFreeAll();//HACK to have access for cheat menu
	if (l_CheatMode == CHEAT_SHOW_LIST)
	{
		(*CoreDoCommand)(M64CMD_ROM_CLOSE, 0, NULL);
		(*CoreShutdown)();
		DetachCoreLib();
		return 11;
	}

	/* search for and load plugins */
	int rval = PluginSearchLoad(l_ConfigUI);
	if (rval != M64ERR_SUCCESS)
	{
		(*CoreDoCommand)(M64CMD_ROM_CLOSE, 0, NULL);
		(*CoreShutdown)();
		DetachCoreLib();
		return 12;
	}

	/* attach plugins to core */
	for (int i = 0; i < 4; i++)
	{
		if ((*CoreAttachPlugin)(g_PluginMap[i].type, g_PluginMap[i].handle) != M64ERR_SUCCESS)
		{
			fprintf(stderr, "UI-Console: error from core while attaching %s plugin.\n", g_PluginMap[i].name);
			(*CoreDoCommand)(M64CMD_ROM_CLOSE, 0, NULL);
			(*CoreShutdown)();
			DetachCoreLib();
			return 13;
		}
	}

	/* run the game */
	(*CoreDoCommand)(M64CMD_EXECUTE, 0, NULL);

	/* detach plugins from core and unload them */
	for (int i = 0; i < 4; i++){
		(*CoreDetachPlugin)(g_PluginMap[i].type);
	}
	PluginUnload();
	printf("Unloaded Plugins...\n");fflush(stdout);

	/* close the ROM image */
	(*CoreDoCommand)(M64CMD_ROM_CLOSE, 0, NULL);
	printf("Closed Rom...\n");fflush(stdout);

	/* Shut down and release the Core library */
	//(*CoreShutdown)();
	//DetachCoreLib();

	printf("Return from EMU...\n");fflush(stdout);
	return 0;
}

void DebugCallback(void *Context, int level, const char *message)
{
    if (level <= 1)
        printf("%s Error: %s\n", (const char *) Context, message);
    else if (level == 2)
        printf("%s Warning: %s\n", (const char *) Context, message);
    else if (level == 3 || (level == 5 && g_Verbose))
        printf("%s: %s\n", (const char *) Context, message);
    else if (level == 4)
        printf("%s Status: %s\n", (const char *) Context, message);
    /* ignore the verbose info for now */
    fflush(stdout);
}

/*********************************************************************************************************
 *  Configuration handling
 */

static m64p_error OpenConfigurationHandles(void)
{
    m64p_error rval;

    /* Open Configuration sections for core library and console User Interface */
    rval = (*ConfigOpenSection)("Core", &l_ConfigCore);
    if (rval != M64ERR_SUCCESS)
    {
        fprintf(stderr, "Error: failed to open 'Core' configuration section\n");
        return rval;
    }

    rval = (*ConfigOpenSection)("Video-General", &l_ConfigVideo);
    if (rval != M64ERR_SUCCESS)
    {
        fprintf(stderr, "Error: failed to open 'Video-General' configuration section\n");
        return rval;
    }

    rval = (*ConfigOpenSection)("UI-Console", &l_ConfigUI);
    if (rval != M64ERR_SUCCESS)
    {
        fprintf(stderr, "Error: failed to open 'UI-Console' configuration section\n");
        return rval;
    }

    /* Set default values for my Config parameters */
    (*ConfigSetDefaultString)(l_ConfigUI, "PluginDir", OSAL_CURRENT_DIR, "Directory in which to search for plugins");
    (*ConfigSetDefaultString)(l_ConfigUI, "VideoPlugin", "mupen64plus-video-rice" OSAL_DLL_EXTENSION, "Filename of video plugin");
    (*ConfigSetDefaultString)(l_ConfigUI, "AudioPlugin", "mupen64plus-audio-sdl" OSAL_DLL_EXTENSION, "Filename of audio plugin");
    (*ConfigSetDefaultString)(l_ConfigUI, "InputPlugin", "mupen64plus-input-sdl" OSAL_DLL_EXTENSION, "Filename of input plugin");
    (*ConfigSetDefaultString)(l_ConfigUI, "RspPlugin", "mupen64plus-rsp-hle" OSAL_DLL_EXTENSION, "Filename of RSP plugin");

    return M64ERR_SUCCESS;
}

static m64p_error SaveConfigurationOptions(void)
{
    /* if shared data directory was given on the command line, write it into the config file */
    if (l_DataDirPath != NULL)
        (*ConfigSetParameter)(l_ConfigCore, "SharedDataPath", M64TYPE_STRING, l_DataDirPath);

    /* if any plugin filepaths were given on the command line, write them into the config file */
    if (g_PluginDir != NULL)
        (*ConfigSetParameter)(l_ConfigUI, "PluginDir", M64TYPE_STRING, g_PluginDir);
    if (g_GfxPlugin != NULL)
        (*ConfigSetParameter)(l_ConfigUI, "VideoPlugin", M64TYPE_STRING, g_GfxPlugin);
    if (g_AudioPlugin != NULL)
        (*ConfigSetParameter)(l_ConfigUI, "AudioPlugin", M64TYPE_STRING, g_AudioPlugin);
    if (g_InputPlugin != NULL)
        (*ConfigSetParameter)(l_ConfigUI, "InputPlugin", M64TYPE_STRING, g_InputPlugin);
    if (g_RspPlugin != NULL)
        (*ConfigSetParameter)(l_ConfigUI, "RspPlugin", M64TYPE_STRING, g_RspPlugin);

    return (*ConfigSaveFile)();
}

int Emulator::SetConfigParameter(string ParamSpec)
{
    char *ParsedString, *VarName, *VarValue;
    m64p_handle ConfigSection;
    m64p_type VarType;
    m64p_error rval;

    if (ParamSpec.c_str() == NULL)
    {
        fprintf(stderr, "UI-Console Error: ParamSpec is NULL in SetConfigParameter()\n");
        return 1;
    }

    /* make a copy of the input string */
    ParsedString = (char *) malloc(strlen(ParamSpec.c_str()) + 1);
    if (ParsedString == NULL)
    {
        fprintf(stderr, "UI-Console Error: SetConfigParameter() couldn't allocate memory for temporary string.\n");
        return 2;
    }
    strcpy(ParsedString, ParamSpec.c_str());

    /* parse it for the simple section[name]=value format */
    VarName = strchr(ParsedString, '[');
    if (VarName != NULL)
    {
        *VarName++ = 0;
        VarValue = strchr(VarName, ']');
        if (VarValue != NULL)
        {
            *VarValue++ = 0;
        }
    }
    if (VarName == NULL || VarValue == NULL || *VarValue != '=')
    {
        fprintf(stderr, "UI-Console Error: invalid (param-spec) '%s'\n", ParamSpec.c_str());
        free(ParsedString);
        return 3;
    }
    VarValue++;

    /* then set the value */
	rval = (*ConfigOpenSection)(ParsedString, &ConfigSection);
	if (rval != M64ERR_SUCCESS)
	{
		fprintf(stderr, "UI-Console Error: SetConfigParameter failed to open config section '%s'\n", ParsedString);
		free(ParsedString);
		return 4;
	}
	if ((*ConfigGetParameterType)(ConfigSection, VarName, &VarType) == M64ERR_SUCCESS)
	{
		switch(VarType)
		{
			int ValueInt;
			float ValueFloat;
			case M64TYPE_INT:
				ValueInt = atoi(VarValue);
				ConfigSetParameter(ConfigSection, VarName, M64TYPE_INT, &ValueInt);
				break;
			case M64TYPE_FLOAT:
				ValueFloat = (float) atof(VarValue);
				ConfigSetParameter(ConfigSection, VarName, M64TYPE_FLOAT, &ValueFloat);
				break;
			case M64TYPE_BOOL:
				ValueInt = (int) (osal_insensitive_strcmp(VarValue, "true") == 0);
				ConfigSetParameter(ConfigSection, VarName, M64TYPE_BOOL, &ValueInt);
				break;
			case M64TYPE_STRING:
				ConfigSetParameter(ConfigSection, VarName, M64TYPE_STRING, VarValue);
				break;
			default:
				fprintf(stderr, "UI-Console Error: invalid VarType in SetConfigParameter()\n");
				return 5;
		}
	}
	else
	{
		ConfigSetParameter(ConfigSection, VarName, M64TYPE_STRING, VarValue);
	}

    free(ParsedString);
    return 0;
}

//Input Stuff
int Emulator::init_controller_config() {
	int i, j;

	for(i=0; i<4; ++i){
		controller[i].present = 0;
		controller[i].plugin = 0;
		controller[i].device = 0;
		controller[i].mouse = 0;
		controller[i].layout = 1;

		for(j=0; j<16; ++j){
			controller[i].button[j] = -1;
		}
		for(j=0; j<2; ++j){
			controller[i].axis[j].a = -1;
			controller[i].axis[j].b = -1;
		}

		for(j=0; j<2; ++j){
			controller[i].analogDeadZone[j] = 0;
			controller[i].analogPeak[2] = 0;
		}
	}
}

int Emulator::print_controller_config() {
	int i, j;

	for(i=0; i<4; ++i){
		printf("Controller %d::", i);
		printf("Preset: %d\n", controller[i].present);
		printf("Plugin: %d\n", controller[i].plugin);
		printf("Device: %d\n", controller[i].device);
		printf("Mouse: %d\n", controller[i].mouse);
		printf("Layout: %d\n", controller[i].layout);

		for(j=0; j<16; ++j){
			printf("Button %d: %d\n", j, controller[i].button[j]);
		}
		for(j=0; j<2; ++j){
			printf("Axis a%d: %d\n", j, controller[i].axis[j].a);
			printf("Axis b%d: %d\n", j, controller[i].axis[j].b);
		}

		for(j=0; j<2; ++j){
			printf("DeadZone%d: %d\n", j, controller[i].analogDeadZone[j]);
			printf("AnalogPeak%d: %d\n", j, controller[i].analogPeak[j]);
		}
	}
}

int Emulator::load_controller_config(const char *SectionName, int i) {
    m64p_handle pConfig;
    char input_str[256], value1_str[16], value2_str[16];
    const char *config_ptr;
    int readOK, j;

    /* Open the configuration section for this controller */
    if (ConfigOpenSection(SectionName, &pConfig) != M64ERR_SUCCESS)
    {
        //DebugMessage(M64MSG_ERROR, "Couldn't open config section '%s'", SectionName);
    	printf("Couldn't open config section: %s\n", SectionName);
        return 0;
    }
    /* try to read all of the configuration values */
    for (readOK = 0; readOK == 0; readOK = 1)
    {
        /* check for the required parameters */
        if (ConfigGetParameter(pConfig, "plugged", M64TYPE_BOOL, &controller[i].present, sizeof(int)) != M64ERR_SUCCESS)
            break;
        if (ConfigGetParameter(pConfig, "plugin", M64TYPE_INT, &controller[i].plugin, sizeof(int)) != M64ERR_SUCCESS)
            break;
        if (ConfigGetParameter(pConfig, "device", M64TYPE_INT, &controller[i].device, sizeof(int)) != M64ERR_SUCCESS)
            break;
        /* then do the optional parameters */
        ConfigGetParameter(pConfig, "mouse", M64TYPE_BOOL, &controller[i].mouse, sizeof(int));
        if (ConfigGetParameter(pConfig, "layout", M64TYPE_INT, &controller[i].layout, sizeof(int)) != M64ERR_SUCCESS)
                    break;
        if (ConfigGetParameter(pConfig, "AnalogDeadzone", M64TYPE_STRING, input_str, 256) == M64ERR_SUCCESS)
        {
            if (sscanf(input_str, "%i,%i", &controller[i].analogDeadZone[0], &controller[i].analogDeadZone[1]) != 2)
            	printf("parsing error in AnalogDeadzone parameter for controller %i\n", i + 1);
            	//DebugMessage(M64MSG_WARNING, "parsing error in AnalogDeadzone parameter for controller %i", i + 1);
        }
        if (ConfigGetParameter(pConfig, "AnalogPeak", M64TYPE_STRING, input_str, 256) == M64ERR_SUCCESS)
        {
            if (sscanf(input_str, "%i,%i", &controller[i].analogPeak[0], &controller[i].analogPeak[1]) != 2)
            	printf("parsing error in AnalogPeak parameter for controller %i\n", i + 1);
                //DebugMessage(M64MSG_WARNING, "parsing error in AnalogPeak parameter for controller %i", i + 1);
        }
        /* load configuration for all the digital buttons */
        for (j = 0; j < 16; j++)
        {
            if (ConfigGetParameter(pConfig, button_names[j], M64TYPE_STRING, input_str, 256) != M64ERR_SUCCESS)
                continue;
            if ((config_ptr = strstr(input_str, "key")) != NULL)
                if (sscanf(config_ptr, "key(%i)", (int *) &controller[i].button[j]) != 1)
                	printf("parsing error in key() parameter of button '%s' for controller %i\n", button_names[j], i + 1);
                    //DebugMessage(M64MSG_WARNING, "parsing error in key() parameter of button '%s' for controller %i", button_names[j], i + 1);
            //if ((config_ptr = strstr(input_str, "button")) != NULL)
                //if (sscanf(config_ptr, "button(%i)", &controller[i].button[j].button) != 1)
                    //DebugMessage(M64MSG_WARNING, "parsing error in button() parameter of button '%s' for controller %i", button_names[j], i + 1);
            //if ((config_ptr = strstr(input_str, "axis")) != NULL)
            //{
                //char chAxisDir;
                //if (sscanf(config_ptr, "axis(%d%c,%d", &controller[i].button[j].axis, &chAxisDir, &controller[i].button[j].axis_deadzone) != 3 &&
                    //sscanf(config_ptr, "axis(%i%c", &controller[i].button[j].axis, &chAxisDir) != 2)
                    //DebugMessage(M64MSG_WARNING, "parsing error in axis() parameter of button '%s' for controller %i", button_names[j], i + 1);
                //controller[i].button[j].axis_dir = (chAxisDir == '+' ? 1 : (chAxisDir == '-' ? -1 : 0));
            //}
            //if ((config_ptr = strstr(input_str, "hat")) != NULL)
            //{
                //char *lastchar = NULL;
                //if (sscanf(config_ptr, "hat(%i %15s", &controller[i].button[j].hat, value1_str) != 2)
                    //DebugMessage(M64MSG_WARNING, "parsing error in hat() parameter of button '%s' for controller %i", button_names[j], i + 1);
                //value1_str[15] = 0;
                /* chop off the last character of value1_str if it is the closing parenthesis */
                //lastchar = &value1_str[strlen(value1_str) - 1];
                //if (lastchar > value1_str && *lastchar == ')') *lastchar = 0;
                //controller[i].button[j].hat_pos = get_hat_pos_by_name(value1_str);
            //}
            //if ((config_ptr = strstr(input_str, "mouse")) != NULL)
                //if (sscanf(config_ptr, "mouse(%i)", &controller[i].button[j].mouse) != 1)
                    //DebugMessage(M64MSG_WARNING, "parsing error in mouse() parameter of button '%s' for controller %i", button_names[j], i + 1);
        }
        /* load configuration for the 2 analog joystick axes */
        for (j = 0; j < 2; j++)
        {
            //int axis_idx = j - 16;
            if (ConfigGetParameter(pConfig, button_names[j+16], M64TYPE_STRING, input_str, 256) != M64ERR_SUCCESS)
                continue;
            if ((config_ptr = strstr(input_str, "key")) != NULL)
                if (sscanf(config_ptr, "key(%i,%i)", (int *) &controller[i].axis[j].a, (int *) &controller[i].axis[j].b) != 2)
                	printf("parsing error in key() parameter of axis '%s' for controller %i", button_names[j+16], i + 1);
                    //DebugMessage(M64MSG_WARNING, "parsing error in key() parameter of axis '%s' for controller %i", button_names[j], i + 1);
            //if ((config_ptr = strstr(input_str, "button")) != NULL)
                //if (sscanf(config_ptr, "button(%i,%i)", &controller[i].axis[axis_idx].button_a, &controller[i].axis[axis_idx].button_b) != 2)
                    //DebugMessage(M64MSG_WARNING, "parsing error in button() parameter of axis '%s' for controller %i", button_names[j], i + 1);
            //if ((config_ptr = strstr(input_str, "axis")) != NULL)
            //{
                //char chAxisDir1, chAxisDir2;
                //if (sscanf(config_ptr, "axis(%i%c,%i%c)", &controller[i].axis[axis_idx].axis_a, &chAxisDir1,
                                                          //&controller[i].axis[axis_idx].axis_b, &chAxisDir2) != 4)
                    //DebugMessage(M64MSG_WARNING, "parsing error in axis() parameter of axis '%s' for controller %i", button_names[j], i + 1);
                //controller[i].axis[axis_idx].axis_dir_a = (chAxisDir1 == '+' ? 1 : (chAxisDir1 == '-' ? -1 : 0));
                //controller[i].axis[axis_idx].axis_dir_b = (chAxisDir2 == '+' ? 1 : (chAxisDir2 == '-' ? -1 : 0));
            //}
            //if ((config_ptr = strstr(input_str, "hat")) != NULL)
            //{
                //char *lastchar = NULL;
                //if (sscanf(config_ptr, "hat(%i %15s %15s", &controller[i].axis[axis_idx].hat, value1_str, value2_str) != 3)
                    //DebugMessage(M64MSG_WARNING, "parsing error in hat() parameter of axis '%s' for controller %i", button_names[j], i + 1);
                //value1_str[15] = value2_str[15] = 0;
                /* chop off the last character of value2_str if it is the closing parenthesis */
                //lastchar = &value2_str[strlen(value2_str) - 1];
                //if (lastchar > value2_str && *lastchar == ')') *lastchar = 0;
                //controller[i].axis[axis_idx].hat_pos_a = get_hat_pos_by_name(value1_str);
                //controller[i].axis[axis_idx].hat_pos_b = get_hat_pos_by_name(value2_str);
            //}
        }
    }
    fflush(stdout);

    return readOK;
}

void Emulator::save_controller_config(int iCtrlIdx)
{
    m64p_handle pConfig;
    char SectionName[32], Param[32], ParamString[128];
    int j;

    /* Delete the configuration section for this controller, so we can use SetDefaults and save the help comments also */
    sprintf(SectionName, "Input-SDL-Control%i", iCtrlIdx + 1);
    ConfigDeleteSection(SectionName);
    /* Open the configuration section for this controller (create a new one) */
    if (ConfigOpenSection(SectionName, &pConfig) != M64ERR_SUCCESS)
    {
    	printf("Couldn't open config section '%s'", SectionName);
        //DebugMessage(M64MSG_ERROR, "Couldn't open config section '%s'", SectionName);
        return;
    }

    /* save the general controller parameters */
    ConfigSetDefaultBool(pConfig, "plugged", controller[iCtrlIdx].present, "Specifies whether this controller is 'plugged in' to the simulated N64");
    ConfigSetDefaultInt(pConfig, "plugin", controller[iCtrlIdx].plugin, "Specifies which type of expansion pak is in the controller: 1=None, 2=Mem pak, 5=Rumble pak");
    ConfigSetDefaultBool(pConfig, "mouse", controller[iCtrlIdx].mouse, "If True, then mouse buttons may be used with this controller");
    ConfigSetDefaultInt(pConfig, "device", controller[iCtrlIdx].device, "Specifies which joystick is bound to this controller: -3=TouchScreen, -2=Keyboard/mouse, -1=Auto config, 0 or more= SDL Joystick number");
    ConfigSetDefaultInt(pConfig, "layout", controller[iCtrlIdx].layout, "Specifies the initial touchscreen overlay used.");

    //Touchscreen
    if(controller[iCtrlIdx].device == -3){
    	overlay_request = controller[iCtrlIdx].layout;
    	return;
    }

    sprintf(Param, "%i,%i", controller[iCtrlIdx].analogDeadZone[0], controller[iCtrlIdx].analogDeadZone[1]);
    ConfigSetDefaultString(pConfig, "AnalogDeadzone", Param, "The minimum absolute value of the SDL analog joystick axis to move the N64 controller axis value from 0.  For X, Y axes.");
    sprintf(Param, "%i,%i", controller[iCtrlIdx].analogPeak[0], controller[iCtrlIdx].analogPeak[1]);
    ConfigSetDefaultString(pConfig, "AnalogPeak", Param, "An absolute value of the SDL joystick axis >= AnalogPeak will saturate the N64 controller axis value (at 80).  For X, Y axes. For each axis, this must be greater than the corresponding AnalogDeadzone value");

    /* save configuration for all the digital buttons */
    for (j = 0; j < 16; j++ )
    {
        const char *Help;
        int len = 0;
        ParamString[0] = 0;
        if (controller[iCtrlIdx].button[j] > 0)
        {
            sprintf(Param, "key(%i) ", controller[iCtrlIdx].button[j]);
            strcat(ParamString, Param);
        }
        /*
        if (controller[iCtrlIdx].button[j].button >= 0)
        {
            sprintf(Param, "button(%i) ", controller[iCtrlIdx].button[j].button);
            strcat(ParamString, Param);
        }
        */
        /*
        if (controller[iCtrlIdx].button[j].axis >= 0)
        {
            if (controller[iCtrlIdx].button[j].axis_deadzone >= 0)
                sprintf(Param, "axis(%i%c,%i) ", controller[iCtrlIdx].button[j].axis, (controller[iCtrlIdx].button[j].axis_dir == -1) ? '-' : '+',
                        controller[iCtrlIdx].button[j].axis_deadzone);
            else
                sprintf(Param, "axis(%i%c) ", controller[iCtrlIdx].button[j].axis, (controller[iCtrlIdx].button[j].axis_dir == -1) ? '-' : '+');
            strcat(ParamString, Param);
        }
        */
        /*
        if (controller[iCtrlIdx].button[j].hat >= 0)
        {
            sprintf(Param, "hat(%i %s) ", controller[iCtrlIdx].button[j].hat, HAT_POS_NAME(controller[iCtrlIdx].button[j].hat_pos));
            strcat(ParamString, Param);
        }
        */
        /*
        if (controller[iCtrlIdx].button[j].mouse >= 0)
        {
            sprintf(Param, "mouse(%i) ", controller[iCtrlIdx].button[j].mouse);
            strcat(ParamString, Param);
        }
        */
        if (j == 0)
            Help = "Digital button configuration mappings";
        else
            Help = NULL;
        /* if last character is a space, chop it off */
        len = strlen(ParamString);
        if (len > 0 && ParamString[len-1] == ' ')
            ParamString[len-1] = 0;
        ConfigSetDefaultString(pConfig, button_names[j], ParamString, Help);
    }

    /* save configuration for the 2 analog axes */
    for (j = 0; j < 2; j++ )
    {
        const char *Help;
        int len = 0;
        ParamString[0] = 0;
        if (controller[iCtrlIdx].axis[j].a > 0 && controller[iCtrlIdx].axis[j].b > 0)
        {
            sprintf(Param, "key(%i,%i) ", controller[iCtrlIdx].axis[j].a, controller[iCtrlIdx].axis[j].b);
            strcat(ParamString, Param);
        }
        /*
        if (controller[iCtrlIdx].axis[j].button_a >= 0 && controller[iCtrlIdx].axis[j].button_b >= 0)
        {
            sprintf(Param, "button(%i,%i) ", controller[iCtrlIdx].axis[j].button_a, controller[iCtrlIdx].axis[j].button_b);
            strcat(ParamString, Param);
        }
        */
        /*
        if (controller[iCtrlIdx].axis[j].axis_a >= 0 && controller[iCtrlIdx].axis[j].axis_b >= 0)
        {
            sprintf(Param, "axis(%i%c,%i%c) ", controller[iCtrlIdx].axis[j].axis_a, (controller[iCtrlIdx].axis[j].axis_dir_a <= 0) ? '-' : '+',
                                               controller[iCtrlIdx].axis[j].axis_b, (controller[iCtrlIdx].axis[j].axis_dir_b <= 0) ? '-' : '+' );
            strcat(ParamString, Param);
        }
        */
        /*
        if (controller[iCtrlIdx].axis[j].hat >= 0)
        {
            sprintf(Param, "hat(%i %s %s) ", controller[iCtrlIdx].axis[j].hat,
                                             HAT_POS_NAME(controller[iCtrlIdx].axis[j].hat_pos_a),
                                             HAT_POS_NAME(controller[iCtrlIdx].axis[j].hat_pos_b));
            strcat(ParamString, Param);
        }
        */
        if (j == 0)
            Help = "Analog axis configuration mappings";
        else
            Help = NULL;
        /* if last character is a space, chop it off */
        len = strlen(ParamString);
        if (len > 0 && ParamString[len-1] == ' ')
            ParamString[len-1] = 0;
        ConfigSetDefaultString(pConfig, button_names[16 + j], ParamString, Help);
    }

    ConfigSaveFile();
}

void Emulator::SaveState(){
	CoreDoCommand(M64CMD_STATE_SAVE,1,NULL);
	save = 1;
}

void Emulator::LoadState(){
	CoreDoCommand(M64CMD_STATE_LOAD,0,NULL);
	load = 1;
}

void Emulator::LoadTouchOverlay(){
	static int current = 2;

	overlay_request = current++;
	if(current == 3){
		current = 0;
	}
}

void Emulator::ExitEmulator(){

	set_z_order(-10);
	int vis = 0;
	screen_set_window_property_iv(screen_win, SCREEN_PROPERTY_VISIBLE, &vis);
	sleep(1);
	CoreDoCommand(M64CMD_STOP,0,NULL);
}
/*
void zeldaSubscreenHack()
{
	static unsigned int crc1[] = {0xEC7011B7,0xD43DA81F,0x693BA2AE,0xB044B569,0xB2055FBD,0xEC7011B7,0xF034001A};
	static unsigned int crc2[] = {0x7616D72B,0x021E1E19,0xB7F14E9F,0x373C1985,0x0BAB4E0C,0x7616D72B,0xAE47ED06};

	m64p_rom_header header;

	int i;
    if ((*CoreDoCommand)(M64CMD_ROM_GET_HEADER, sizeof(header), &header) == M64ERR_SUCCESS)
    {
    	printf("Looking for zelda rom.\n");fflush(stdout);
    	for(i = 0;i < sizeof(crc1);i++)
    	{
    		if(header.CRC1 == crc1[i] && header.CRC2 == crc2[i])
    		{
    			printf("Found Zelda! using subscreen cheat!\n");
				l_CheatMode = CHEAT_LIST;
				l_CheatNumList = "0";
				return;
    		}
    	}
    }

}*/
