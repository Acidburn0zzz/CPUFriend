//
//  CPUFriend.cpp
//  CPUFriend
//
//  Copyright © 2017 Vanilla. All rights reserved.
//

#include <Headers/kern_api.hpp>

#include "CPUFriend.hpp"

static const char * binList[] {
	"/System/Library/Extensions/IOPlatformPluginFamily.kext/Contents/PlugIns/X86PlatformPlugin.kext/Contents/MacOS/X86PlatformPlugin"
};

static const char * idList[] {
	"com.apple.driver.X86PlatformPlugin"
};

static const char * symbolList[] {
	"__ZN17X86PlatformPlugin22configResourceCallbackEjiPKvjPv"
};

static KernelPatcher::KextInfo kextList[] {
	{ idList[0], & binList[0], arrsize(binList), { false, false }, {}, KernelPatcher::KextInfo::Unloaded }
};

static constexpr size_t kextListSize = arrsize(kextList);

static CPUFriendPlugin * callbackCpuf = nullptr;

OSDefineMetaClassAndStructors(CPUFriendPlatform, IOService)

IOService * CPUFriendPlatform::probe(IOService * provider, SInt32 * score) {
	if (provider) {
		if (callbackCpuf) {
			if (! callbackCpuf->frequencyData) {
				auto name = provider->getName();
				if (! name)
					name = "(null)";
				DBGLOG("probe", "looking for cf-frequency-data in %s", name);
				
				auto data = OSDynamicCast(OSData, provider->getProperty("cf-frequency-data"));
				if (! data) {
					auto cpu = provider->getParentEntry(gIOServicePlane);
					if (cpu) {
						name = cpu->getName();
						if (! name)
							name = "(null)";
						DBGLOG("probe", "looking for cf-frequency-data in %s", name);
						data = OSDynamicCast(OSData, cpu->getProperty("cf-frequency-data"));
					} else {
						SYSLOG("probe", "unable to access cpu parent");
					}
				}
				
				if (data) {
					callbackCpuf->frequencyDataSize = data->getLength();
					callbackCpuf->frequencyData = data->getBytesNoCopy();
				} else {
					SYSLOG("probe", "failed to obtain cf-frequency-data");
				}
			}
		} else
			SYSLOG("probe", "missing storage instance");
	}
	
	return nullptr;
}

bool CPUFriendPlugin::init() {
	callbackCpuf = this;
	
	LiluAPI::Error error = lilu.onKextLoad(kextList, kextListSize, [](void * user, KernelPatcher & patcher, size_t index, mach_vm_address_t address, size_t size) {
		static_cast<CPUFriendPlugin * >(user)->processKext(patcher, index, address, size);
	}, this);
	
	if (error != LiluAPI::Error::NoError) {
		SYSLOG("init", "failed to register onKextLoad method %d", error);
		return false;
	}

	return true;
}

void CPUFriendPlugin::myConfigResourceCallback(uint32_t requestTag, kern_return_t result, const void * resourceData, uint32_t resourceDataLength, void * context) {
	if (callbackCpuf && callbackCpuf->orgConfigLoadCallback) {
		auto data = callbackCpuf->frequencyData;
		auto sz = callbackCpuf->frequencyDataSize;
		if (data && sz > 0) {
			DBGLOG("myConfigResourceCallback", "feeding frequency data %u", sz);
			resourceData = data;
			resourceDataLength = sz;
			result = kOSReturnSuccess;
		} else {
			SYSLOG("myConfigResourceCallback", "failed to feed cpu data (%u, %d)", sz, data != nullptr);
		}
		callbackCpuf->orgConfigLoadCallback(requestTag, result, resourceData, resourceDataLength, context);
	} else {
		SYSLOG("myConfigResourceCallback", "config callback arrived at nowhere");
	}
}

void CPUFriendPlugin::processKext(KernelPatcher & patcher, size_t index, mach_vm_address_t address, size_t size) {
	if (progressState != ProcessingState::EverythingDone) {
		for (size_t i = 0; i < kextListSize; i++) {
			if (kextList[i].loadIndex == index) {
				DBGLOG("processKext", "current kext is %s progressState %d", kextList[i].id, progressState);
				// clear error from the very beginning just in case
				patcher.clearError();
				if (! strcmp(kextList[i].id, idList[0])) {
					auto callback = patcher.solveSymbol(index, symbolList[0]);
					if (callback) {
						orgConfigLoadCallback = reinterpret_cast<t_callback>(patcher.routeFunction(callback, reinterpret_cast<mach_vm_address_t>(myConfigResourceCallback), true));
						if (patcher.getError() == KernelPatcher::Error::NoError) {
							DBGLOG("processKext", "routed %s", symbolList[0]);
						} else {
							SYSLOG("processKext", "failed to route %s", symbolList[0]);
						}
					} else {
						SYSLOG("processKext", "failed to find %s", symbolList[0]);
					}
					progressState |= ProcessingState::CallbackRouted;
				}
			}
		}
	}
	// Ignore all the errors for other processors
	patcher.clearError();
}
