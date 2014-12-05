/*
 * omap_hwmod debugfs view
 *
 * Copyright (C) 2014 Texas Instruments, Incorporated - http://www.ti.com
 * Author: Felipe Balbi <balbi@ti.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#undef DEBUG

#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/io.h>
#include <linux/err.h>
#include <linux/fs.h>
#include <linux/list.h>
#include <linux/mutex.h>
#include <linux/spinlock.h>
#include <linux/debugfs.h>
#include <linux/seq_file.h>

#include "clock.h"
#include "omap_hwmod.h"

#include "soc.h"
#include "common.h"
#include "clockdomain.h"
#include "powerdomain.h"
#include "cm2xxx.h"
#include "cm3xxx.h"
#include "cm33xx.h"
#include "prm.h"
#include "prm3xxx.h"
#include "prm44xx.h"
#include "prm33xx.h"
#include "prminst44xx.h"
#include "mux.h"
#include "pm.h"

#if IS_ENABLED(CONFIG_DEBUG_FS)
static struct dentry *omap_hwmod_debugfs_root;

static int __init omap_hwmod_create_files(struct omap_hwmod *oh,
		struct dentry *dir)
{
	struct dentry *file;

	file = debugfs_create_u16("flags", S_IRUGO, dir, &oh->flags);
	if (!file)
		return -ENOMEM;

	file = debugfs_create_u8("mpu_rt_idx", S_IRUGO, dir, &oh->mpu_rt_idx);
	if (!file)
		return -ENOMEM;

	file = debugfs_create_u8("response_lat", S_IRUGO, dir,
			&oh->response_lat);
	if (!file)
		return -ENOMEM;

	file = debugfs_create_u8("rst_lines_cnt", S_IRUGO, dir,
			&oh->rst_lines_cnt);
	if (!file)
		return -ENOMEM;

	file = debugfs_create_u8("opt_clks_cnt", S_IRUGO, dir,
			&oh->opt_clks_cnt);
	if (!file)
		return -ENOMEM;

	file = debugfs_create_u8("masters_cnt", S_IRUGO, dir, &oh->masters_cnt);
	if (!file)
		return -ENOMEM;

	file = debugfs_create_u8("slaves_cnt", S_IRUGO, dir, &oh->slaves_cnt);
	if (!file)
		return -ENOMEM;

	file = debugfs_create_u8("hwmods_cnt", S_IRUGO, dir, &oh->hwmods_cnt);
	if (!file)
		return -ENOMEM;

	file = debugfs_create_u8("_int_flags", S_IRUGO, dir, &oh->_int_flags);
	if (!file)
		return -ENOMEM;

	file = debugfs_create_u8("_state", S_IRUGO, dir, &oh->_state);
	if (!file)
		return -ENOMEM;

	file = debugfs_create_u8("_postsetup_state", S_IRUGO, dir,
			&oh->_postsetup_state);
	if (!file)
		return -ENOMEM;

	return 0;
}

static int __init
omap_hwmod_create_sysc_files(struct omap_hwmod_class_sysconfig *sysc,
		struct dentry *dir)
{
	struct dentry *subdir;
	struct dentry *file;

	if (!sysc)
		return 0;

	subdir = debugfs_create_dir("sysc", dir);
	if (!subdir)
		return -ENOMEM;

	file = debugfs_create_x32("rev_offs", S_IRUGO, subdir,
			&sysc->rev_offs);
	if (!file)
		return -ENOMEM;

	file = debugfs_create_x32("sysc_offs", S_IRUGO, subdir,
			&sysc->sysc_offs);
	if (!file)
		return -ENOMEM;

	file = debugfs_create_x32("syss_offs", S_IRUGO, subdir,
			&sysc->syss_offs);
	if (!file)
		return -ENOMEM;

	file = debugfs_create_x16("sysc_flags", S_IRUGO, subdir,
			&sysc->sysc_flags);
	if (!file)
		return -ENOMEM;

	file = debugfs_create_u8("srst_udelay", S_IRUGO, subdir,
			&sysc->srst_udelay);
	if (!file)
		return -ENOMEM;

	file = debugfs_create_x8("idlemodes", S_IRUGO, subdir,
			&sysc->idlemodes);
	if (!file)
		return -ENOMEM;

	file = debugfs_create_u8("clockact", S_IRUGO, subdir, &sysc->clockact);
	if (!file)
		return -ENOMEM;

	return 0;
}

static int __init omap_hwmod_create_class_files(struct omap_hwmod_class *class,
		struct dentry *dir)
{
	struct dentry *subdir;
	struct dentry *file;
	int ret;

	if (!class)
		return 0;

	subdir = debugfs_create_dir("class", dir);
	if (!subdir)
		return -ENOMEM;

	file = debugfs_create_u32("rev", S_IRUGO, subdir, &class->rev);
	if (!file)
		return -ENOMEM;

	ret = omap_hwmod_create_sysc_files(class->sysc, subdir);
	if (ret)
		return -ENOMEM;

	return 0;
}

static int __init omap_hwmod_create_irq_files(struct omap_hwmod_irq_info *irqs,
		struct dentry *dir)
{
	struct dentry *subdir;
	int i = 0;

	if (!irqs)
		return 0;

	subdir = debugfs_create_dir("irqs", dir);
	if (!subdir)
		return -ENOMEM;

	while (irqs[i].irq != -1) {
		struct dentry *file;

		file = debugfs_create_u16(irqs[i].name, S_IRUGO, subdir,
				&irqs[i].irq);
		if (!file)
			return -ENOMEM;
		i++;
	}

	return 0;
}

static int __init omap_hwmod_create_dma_files(struct omap_hwmod_dma_info *dmas,
		struct dentry *dir)
{
	struct dentry *subdir;
	int i = 0;

	if (!dmas)
		return 0;

	subdir = debugfs_create_dir("dmas", dir);
	if (!subdir)
		return -ENOMEM;

	while (dmas[i].dma_req != -1) {
		struct dentry *file;

		file = debugfs_create_u16(dmas[i].name, S_IRUGO, subdir,
				&dmas[i].dma_req);
		if (!file)
			return -ENOMEM;
		i++;
	}

	return 0;
}

int omap_hwmod_register_debugfs(struct omap_hwmod *oh)
{
	struct dentry *root;
	struct dentry *dir;
	int ret;

	if (!omap_hwmod_debugfs_root) {
		root = debugfs_create_dir("omap_hwmod", NULL);
		if (!root ) {
			pr_debug("omap_hwmod: unable to initialize debugfs\n");
			return 0;
		}

		omap_hwmod_debugfs_root = root;
	} else {
		root = omap_hwmod_debugfs_root;
	}

	dir = debugfs_create_dir(oh->name, root);
	if (!dir)
		return -ENOMEM;

	ret = omap_hwmod_create_files(oh, dir);
	if (ret)
		return ret;

	ret = omap_hwmod_create_class_files(oh->class, dir);
	if (ret)
		return ret;

	ret = omap_hwmod_create_irq_files(oh->mpu_irqs, dir);
	if (ret)
		return ret;

	ret = omap_hwmod_create_dma_files(oh->sdma_reqs, dir);
	if (ret)
		return ret;

	return 0;
}
#endif
