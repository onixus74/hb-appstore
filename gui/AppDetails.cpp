#include <SDL2/SDL2_gfxPrimitives.h>
#include <fstream>
#include <sstream>

#if defined(SWITCH)
#include <switch.h>
#endif

#include "../libs/get/src/Get.hpp"
#include "../libs/get/src/Utils.hpp"

#include "../libs/chesto/src/Button.hpp"
#include "../libs/chesto/src/RootDisplay.hpp"
#include "../libs/chesto/src/NetImageElement.hpp"

#include "AppDetails.hpp"
#include "Feedback.hpp"
#include "AppList.hpp"
#include "ImageCache.hpp"
#include "main.hpp"

int AppDetails::lastFrameTime = 99;

AppDetails::AppDetails(Package* package, AppList* appList)
{
	this->package = package;
	this->get = appList->get;
	this->appList = appList;

	const char* action;
	switch (package->status)
	{
	case GET:
		action = "Download";
		break;
	case UPDATE:
		action = "Update";
		break;
	case INSTALLED:
		action = "Remove";
		break;
	case LOCAL:
		action = "Reinstall";
		break;
	default:
		action = "?";
	}

	// TODO: show current app status somewhere

	// download/update/remove button (2)

	Button* download = new Button(action, A_BUTTON, true, 30);
	download->position(970, 550);
	download->action = std::bind(&AppDetails::proceed, this);

	Button* cancel = new Button("Cancel", B_BUTTON, true, 30, download->width);
	cancel->position(970, 630);
	cancel->action = std::bind(&AppDetails::back, this);

#if defined(SWITCH)
	// display an additional launch/install button if the package is installed,  and has a binary or is a theme

	bool hasBinary = package->binary != "none";
	bool isTheme = this->package->category == "theme";

	if (package->status != GET && (hasBinary || isTheme))
	{
		download->position(970, 470);
		cancel->position(970, 630);

		const char* buttonLabel = "Launch";
		bool injectorPresent = false;

		if (isTheme)
		{
			Package* installer = get->lookup("NXthemes_Installer");
			injectorPresent = installer != NULL; // whether or not the currently hardcoded installer package exists, in the future becomes something functionality-based like "theme_installer"
			buttonLabel = (injectorPresent && installer->status == GET) ? "Injector" : "Inject";
		}

		// show the third button if a binary is present, or a theme injector is available (installed or not)
		if (hasBinary || injectorPresent)
		{
			this->canLaunch = true;

			Button* start = new Button(buttonLabel, START_BUTTON, true, 30, download->width);
			start->position(970, 550);
			start->action = std::bind(&AppDetails::launch, this);
			this->elements.push_back(start);
		}
	}
#endif

	this->elements.push_back(cancel);
	this->elements.push_back(download);

	// the scrollable portion of the app details page
	AppDetailsContent* content = new AppDetailsContent();
	this->elements.push_back(content);

	int MARGIN = content->MARGIN;
	int BANNER_X = content->BANNER_X;
	int BANNER_Y = content->BANNER_Y;

	TextElement* title = new TextElement(package->title.c_str(), 35, &black);
	title->position(MARGIN, 30);
	content->elements.push_back(title);

	Button* moreByAuthor = new Button("More by Author", X_BUTTON);

	Button* reportIssue = new Button("Report Issue", Y_BUTTON);
	reportIssue->position(920 - MARGIN - reportIssue->width, 45);
	moreByAuthor->position(reportIssue->x - 20 - moreByAuthor->width, 45);
	moreByAuthor->action = std::bind(&AppDetails::moreByAuthor, this);
	reportIssue->action = std::bind(&AppDetails::leaveFeedback, this);
	content->elements.push_back(reportIssue);
	content->elements.push_back(moreByAuthor);

#if defined(__WIIU__)
	// Use an icon banner
	NetImageElement* banner = new NetImageElement(package->getIconUrl().c_str(), []{
		// if the icon fails to load, use the default icon
		ImageElement *defaultIcon = new ImageElement(RAMFS "res/default.png");
		defaultIcon->setScaleMode(SCALE_PROPORTIONAL_WITH_BG);
		return defaultIcon;
	});
	banner->setScaleMode(SCALE_PROPORTIONAL_WITH_BG);
#else
	// Load the banner from network
	NetImageElement *banner = new NetImageElement(package->getBannerUrl().c_str(), [package]{
		// If the banner fails to load, use an icon banner
		NetImageElement* icon = new NetImageElement(package->getIconUrl().c_str(), []{
			// if even the icon fails to load, use the default icon
			ImageElement *defaultIcon = new ImageElement(RAMFS "res/default.png");
			defaultIcon->setScaleMode(SCALE_PROPORTIONAL_WITH_BG);
			return defaultIcon;
		});
		icon->setScaleMode(SCALE_PROPORTIONAL_WITH_BG);
		return icon;
	});
#endif
	banner->resize(787, 193);
	banner->position(BANNER_X, BANNER_Y);
	content->elements.push_back(banner);

	TextElement* title2 = new TextElement(package->author.c_str(), 27, &gray);
	title2->position(MARGIN, 80);
	content->elements.push_back(title2);

	// the main description (wrapped text)
	TextElement* details = new TextElement(package->long_desc.c_str(), 20, &black, false, 740);
	details->position(MARGIN + 30, banner->y + banner->height + 22);
	content->elements.push_back(details);

	TextElement* changelog = new TextElement((std::string("Changelog:\n") + package->changelog).c_str(), 20, &black, false, 740);
	changelog->position(MARGIN + 30, details->y + details->height + 30);
	content->elements.push_back(changelog);

	// lots of details that we know about the package
	std::stringstream more_details;
	more_details << "Title: " << package->title << "\n"
				 << package->short_desc << "\n\n"
				 << "Author: " << package->author << "\n"
				 << "Version: " << package->version << "\n"
				 << "License: " << package->license << "\n\n"
				 << "Package: " << package->pkg_name << "\n"
				 << "Downloads: " << package->downloads << "\n"
				 << "Updated: " << package->updated << "\n\n"
				 << "Download size: " << package->download_size << " KB\n"
				 << "Install size: " << package->extracted_size << " KB\n";

	auto mdeets = more_details.str();

	TextElement* more_details_elem = new TextElement(mdeets.c_str(), 20, &white, false, 300);
	more_details_elem->position(940, 50);
	this->elements.push_back(more_details_elem);
}

