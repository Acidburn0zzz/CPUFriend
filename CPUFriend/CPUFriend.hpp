//
//  CPUFriend.hpp
//  CPUFriend
//
//  Copyright © 2017 Vanilla. All rights reserved.
//

#ifndef kern_cpuf_hpp
#define kern_cpuf_hpp

#include <Headers/kern_patcher.hpp>
#include <Library/LegacyIOService.h>

class EXPORT CPUFriendData : public IOService {
	OSDeclareDefaultStructors(CPUFriendData)
public:
	IOService *probe(IOService *provider, SInt32 *score) override;
};

class CPUFriendPlugin {
	public:
		bool init();
		
		/**
		 *  Loaded user-specified frequency data
		 */
		const void *frequencyData = nullptr;
		
		/**
		 *  Loaded user-specified frequency data size
		 */
		uint32_t frequencyDataSize = 0;

	private:
		/**
		 *  Trampolines for original resource load callback
		 */
		mach_vm_address_t orgACPISMCConfigLoadCallback {0};
		mach_vm_address_t orgX86PPConfigLoadCallback   {0};
	
		/**
		 *  Hooked ResourceLoad callback returning user-specified platform data
		 */
		static void myConfigResourceCallback(uint32_t requestTag, kern_return_t result, const void *resourceData, uint32_t resourceDataLength, void *context);
		
		/**
		 *  Patch kext if needed and prepare other patches
		 *
		 *  @param patcher KernelPatcher instance
		 *  @param index   kinfo handle
		 *  @param address kinfo load address
		 *  @param size    kinfo memory size
		 */
		void processKext(KernelPatcher &patcher, size_t index, mach_vm_address_t address, size_t size);
		
		/**
		 *  Current progress mask
		 */
		struct ProcessingState {
			enum : uint32_t {
				NothingReady   = 0,
				CallbackRouted = 1,
				EverythingDone = CallbackRouted
			};
		};
		uint32_t progressState = ProcessingState::NothingReady;
};

#endif /* kern_cpuf_hpp */
