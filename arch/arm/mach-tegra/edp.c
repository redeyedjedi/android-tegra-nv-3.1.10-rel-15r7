/*
 * arch/arm/mach-tegra/edp.c
 *
 * Copyright (C) 2011 NVIDIA, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
 * 02111-1307, USA
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/seq_file.h>
#include <linux/slab.h>
#include <mach/edp.h>

#include "fuse.h"


static const struct tegra_edp_limits *edp_limits;
static int edp_limits_size;


/*
 * Temperature step size cannot be less than 4C because of hysteresis
 * delta
 * Code assumes different temperatures for the same speedo_id /
 * regulator_cur are adjacent in the table, and higest regulator_cur
 * comes first
 */
static char __initdata tegra_edp_map[] = {
	0x00, 0x2f, 0x2d, 0x82, 0x78, 0x78, 0x78, 0x00,
	0x2f, 0x3c, 0x82, 0x78, 0x78, 0x78, 0x00, 0x2f,
	0x4b, 0x82, 0x78, 0x78, 0x78, 0x00, 0x2f, 0x5a,
	0x82, 0x78, 0x78, 0x78, 0x00, 0x28, 0x2d, 0x82,
	0x78, 0x78, 0x78, 0x00, 0x28, 0x3c, 0x82, 0x78,
	0x78, 0x78, 0x00, 0x28, 0x4b, 0x82, 0x78, 0x78,
	0x73, 0x00, 0x28, 0x5a, 0x82, 0x78, 0x73, 0x69,
	0x00, 0x23, 0x2d, 0x82, 0x78, 0x78, 0x78, 0x00,
	0x23, 0x3c, 0x82, 0x78, 0x78, 0x6e, 0x00, 0x23,
	0x4b, 0x82, 0x78, 0x78, 0x64, 0x00, 0x23, 0x5a,
	0x82, 0x78, 0x6e, 0x5a, 0x00, 0x1e, 0x2d, 0x82,
	0x78, 0x78, 0x69, 0x00, 0x1e, 0x3c, 0x82, 0x78,
	0x78, 0x64, 0x00, 0x1e, 0x4b, 0x82, 0x78, 0x6e,
	0x5a, 0x00, 0x1e, 0x5a, 0x82, 0x78, 0x64, 0x5a,
	0x00, 0x19, 0x2d, 0x82, 0x78, 0x6e, 0x5a, 0x00,
	0x19, 0x3c, 0x82, 0x78, 0x69, 0x55, 0x00, 0x19,
	0x4b, 0x82, 0x78, 0x5f, 0x4b, 0x00, 0x19, 0x5a,
	0x82, 0x73, 0x5a, 0x3c, 0x01, 0x2f, 0x2d, 0x82,
	0x78, 0x78, 0x78, 0x01, 0x2f, 0x3c, 0x82, 0x78,
	0x78, 0x78, 0x01, 0x2f, 0x4b, 0x82, 0x78, 0x78,
	0x78, 0x01, 0x2f, 0x5a, 0x82, 0x78, 0x78, 0x78,
	0x01, 0x28, 0x2d, 0x82, 0x78, 0x78, 0x78, 0x01,
	0x28, 0x3c, 0x82, 0x78, 0x78, 0x78, 0x01, 0x28,
	0x4b, 0x82, 0x78, 0x78, 0x73, 0x01, 0x28, 0x5a,
	0x82, 0x78, 0x73, 0x69, 0x01, 0x23, 0x2d, 0x82,
	0x78, 0x78, 0x78, 0x01, 0x23, 0x3c, 0x82, 0x78,
	0x78, 0x6e, 0x01, 0x23, 0x4b, 0x82, 0x78, 0x78,
	0x64, 0x01, 0x23, 0x5a, 0x82, 0x78, 0x6e, 0x5a,
	0x01, 0x1e, 0x2d, 0x82, 0x78, 0x78, 0x69, 0x01,
	0x1e, 0x3c, 0x82, 0x78, 0x78, 0x64, 0x01, 0x1e,
	0x4b, 0x82, 0x78, 0x6e, 0x5a, 0x01, 0x1e, 0x5a,
	0x82, 0x78, 0x64, 0x5a, 0x01, 0x19, 0x2d, 0x82,
	0x78, 0x6e, 0x5a, 0x01, 0x19, 0x3c, 0x82, 0x78,
	0x69, 0x55, 0x01, 0x19, 0x4b, 0x82, 0x78, 0x5f,
	0x4b, 0x01, 0x19, 0x5a, 0x82, 0x73, 0x5a, 0x3c,
	0x02, 0x3d, 0x2d, 0x8c, 0x82, 0x82, 0x82, 0x02,
	0x3d, 0x3c, 0x8c, 0x82, 0x82, 0x82, 0x02, 0x3d,
	0x4b, 0x8c, 0x82, 0x82, 0x82, 0x02, 0x3d, 0x5a,
	0x8c, 0x82, 0x82, 0x82, 0x02, 0x32, 0x2d, 0x8c,
	0x82, 0x82, 0x82, 0x02, 0x32, 0x3c, 0x8c, 0x82,
	0x82, 0x82, 0x02, 0x32, 0x4b, 0x8c, 0x82, 0x82,
	0x78, 0x02, 0x32, 0x5a, 0x8c, 0x82, 0x82, 0x6e,
	0x02, 0x28, 0x2d, 0x8c, 0x82, 0x82, 0x78, 0x02,
	0x28, 0x3c, 0x8c, 0x82, 0x82, 0x73, 0x02, 0x28,
	0x4b, 0x8c, 0x82, 0x78, 0x6e, 0x02, 0x28, 0x5a,
	0x8c, 0x82, 0x73, 0x5f, 0x02, 0x23, 0x2d, 0x8c,
	0x82, 0x82, 0x6e, 0x02, 0x23, 0x3c, 0x8c, 0x82,
	0x78, 0x69, 0x02, 0x23, 0x4b, 0x8c, 0x82, 0x73,
	0x5f, 0x02, 0x23, 0x5a, 0x8c, 0x82, 0x69, 0x55,
	0x03, 0x3d, 0x2d, 0x8c, 0x82, 0x82, 0x82, 0x03,
	0x3d, 0x3c, 0x8c, 0x82, 0x82, 0x82, 0x03, 0x3d,
	0x4b, 0x8c, 0x82, 0x82, 0x82, 0x03, 0x3d, 0x5a,
	0x8c, 0x82, 0x82, 0x82, 0x03, 0x32, 0x2d, 0x8c,
	0x82, 0x82, 0x82, 0x03, 0x32, 0x3c, 0x8c, 0x82,
	0x82, 0x82, 0x03, 0x32, 0x4b, 0x8c, 0x82, 0x82,
	0x73, 0x03, 0x32, 0x5a, 0x8c, 0x82, 0x82, 0x6e,
	0x03, 0x28, 0x2d, 0x8c, 0x82, 0x82, 0x78, 0x03,
	0x28, 0x3c, 0x8c, 0x82, 0x82, 0x73, 0x03, 0x28,
	0x4b, 0x8c, 0x82, 0x7d, 0x6e, 0x03, 0x28, 0x5a,
	0x8c, 0x82, 0x73, 0x5f, 0x03, 0x23, 0x2d, 0x8c,
	0x82, 0x82, 0x6e, 0x03, 0x23, 0x3c, 0x8c, 0x82,
	0x78, 0x6e, 0x03, 0x23, 0x4b, 0x8c, 0x82, 0x78,
	0x5f, 0x03, 0x23, 0x5a, 0x8c, 0x82, 0x6e, 0x5a,
};