// TODO: make one push event function to bind instead of X separeate ones
void AppDetails::proceed()
{
	SDL_Event sdlevent;
	sdlevent.type = SDL_KEYDOWN;
	sdlevent.key.keysym.sym = SDLK_a;
	sdlevent.key.repeat = 0;
	SDL_PushEvent(&sdlevent);
}

void AppDetails::launch()
{
	if (!this->canLaunch)
		return;

	SDL_Event sdlevent;
	sdlevent.type = SDL_KEYDOWN;
	sdlevent.key.keysym.sym = SDLK_RETURN;
	sdlevent.key.repeat = 0;
	SDL_PushEvent(&sdlevent);
}

void AppDetails::getSupported()
{
	Package* installer = get->lookup("NXthemes_Installer");
	if (installer != NULL)
		RootDisplay::subscreen = new AppDetails(installer, appList);
}

void AppDetails::back()
{
	SDL_Event sdlevent;
	sdlevent.type = SDL_KEYDOWN;
	sdlevent.key.keysym.sym = SDLK_b;
	sdlevent.key.repeat = 0;
	SDL_PushEvent(&sdlevent);
}

void AppDetails::moreByAuthor()
{
	const char* author = this->package->author.c_str();
	appList->sidebar->searchQuery = std::string(author);
	appList->sidebar->curCategory = 0;
	appList->update();
	appList->y = 0;
	appList->keyboard->hidden = true;
	RootDisplay::subscreen = NULL; // TODO: clean up memory???
}

void AppDetails::leaveFeedback()
{
	RootDisplay::subscreen = new Feedback(this->package);
}

bool AppDetails::process(InputEvents* event)
{

	// don't process any keystrokes if an operation is in progress
	if (this->operating)
		return false;

	if (event->pressed(B_BUTTON))
	{
		RootDisplay::subscreen = NULL;
		return true;
	}

	if (event->pressed(A_BUTTON))
	{
		this->operating = true;
		// event->key.keysym.sym = SDLK_z;
		event->update();
		
		// description of what we're doing
		
		TextElement* description = new TextElement("Downloading package...", 30);
		description->position(10, 10);
		this->elements.push_back(description);

		// add a progress bar to the screen to be drawn
		this->pbar = new ProgressBar();
		pbar->width = 740;
		pbar->position(1280 / 2 - this->pbar->width / 2, 720 / 2 - 5);
		pbar->color = 0xff0000ff;
		pbar->dimBg = true;
		this->elements.push_back(pbar);

		// setup progress bar callback
		networking_callback = AppDetails::updateCurrentlyDisplayedPopup;

		// if we're installing ourselves, we need to quit after on switch
		preInstallHook();

		// install or remove this package based on the package status
		if (this->package->status == INSTALLED)
			get->remove(this->package);
		else
			get->install(this->package);

		postInstallHook();

		// refresh the screen
		this->wipeElements();
		RootDisplay::subscreen = NULL;

		this->operating = false;
		this->appList->update();
		return true;
	}

#if defined(SWITCH)
	if (event->pressed(START_BUTTON) && this->canLaunch == true)
	{
		char path[8 + strlen(package->binary.c_str())];

		sprintf(path, "sdmc:/%s", package->binary.c_str());
		printf("Launch path: %s\n", path);

		FILE* file;
		bool successLaunch = false;

		if (package->category == "theme")
		{
			Package* installer = get->lookup("NXthemes_Installer"); // This should probably be more dynamic in future, e.g. std::vector<Package*> Get::find_functionality("theme_installer")
			if (installer != NULL && installer->status != GET)
			{
				sprintf(path, "sdmc:/%s", installer->binary.c_str());
				successLaunch = this->themeInstall(path);
			}
			else
			{
				successLaunch = true;
				this->getSupported();
			}
		}
		else
		{
			//Final check if path actually exists
			if ((file = fopen(path, "r")))
			{
				fclose(file);
				printf("Path OK, Launching...");
				successLaunch = this->launchFile(path, path);
			}
			else
				successLaunch = false;
		}

		if (!successLaunch)
		{
			printf("Failed to launch.");
			TextElement* errorText = new TextElement("Couldn't launch app", 24, &red, false, 300);
			errorText->position(970, 430);
			this->elements.push_back(errorText);
			this->canLaunch = false;
		}
		return true;
	}
#endif

	if (event->isTouchDown())
		this->dragging = true;

	// if A or B were hit, we don't get down here (which is good, because the children buttons are just pushing A and B events)
	return super::process(event);
}

