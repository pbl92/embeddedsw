/*******************************************************************************
 *
 * Copyright (C) 2015 Xilinx, Inc.  All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * Use of the Software is limited solely to applications:
 * (a) running on a Xilinx device, or
 * (b) that interact with a Xilinx device through a bus or interconnect.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * XILINX CONSORTIUM BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF
 * OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 * Except as contained in this notice, the name of the Xilinx shall not be used
 * in advertising or otherwise to promote the sale, use or other dealings in
 * this Software without prior written authorization from Xilinx.
 *
*******************************************************************************/
/******************************************************************************/
/**
 *
 * @file xdprx.c
 *
 * @note	None.
 *
 * <pre>
 * MODIFICATION HISTORY:
 *
 * Ver   Who  Date     Changes
 * ----- ---- -------- -----------------------------------------------
 * </pre>
 *
*******************************************************************************/

/******************************* Include Files ********************************/

#include "xdprx.h"
#include "xstatus.h"
#if defined(__arm__)
#include "sleep.h"
#elif defined(__MICROBLAZE__)
#include "microblaze_sleep.h"
#endif

/**************************** Function Prototypes *****************************/

/* Miscellaneous functions. */
static u32 XDprx_WaitPhyReady(XDprx *InstancePtr, u8 Mask);

/**************************** Function Definitions ****************************/

/******************************************************************************/
/**
 * This function retrieves the configuration for this DisplayPort RX instance
 * and fills in the InstancePtr->Config structure.
 *
 * @param	InstancePtr is a pointer to the XDprx instance.
 * @param	ConfigPtr is a pointer to the configuration structure that will
 *		be used to copy the settings from.
 * @param	EffectiveAddr is the device base address in the virtual memory
 *		space. If the address translation is not used, then the physical
 *		address is passed.
 *
 * @return	None.
 *
 * @note	Unexpected errors may occur if the address mapping is changed
 *		after this function is invoked.
 *
*******************************************************************************/
void XDprx_CfgInitialize(XDprx *InstancePtr, XDp_Config *ConfigPtr,
							u32 EffectiveAddr)
{
	/* Verify arguments. */
	Xil_AssertVoid(InstancePtr != NULL);
	Xil_AssertVoid(ConfigPtr != NULL);
	Xil_AssertVoid(EffectiveAddr != 0x0);

	InstancePtr->IsReady = 0;

	InstancePtr->Config.DeviceId = ConfigPtr->DeviceId;
	InstancePtr->Config.BaseAddr = EffectiveAddr;
	InstancePtr->Config.SAxiClkHz = ConfigPtr->SAxiClkHz;

	InstancePtr->Config.MaxLaneCount = ConfigPtr->MaxLaneCount;
	InstancePtr->Config.MaxLinkRate = ConfigPtr->MaxLinkRate;

	InstancePtr->Config.MaxBitsPerColor = ConfigPtr->MaxBitsPerColor;
	InstancePtr->Config.QuadPixelEn = ConfigPtr->QuadPixelEn;
	InstancePtr->Config.DualPixelEn = ConfigPtr->DualPixelEn;
	InstancePtr->Config.YCrCbEn = ConfigPtr->YCrCbEn;
	InstancePtr->Config.YOnlyEn = ConfigPtr->YOnlyEn;
	InstancePtr->Config.PayloadDataWidth = ConfigPtr->PayloadDataWidth;

	InstancePtr->Config.SecondaryChEn = ConfigPtr->SecondaryChEn;
	InstancePtr->Config.NumAudioChs = ConfigPtr->NumAudioChs;

	InstancePtr->Config.MstSupport = ConfigPtr->MstSupport;
	InstancePtr->Config.NumMstStreams = ConfigPtr->NumMstStreams;

	InstancePtr->Config.DpProtocol = ConfigPtr->DpProtocol;

	InstancePtr->Config.IsRx = ConfigPtr->IsRx;

	InstancePtr->IsReady = XIL_COMPONENT_IS_READY;
}

/******************************************************************************/
/**
 * This function installs a custom delay/sleep function to be used by the XDprx
 * driver.
 *
 * @param	InstancePtr is a pointer to the XDprx instance.
 * @param	CallbackFunc is the address to the callback function.
 * @param	CallbackRef is the user data item (microseconds to delay) that
 *		will be passed to the custom sleep/delay function when it is
 *		invoked.
 *
 * @return	None.
 *
 * @note	None.
 *
*******************************************************************************/
void XDprx_SetUserTimerHandler(XDprx *InstancePtr,
			XDp_TimerHandler CallbackFunc, void *CallbackRef)
{
	/* Verify arguments. */
	Xil_AssertVoid(InstancePtr != NULL);
	Xil_AssertVoid(CallbackFunc != NULL);
	Xil_AssertVoid(CallbackRef != NULL);

	InstancePtr->UserTimerWaitUs = CallbackFunc;
	InstancePtr->UserTimerPtr = CallbackRef;
}

/******************************************************************************/
/**
 * This function is the delay/sleep function for the XDprx driver. For the Zynq
 * family, there exists native sleep functionality. For MicroBlaze however,
 * there does not exist such functionality. In the MicroBlaze case, the default
 * method for delaying is to use a predetermined amount of loop iterations. This
 * method is prone to inaccuracy and dependent on system configuration; for
 * greater accuracy, the user may supply their own delay/sleep handler, pointed
 * to by InstancePtr->UserTimerWaitUs, which may have better accuracy if a
 * hardware timer is used.
 *
 * @param	InstancePtr is a pointer to the XDprx instance.
 * @param	MicroSeconds is the number of microseconds to delay/sleep for.
 *
 * @return	None.
 *
 * @note	None.
 *
*******************************************************************************/
void XDprx_WaitUs(XDprx *InstancePtr, u32 MicroSeconds)
{
	/* Verify arguments. */
	Xil_AssertVoid(InstancePtr != NULL);
	Xil_AssertVoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

	if (MicroSeconds == 0) {
		return;
	}

#if defined(__MICROBLAZE__)
	if (InstancePtr->UserTimerWaitUs != NULL) {
		/* Use the timer handler specified by the user for better
		 * accuracy. */
		InstancePtr->UserTimerWaitUs(InstancePtr, MicroSeconds);
	}
	else {
		/* MicroBlaze sleep only has millisecond accuracy. Round up. */
		u32 MilliSeconds = (MicroSeconds + 999) / 1000;
		MB_Sleep(MilliSeconds);
	}
#elif defined(__arm__)
	/* Wait the requested amount of time. */
	usleep(MicroSeconds);
#endif
}

/******************************************************************************/
/**
 * This function waits for the DisplayPort PHY to come out of reset.
 *
 * @param	InstancePtr is a pointer to the XDprx instance.
 * @param	Mask specifies which bits to wait for the PHY to be ready on.
 *
 * @return
 *		- XST_ERROR_COUNT_MAX if the PHY failed to be ready.
 *		- XST_SUCCESS otherwise.
 *
 * @note	None.
 *
*******************************************************************************/
static u32 XDprx_WaitPhyReady(XDprx *InstancePtr, u8 Mask)
{
	u32 Timeout = 100;
	u32 PhyStatus;

	/* Wait until the PHY is ready. */
	do {
		PhyStatus = XDprx_ReadReg(InstancePtr->Config.BaseAddr,
						XDPRX_PHY_STATUS) & Mask;

		/* Protect against an infinite loop. */
		if (!Timeout--) {
			return XST_ERROR_COUNT_MAX;
		}
		XDprx_WaitUs(InstancePtr, 20);
	}
	while (PhyStatus != Mask);

	return XST_SUCCESS;
}