/*
 * "Safe entry" to be used when no match for speedo_id /
 * regulator_cur is found; must be the last one
 */
static struct tegra_edp_limits edp_default_limits[] = {
	{90, {1000000, 1000000, 1000000, 1000000} },
};



/*
 * Specify regulator current in mA, e.g. 5000mA
 * Use 0 for default
 */
void __init tegra_init_cpu_edp_limits(unsigned int regulator_mA)
{
	int cpu_speedo_id = tegra_cpu_speedo_id();
	int i, j;
	struct tegra_edp_limits *e;
	struct tegra_edp_entry *t = (struct tegra_edp_entry *)tegra_edp_map;
	int tsize = sizeof(tegra_edp_map)/sizeof(struct tegra_edp_entry);

	if (!regulator_mA) {
		edp_limits = edp_default_limits;
		edp_limits_size = ARRAY_SIZE(edp_default_limits);
		return;
	}

	for (i = 0; i < tsize; i++) {
		if (t[i].speedo_id == cpu_speedo_id &&
		    t[i].regulator_100mA <= regulator_mA / 100)
			break;
	}

	/* No entry found in tegra_edp_map */
	if (i >= tsize) {
		edp_limits = edp_default_limits;
		edp_limits_size = ARRAY_SIZE(edp_default_limits);
		return;
	}

	/* Find all rows for this entry */
	for (j = i + 1; j < tsize; j++) {
		if (t[i].speedo_id != t[j].speedo_id ||
		    t[i].regulator_100mA != t[j].regulator_100mA)
			break;
	}

	edp_limits_size = j - i;
	e = kmalloc(sizeof(struct tegra_edp_limits) * edp_limits_size,
		    GFP_KERNEL);
	BUG_ON(!e);

	for (j = 0; j < edp_limits_size; j++) {
		e[j].temperature = (int)t[i+j].temperature;
		e[j].freq_limits[0] = (unsigned int)t[i+j].freq_limits[0] * 10000;
		e[j].freq_limits[1] = (unsigned int)t[i+j].freq_limits[1] * 10000;
		e[j].freq_limits[2] = (unsigned int)t[i+j].freq_limits[2] * 10000;
		e[j].freq_limits[3] = (unsigned int)t[i+j].freq_limits[3] * 10000;
	}

	if (edp_limits && edp_limits != edp_default_limits)
		kfree(edp_limits);

	edp_limits = e;
}

void tegra_get_cpu_edp_limits(const struct tegra_edp_limits **limits, int *size)
{
	*limits = edp_limits;
	*size = edp_limits_size;
}

#ifdef CONFIG_DEBUG_FS

static int edp_debugfs_show(struct seq_file *s, void *data)
{
	int i;
	const struct tegra_edp_limits *limits;
	int size;

	tegra_get_cpu_edp_limits(&limits, &size);

	seq_printf(s, "-- EDP table --\n");
	for (i = 0; i < size; i++) {
		seq_printf(s, "%4dC: %10d %10d %10d %10d\n",
			   limits[i].temperature,
			   limits[i].freq_limits[0],
			   limits[i].freq_limits[1],
			   limits[i].freq_limits[2],
			   limits[i].freq_limits[3]);
	}

	return 0;
}


static int edp_debugfs_open(struct inode *inode, struct file *file)
{
	return single_open(file, edp_debugfs_show, inode->i_private);
}


static const struct file_operations edp_debugfs_fops = {
	.open		= edp_debugfs_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};


static int __init tegra_edp_debugfs_init()
{
	struct dentry *d;

	d = debugfs_create_file("edp", S_IRUGO, NULL, NULL,
				&edp_debugfs_fops);
	if (!d)
		return -ENOMEM;

	return 0;
}

late_initcall(tegra_edp_debugfs_init);
#endif /* CONFIG_DEBUG_FS */