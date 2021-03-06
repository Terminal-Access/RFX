#include "Refractor.h"
#include "BF2Stl.h"
#include <Windows.h>
#include "bs\vtable_hook.h"
#include "bs\vectors.h"

namespace DICE
{
	bs::IWindow* pWindow = nullptr;

	void init(bs::IWindow* pWindow_)
	{
		detail::g_pClassManager = *reinterpret_cast<detail::CClassManager**>(0x5FF960 + unsigned int(GetModuleHandle(0)));
		detail::g_pMainConsole = reinterpret_cast<detail::CMainConsole*>(detail::g_pClassManager->getClassByName(&std::string("MainConsole")));
		detail::g_pMainConsoleObjects = reinterpret_cast<detail::CMainConsoleObjects*>(detail::g_pClassManager->getClassByName(&std::string("MainConsoleObjects")));
		detail::g_pHudInformationLayer = reinterpret_cast<detail::CHudInformationLayer*>(detail::g_pClassManager->getClassByName(&std::string("HudInformationLayer")));

		HudItems::init();
		ShaderManager::init();

		pWindow = pWindow_;
	}

	void update()
	{
		// TODO: Find a better place/way to do this
		// there seems to be a new instance of SwiffPlayer each time the main menu is opened. Maybe we could hook the method creating the instance and doing this there?
		detail::CSwiffPlayer* pSwiffPlayer = nullptr;
		static detail::CSwiffPlayer* preSwiffPlayer = nullptr;
		if(detail::g_pClassManager)
		{
			pSwiffPlayer = reinterpret_cast<detail::CSwiffPlayer*>(detail::g_pClassManager->getClassByName(&std::string("SwiffPlayer")));
			if (pSwiffPlayer && pSwiffPlayer != preSwiffPlayer)
			{
				HOOK_VTABLE_FUNCTION(pSwiffPlayer, &detail::CSwiffPlayer::updateMouse, [](Hook_t::DetourArgs_t detourArgs)
				{
					// Fixes mouse related bug in flash menu after resizing window.
					detail::CSwiffHost* pSwiffHost = reinterpret_cast<detail::CSwiffHost*>(detail::g_pClassManager->getClassByName(&std::string("SwiffHost")));

					bs::TUInt2 preMouseArea;
					if (pSwiffHost)
					{
						preMouseArea = bs::TUInt2(pSwiffHost->mouseAreaWidth, pSwiffHost->mouseAreaHeight);
						if (pWindow)
						{
							pSwiffHost->mouseAreaWidth = pWindow->getClientSize().x();
							pSwiffHost->mouseAreaHeight = pWindow->getClientSize().y();
						}
					}

					detourArgs.callOriginal();

					if (pSwiffHost)
					{
						pSwiffHost->mouseAreaWidth = preMouseArea.x();
						pSwiffHost->mouseAreaHeight = preMouseArea.y();
					}
				});
				preSwiffPlayer = pSwiffPlayer;
			}
		}
	}

	namespace detail
	{
		CClassManager* g_pClassManager;
		CMainConsole* g_pMainConsole;
		CMainConsoleObjects* g_pMainConsoleObjects;
		CHudInformationLayer* g_pHudInformationLayer;

		std::string* pInvokeReturn = nullptr;

		void* __fastcall  myInvoke(CMainConsoleObject* thisptr, int, std::string* result, std::string* _args, int numArgs)
		{
			delete pInvokeReturn;
			::std::vector<::std::string> args;
			for (int i = 0; i < numArgs; i++)
			{
				args.emplace_back(_args[i].c_str());
			}
			::std::string retVal = thisptr->pRFXObject->operator()(args);
			if (retVal.size() < 16)
			{
				*result = retVal.c_str();
				return result;
			}
			//only way i got this working with strings longer than 16 chars
			return pInvokeReturn = new std::string(retVal.c_str());
		}

		void createHookedConsoleObject(bs::string_functor* pFunctor, const ::std::string& name, int minNumArgs, int maxNumArgs)
		{
			static detail::CMainConsoleObject* pSource = nullptr;
			static void** customVTable = nullptr;
			if (!pSource)
			{
				pSource = reinterpret_cast<CMainConsoleObject*>(stl_71_wrapper::map_find(&g_pMainConsoleObjects->entryList, "Varsset"));
				// Allocate a new virtual function table for our custom console objects
				void** originalVTable = *(void***)pSource;
				customVTable = (void**)malloc(sizeof(void*) * 32/*nrOfFunctions*/);

				// Copy all vBF2 methods to the custom table
				memcpy(customVTable, originalVTable, sizeof(void*) * 32);
				// Replace original invoke method with ours
				customVTable[21/*invoke*/] = myInvoke;
			}

			CMainConsoleObject* pObject = reinterpret_cast<CMainConsoleObject*>(malloc(sizeof(CMainConsoleObject)));
			memcpy(pObject, pSource, sizeof(CMainConsoleObject));
			
			*(void***)pObject = customVTable;

			//pObject->authorizationLevel = userAccessible ? 1 : 2;
			// this determines whether the command can be used from the ingame console or not
			pObject->authorizationLevel = 1;
			pObject->minNumArgs = minNumArgs;
			pObject->maxNumArgs = maxNumArgs;

			// AFAIK these are only used for auto complete and dices' implementation of invoke
			// auto complete doesn't work for custom commands and we have our own invoke method
			// so these are nulled/used for our own purposes
			pObject->returnType = nullptr;
			pObject->argType1 = nullptr;
			pObject->argType2 = nullptr;
			pObject->argType3 = nullptr;
			pObject->nodeName = nullptr;
			pObject->pRFXObject = pFunctor;

			stl_71_wrapper::map_insert(&g_pMainConsoleObjects->entryList, name.c_str(), pObject);
		}
	}

	::std::string invoke(const ::std::string& cmd)
	{
		if(!detail::g_pMainConsole)
			return "";
		std::string retVal;
		retVal._Grow(1024); // TODO: figure out what is the actual required size / size used by DICE
		detail::g_pMainConsole->Exec(&std::string((cmd + '\n').c_str()), &retVal);
		return ::std::string(retVal.c_str());
	}

	/*void* getLocalPlayer()
	{
		return nullptr;
	}*/
}