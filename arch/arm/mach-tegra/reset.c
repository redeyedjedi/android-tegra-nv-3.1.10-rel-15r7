/*
 * arch/arm/mach-tegra/reset.c
 *
 * Copyright (C) 2011 NVIDIA Corporation.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/init.h>
#include <linux/io.h>
#include <linux/cpumask.h>
#include <linux/bitops.h>

#include <asm/cacheflush.h>
#include <asm/hardware/cache-l2x0.h>

#include <mach/iomap.h>

#include "reset.h"
#include "sleep.h"

static bool is_enabled;

void tegra_cpu_reset_handler_enable(void)
{
	void __tegra_cpu_reset_handler(void);
	void __tegra_cpu_reset_handler_start(void);
	void __tegra_cpu_reset_handler_end(void);
	void __iomem *evp_cpu_reset =
		IO_ADDRESS(TEGRA_EXCEPTION_VECTORS_BASE + 0x100);
	void __iomem *iram_base = IO_ADDRESS(TEGRA_IRAM_BASE);
	void __iomem *sb_ctrl = IO_ADDRESS(TEGRA_SB_BASE);
	unsigned long cpu_reset_handler_size =
		__tegra_cpu_reset_handler_end - __tegra_cpu_reset_handler_start;
	unsigned long cpu_reset_handler_offset =
		__tegra_cpu_reset_handler - __tegra_cpu_reset_handler_start;
	unsigned long reg;

	BUG_ON(is_enabled);
	BUG_ON(cpu_reset_handler_size > TEGRA_RESET_HANDLER_SIZE);

	memcpy(iram_base, (void *)__tegra_cpu_reset_handler_start,
	       cpu_reset_handler_size);

	/* NOTE: This must be the one and only write to the EVP CPU reset
		 vector in the entire system. */
	writel(TEGRA_RESET_HANDLER_BASE + cpu_reset_handler_offset,
		evp_cpu_reset);
	wmb();
	reg = readl(evp_cpu_reset);

	/* Prevent further modifications to the physical reset vector.
	   NOTE: Has no effect on chips prior to Tegra3. */
	reg = readl(sb_ctrl);
	reg |= 2;
	writel(reg, sb_ctrl);
	is_enabled = true;
	wmb();
}

#ifdef CONFIG_PM_SLEEP
static unsigned long cpu_reset_handler_save[TEGRA_RESET_DATA_SIZE];

void tegra_cpu_reset_handler_save(void)
{
	unsigned int i;
	BUG_ON(!is_enabled);
	for (i = 0; i < TEGRA_RESET_DATA_SIZE; i++)
		cpu_reset_handler_save[i] =
					tegra_cpu_reset_handler_ptr[i];
	is_enabled = false;
}

void tegra_cpu_reset_handler_restore(void)
{
	unsigned int i;
	BUG_ON(is_enabled);
	for (i = 0; i < TEGRA_RESET_DATA_SIZE; i++)
		tegra_cpu_reset_handler_ptr[i] =
					cpu_reset_handler_save[i];
	is_enabled = true;
}
#endif

void __init tegra_cpu_reset_handler_init(void)
{
#ifdef CONFIG_SMP
	__tegra_cpu_reset_handler_data[TEGRA_RESET_MASK_PRESENT] =
		*((u32 *)cpu_present_mask);
	__tegra_cpu_reset_handler_data[TEGRA_RESET_STARTUP_SECONDARY] =
		virt_to_phys((void *)tegra_secondary_startup);
#endif
#ifdef CONFIG_PM_SLEEP
	__tegra_cpu_reset_handler_data[TEGRA_RESET_STARTUP_LP1] =
		TEGRA_IRAM_CODE_AREA;
	__tegra_cpu_reset_handler_data[TEGRA_RESET_STARTUP_LP2] =
		virt_to_phys((void *)tegra_resume);
#endif

	/* Push all of reset handler data out to the L3 memory system. */
	__cpuc_coherent_kern_range(
		(unsigned long)&__tegra_cpu_reset_handler_data[0],
		(unsigned long)&__tegra_cpu_reset_handler_data[TEGRA_RESET_DATA_SIZE]);

	outer_clean_range(__pa(&__tegra_cpu_reset_handler_data[0]),
			  __pa(&__tegra_cpu_reset_handler_data[TEGRA_RESET_DATA_SIZE]));

	tegra_cpu_reset_handler_enable();
}