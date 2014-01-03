/**
 * otg.c - DesignWare USB3 DRD Controller OTG Support
 *
 * Copyright (C) 2010-2014 Texas Instruments Incorporated - http://www.ti.com
 *
 * Authors: Felipe Balbi <balbi@ti.com>,
 *	    George Cherian <george.cherian@ti.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2  of
 * the License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/spinlock.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>

#include "core.h"
#include "io.h"

static void dwc3_otg_enable_events(struct dwc3 *dwc)
{
	u32 reg;

	reg = (DWC3_OEVTEN_CONIDSTSCHNGEN |
			DWC3_OEVTEN_HRRCONFNOTIFEN |
			DWC3_OEVTEN_HRRINITNOTIFEN |
			DWC3_OEVTEN_ADEVIDLEEN |
			DWC3_OEVTEN_ADEVBHOSTENDEN |
			DWC3_OEVTEN_ADEVHOSTEN |
			DWC3_OEVTEN_ADEVHNPCHNGEN |
			DWC3_OEVTEN_ADEVSRPDETEN |
			DWC3_OEVTEN_ADEVSESSENDDETEN |
			DWC3_OEVTEN_BDEVHOSTENDEN |
			DWC3_OEVTEN_BDEVHNPCHNGEN |
			DWC3_OEVTEN_BDEVSESSVLDDETEN |
			DWC3_OEVTEN_BDEVVBUSCHNGEVNTEN);

	dwc3_writel(dwc->regs, DWC3_OEVTEN, reg);
}

static void dwc3_otg_disable_events(struct dwc3 *dwc)
{
	dwc3_writel(dwc->regs, DWC3_OEVTEN, 0x00);
}

static irqreturn_t dwc3_otg_interrupt(int irq, void *_dwc)
{
	struct dwc3	*dwc = _dwc;
	int		peripheral;
	u32		oevt;

	oevt = dwc3_readl(dwc->regs, DWC3_OEVT);
	dwc3_writel(dwc->regs, DWC3_OEVT, oevt);

	peripheral = oevt & DWC3_OEVT_DEVICEMODE;

	dev_vdbg(dwc->dev, "Switching to %s role\n",
			peripheral ? "Peripheral" : "Host");

	if (peripheral) {
		/**
		 * WORKAROUND: Some host-side registers get mirrored into the
		 * device-side endpoint address space because host and device
		 * share the same memory space in RAM0.
		 *
		 * All known versions of this core suffer from the same
		 * problem, this is why we're not adding any revision check.
		 * Should Synopsys decide to change that behavior, we will add
		 * a revision check to enable/disable this workaround in
		 * runtime.
		 *
		 * Caching those register values here.
		 */
		dwc->dcbaap = dwc3_readl(dwc->regs, DWC3_DEPCMDPAR2(2));
		dwc->erstba = dwc3_readl(dwc->regs, DWC3_DEPCMDPAR2(4));
		dwc->erdp = dwc3_readl(dwc->regs, DWC3_DEPCMDPAR0(4));

		/* switching to peripheral mode using OTG Control Register */
		dwc3_writel(dwc->regs, DWC3_OCTL, DWC3_OCTL_PERIMODE);
	} else {
		/**
		 * WORKAROUND: Some host-side registers get mirrored into the
		 * device-side endpoint address space because host and device
		 * share the same memory space in RAM0.
		 *
		 * All known versions of this core suffer from the same
		 * problem, this is why we're not adding any revision check.
		 * Should Synopsys decide to change that behavior, we will add
		 * a revision check to enable/disable this workaround in
		 * runtime.
		 *
		 * Restoring those register values here.
		 */
		dwc3_writel(dwc->regs, DWC3_DEPCMDPAR2(2), dwc->dcbaap);
		dwc3_writel(dwc->regs, DWC3_DEPCMDPAR2(4), dwc->erstba);
		dwc3_writel(dwc->regs, DWC3_DEPCMDPAR0(4), dwc->erdp);

		/* switching to host role using OTG Control Register */
		dwc3_writel(dwc->regs, DWC3_OCTL, DWC3_OCTL_PRTPWRCTL);
	}

	return IRQ_HANDLED;
}

int dwc3_otg_init(struct dwc3 *dwc)
{
	int irq = dwc->otg_irq;
	int ret;

	if (dwc->otg_irq < 0) {
		dev_dbg(dwc->dev, "missing OTG IRQ, can't switch roles\n");
		return 0;
	}

	ret = devm_request_irq(dwc->dev, irq, dwc3_otg_interrupt, IRQF_SHARED,
			"dwc3-otg", dwc);
	if (ret) {
		dev_err(dwc->dev, "failed to request otg IRQ --> %d\n", ret);
		return ret;
	}

	dwc3_otg_enable_events(dwc);

	return 0;
}

void dwc3_otg_exit(struct dwc3 *dwc)
{
	dwc3_otg_disable_events(dwc);
}