void AppDetails::preInstallHook()
{
  // is there an echo in here?
}

bool AppDetails::themeInstall(char* installerPath)
{
	std::string ManifestPathInternal = "manifest.install";
	std::string ManifestPath = get->pkg_path + this->package->pkg_name + "/" + ManifestPathInternal;

	std::vector<std::string> themePaths;

	if (!package->manifest) package->manifest = new Manifest(ManifestPath, ROOT_PATH);

	if (package->manifest->valid)
	{
		for (size_t i = 0; i <= package->manifest->entries.size() - 1; i++)
		{
			if (package->manifest->entries[i].operation == MUPDATE && package->manifest->entries[i].extension == "nxtheme")
			{
				printf("Found nxtheme\n");
				themePaths.push_back(package->manifest->entries[i].path);
			}
		}

	}else{
		printf("--> ERROR: no manifest found/manifest invalid at %s\n", ManifestPath.c_str());
		return false;
	}

	std::string themeArg = "installtheme=";
	for (int i = 0; i < (int)themePaths.size(); i++)
	{
		if (i == (int)themePaths.size() - 1)
		{
			themeArg.append(themePaths[i]);
		}
		else
		{
			themeArg.append(themePaths[i]);
			themeArg.append(",");
		}
	}
	printf("Theme Install: %s\n", themeArg.c_str());
	size_t index = 0;
	while (true)
	{
		index = themeArg.find(" ", index);
		if (index == std::string::npos) break;
		themeArg.replace(index, 1, "(_)");
	}
	char args[strlen(installerPath) + themeArg.size() + 8];
	sprintf(args, "%s %s", installerPath, themeArg.c_str());
	return this->launchFile(installerPath, args);
}

bool AppDetails::launchFile(char* path, char* context)
{
#if defined(SWITCH)
	//If setnexload works without problems, quit to make loader open next nro
	if (R_SUCCEEDED(envSetNextLoad(path, context)))
	{
		quit();
		return true;
	}
	return false;
#endif
	return false;
}

void AppDetails::postInstallHook()
{
  // nothing!!
}

void AppDetails::render(Element* parent)
{
	if (this->renderer == NULL)
		this->renderer = parent->renderer;
	if (this->parent == NULL)
		this->parent = parent;

	// draw white background
	SDL_Rect dimens = { 0, 0, 920, 720 };

	SDL_SetRenderDrawColor(parent->renderer, 0xFF, 0xFF, 0xFF, 0xFF);
	SDL_RenderFillRect(parent->renderer, &dimens);

	SDL_SetRenderDrawColor(parent->renderer, 0xFF, 0xFF, 0xFF, 0xFF);

	// draw all elements
	super::render(this);
}

int AppDetails::updateCurrentlyDisplayedPopup(void* clientp, double dltotal, double dlnow, double ultotal, double ulnow)
{
	int now = SDL_GetTicks();
	int diff = now - AppDetails::lastFrameTime;

	if (dltotal == 0) dltotal = 1;

	double amount = dlnow / dltotal;

	// don't update the GUI too frequently here, it slows down downloading
	// (never return early if it's 100% done)
	if (diff < 32 && amount != 1)
		return 0;

	AppDetails* popup = (AppDetails*)RootDisplay::subscreen;

	// update the amount
	if (popup != NULL)
	{
		if (popup->pbar != NULL)
			popup->pbar->percent = amount;

		// force render the element right here (and it's progress bar too)
		if (popup->parent != NULL)
		{
			InputEvents* events = new InputEvents();
			while (events->update())
				popup->parent->process(events);
			popup->parent->render(NULL);
		}
	}

	AppDetails::lastFrameTime = SDL_GetTicks();

	return 0;
}

void AppDetailsContent::render(Element* parent)
{
	if (this->parent == NULL)
		this->parent = parent;

	this->renderer = parent->renderer;

	super::render(this);
}

bool AppDetailsContent::process(InputEvents* event)
{
	int SPEED = 60;
	bool ret = false;

	// handle up and down for the scroll view
	if (event->isKeyDown())
	{
		// scroll the view
		this->y += (SPEED * event->held(UP_BUTTON) - SPEED * event->held(DOWN_BUTTON));
		if (this->y > 0)
			this->y = 0;
		ret |= event->held(UP_BUTTON) || event->held(DOWN_BUTTON);
	}

	return ret || ListElement::process(event);
}
